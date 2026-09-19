#include <algorithm>
#include <functional>
#include <limits>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "safetyhook/os.hpp"
#include "safetyhook/utility.hpp"

#include "safetyhook/utility.hpp"

#include "safetyhook/allocator.hpp"

namespace safetyhook {
thread_local unsigned long p5_virtual_alloc2_error = 0;
thread_local unsigned long p5_virtual_alloc2_calls = 0;
Allocation::Allocation(Allocation&& other) noexcept {
    *this = std::move(other);
}

Allocation& Allocation::operator=(Allocation&& other) noexcept {
    if (this != &other) {
        free();

        m_allocator = std::move(other.m_allocator);
        m_address = other.m_address;
        m_size = other.m_size;

        other.m_address = nullptr;
        other.m_size = 0;
    }

    return *this;
}

Allocation::~Allocation() {
    free();
}

void Allocation::free() {
    if (m_allocator && m_address != nullptr && m_size != 0) {
        m_allocator->free(m_address, m_size);
        m_address = nullptr;
        m_size = 0;
        m_allocator.reset();
    }
}

Allocation::Allocation(std::shared_ptr<Allocator> allocator, uint8_t* address, size_t size) noexcept
    : m_allocator{std::move(allocator)}, m_address{address}, m_size{size} {
}

std::shared_ptr<Allocator> Allocator::global() {
    static std::weak_ptr<Allocator> global_allocator{};
    static std::mutex global_allocator_mutex{};

    std::scoped_lock lock{global_allocator_mutex};

    if (auto allocator = global_allocator.lock()) {
        return allocator;
    }

    auto allocator = Allocator::create();

    global_allocator = allocator;

    return allocator;
}

std::shared_ptr<Allocator> Allocator::create() {
    return std::shared_ptr<Allocator>{new Allocator{}};
}

std::expected<Allocation, Allocator::Error> Allocator::allocate(size_t size) {
    return allocate_near({}, size, std::numeric_limits<size_t>::max());
}

std::expected<Allocation, Allocator::Error> Allocator::allocate_near(
    const std::vector<uint8_t*>& desired_addresses, size_t size, size_t max_distance) {
    std::scoped_lock lock{m_mutex};
    return internal_allocate_near(desired_addresses, size, max_distance);
}

void Allocator::free(uint8_t* address, size_t size) {
    std::scoped_lock lock{m_mutex};
    return internal_free(address, size);
}

std::expected<Allocation, Allocator::Error> Allocator::internal_allocate_near(
    const std::vector<uint8_t*>& desired_addresses, size_t size, size_t max_distance) {
    // Align to 2 bytes to pass MFP virtual method check
    // See https://itanium-cxx-abi.github.io/cxx-abi/abi.html#member-function-pointers
    size_t aligned_size = align_up(size, 2);

    // First search through our list of allocations for a free block that is large
    // enough.
    for (const auto& allocation : m_memory) {
        if (allocation->size < aligned_size) {
            continue;
        }

        for (auto node = allocation->freelist.get(); node != nullptr; node = node->next.get()) {
            // Enough room?
            if (static_cast<size_t>(node->end - node->start) < aligned_size) {
                continue;
            }

            const auto address = node->start;

            // Close enough?
            if (!in_range(address, desired_addresses, max_distance)) {
                continue;
            }

            node->start += aligned_size;

            return Allocation{shared_from_this(), address, size};
        }
    }

    // If we didn't find a free block, we need to allocate a new one.
    // A small trampoline needs a page, not an entire 64 KiB allocation unit.
    // Windows still aligns its base to allocation granularity; the committed
    // extent can be one page, which fits fragmented startup address spaces.
    auto allocation_size = align_up(aligned_size, system_info().page_size);
    auto allocation_address = allocate_nearby_memory(desired_addresses, allocation_size, max_distance);

    if (!allocation_address) {
        return std::unexpected{allocation_address.error()};
    }

    auto& allocation = m_memory.emplace_back(new Memory);

    allocation->address = *allocation_address;
    allocation->size = allocation_size;
    allocation->freelist = std::make_unique<FreeNode>();
    allocation->freelist->start = *allocation_address + aligned_size;
    allocation->freelist->end = *allocation_address + allocation_size;

    return Allocation{shared_from_this(), *allocation_address, size};
}

void Allocator::internal_free(uint8_t* address, size_t size) {
    // See internal_allocate_near
    size = align_up(size, 2);

    for (const auto& allocation : m_memory) {
        if (allocation->address > address || allocation->address + allocation->size < address) {
            continue;
        }

        // Find the right place for our new freenode.
        FreeNode* prev{};

        for (auto node = allocation->freelist.get(); node != nullptr; prev = node, node = node->next.get()) {
            if (node->start > address) {
                break;
            }
        }

        // Add new freenode.
        auto free_node = std::make_unique<FreeNode>();

        free_node->start = address;
        free_node->end = address + size;

        if (prev == nullptr) {
            free_node->next.swap(allocation->freelist);
            allocation->freelist.swap(free_node);
        } else {
            free_node->next.swap(prev->next);
            prev->next.swap(free_node);
        }

        combine_adjacent_freenodes(*allocation);
        break;
    }
}

void Allocator::combine_adjacent_freenodes(Memory& memory) {
    for (auto prev = memory.freelist.get(), node = prev; node != nullptr; node = node->next.get()) {
        if (prev->end == node->start) {
            prev->end = node->end;
            prev->next.swap(node->next);
            node->next.reset();
            node = prev;
        } else {
            prev = node;
        }
    }
}

std::expected<uint8_t*, Allocator::Error> Allocator::allocate_nearby_memory(
    const std::vector<uint8_t*>& desired_addresses, size_t size, size_t max_distance) {
    if (desired_addresses.empty()) {
        if (auto result = vm_allocate(nullptr, size, VM_ACCESS_RWX)) {
            return result.value();
        }

        return std::unexpected{Error::BAD_VIRTUAL_ALLOC};
    }

    // P5R-local adaptation (SafetyHook, Boost Software License 1.0).
    // Reloaded/CoreCLR fragment the executable's near address space. Ask Windows
    // to choose an aligned free region atomically instead of relying exclusively
    // on a VirtualQuery/VirtualAlloc walk. Constrain the ENTIRE allocation to the
    // intersection of all original RIP-relative target ranges.
    using Allocate2 = PVOID(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
    static const auto allocate2 = reinterpret_cast<Allocate2>(
        GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "VirtualAlloc2"));
    if (allocate2) {
        const auto sys = system_info();
        auto low = reinterpret_cast<uintptr_t>(sys.min_address);
        auto high = reinterpret_cast<uintptr_t>(sys.max_address);
        for (auto desired : desired_addresses) {
            const auto center = reinterpret_cast<uintptr_t>(desired);
            low = std::max(low, center > max_distance ? center - max_distance : uintptr_t{0});
            high = std::min(high, max_distance > UINTPTR_MAX - center ? UINTPTR_MAX : center + max_distance);
        }
        low = align_up(low, static_cast<uintptr_t>(sys.allocation_granularity));
        // HighestEndingAddress is inclusive and Windows requires granularity-1.
        high = align_down(high + 1, static_cast<uintptr_t>(sys.allocation_granularity)) - 1;
        if (high >= low && size - 1 <= high - low) {
            MEM_ADDRESS_REQUIREMENTS requirements{};
            requirements.LowestStartingAddress = reinterpret_cast<PVOID>(low);
            requirements.HighestEndingAddress = reinterpret_cast<PVOID>(high);
            requirements.Alignment = sys.allocation_granularity;
            MEM_EXTENDED_PARAMETER parameter{};
            parameter.Type = MemExtendedParameterAddressRequirements;
            parameter.Pointer = &requirements;
            ++p5_virtual_alloc2_calls;
            p5_virtual_alloc2_error = 0;
            if (auto memory = allocate2(GetCurrentProcess(), nullptr, size,
                    MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE, &parameter, 1)) {
                return static_cast<uint8_t*>(memory);
            }
            p5_virtual_alloc2_error = GetLastError();
        }
    }

    auto attempt_allocation = [&](uint8_t* p) -> uint8_t* {
        if (!in_range(p, desired_addresses, max_distance)) {
            return nullptr;
        }

        if (auto result = vm_allocate(p, size, VM_ACCESS_RWX)) {
            return result.value();
        }

        return nullptr;
    };

    auto si = system_info();
    auto desired_address = desired_addresses[0];
    auto search_start = si.min_address;
    auto search_end = si.max_address;

    if (static_cast<size_t>(desired_address - search_start) > max_distance) {
        search_start = desired_address - max_distance;
    }

    if (static_cast<size_t>(search_end - desired_address) > max_distance) {
        search_end = desired_address + max_distance;
    }

    search_start = std::max(search_start, si.min_address);
    search_end = std::min(search_end, si.max_address);
    desired_address = align_up(desired_address, si.allocation_granularity);
    VmBasicInfo mbi{};

    // Search backwards from the desired_address.
    for (auto p = desired_address; p > search_start && in_range(p, desired_addresses, max_distance);
         p = align_down(mbi.address - 1, si.allocation_granularity)) {
        auto result = vm_query(p);

        if (!result) {
            break;
        }

        mbi = result.value();

        if (!mbi.is_free) {
            continue;
        }

        if (auto allocation_address = attempt_allocation(p); allocation_address != nullptr) {
            return allocation_address;
        }
    }

    // Search forwards from the desired_address.
    for (auto p = desired_address; p < search_end && in_range(p, desired_addresses, max_distance); p += mbi.size) {
        auto result = vm_query(p);

        if (!result) {
            break;
        }

        mbi = result.value();

        if (!mbi.is_free) {
            continue;
        }

        if (auto allocation_address = attempt_allocation(p); allocation_address != nullptr) {
            return allocation_address;
        }
    }

    return std::unexpected{Error::NO_MEMORY_IN_RANGE};
}

bool Allocator::in_range(uint8_t* address, const std::vector<uint8_t*>& desired_addresses, size_t max_distance) {
    return std::all_of(desired_addresses.begin(), desired_addresses.end(), [&](const auto& desired_address) {
        const size_t delta = (address > desired_address) ? address - desired_address : desired_address - address;
        return delta <= max_distance;
    });
}

Allocator::Memory::~Memory() {
    vm_free(address);
}
} // namespace safetyhook
