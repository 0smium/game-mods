// Reflected property readers shared by the scoped visibility fixes.
namespace GardenDiagnostics {
struct Property {
    int offset{-1};
    int size{};
    unsigned byteOffset{};
    unsigned mask{};
    std::string kind;
};
struct PropertyCache {
    ObjectIdentity type;
    std::unordered_map<std::string, Property> values;
};
std::vector<PropertyCache> propertyCache;

Property FindProperty(SDK::UObject* object, const char* name) {
    if (!IndexedObject(object)) return {};
    PropertyCache* cache{};
    for (auto& item : propertyCache) if (Matches(item.type, object->Class)) { cache = &item; break; }
    if (!cache && propertyCache.size() < 64) {
        propertyCache.push_back({Identify(object->Class), {}});
        cache = &propertyCache.back();
    }
    if (cache) {
        const auto found = cache->values.find(name);
        if (found != cache->values.end()) return found->second;
    }
    Property result;
    auto type = reinterpret_cast<SDK::UStruct*>(object->Class);
    for (unsigned depth = 0; type && depth < 24; ++depth) {
        if (!IndexedObject(type) || !Readable(type, sizeof(SDK::UStruct))) break;
        auto field = type->ChildProperties;
        for (unsigned n = 0; field && n < 512; ++n) {
            if (!Readable(field, sizeof(SDK::FProperty)) ||
                !Readable(field->ClassPrivate, sizeof(SDK::FFieldClass))) break;
            if (field->Name.ToString() == name) {
                const auto prop = reinterpret_cast<SDK::FProperty*>(field);
                if (prop->ArrayDim != 1 || prop->Offset < 0 || prop->Offset > 0x10000 ||
                    prop->ElementSize < 1 || prop->ElementSize > 0x1000) break;
                result = {prop->Offset, prop->ElementSize, 0, 0, field->ClassPrivate->Name.ToString()};
                if (result.kind == "BoolProperty") {
                    const auto boolean = reinterpret_cast<SDK::FBoolProperty*>(field);
                    if (!Readable(boolean, sizeof(*boolean)) || boolean->ByteOffset > 7) { result = {}; break; }
                    result.byteOffset = boolean->ByteOffset;
                    result.mask = boolean->FieldMask;
                }
                break;
            }
            field = field->Next;
        }
        if (result.offset >= 0) break;
        type = type->Super;
    }
    if (cache && cache->values.size() < 96) cache->values.emplace(name, result);
    return result;
}

SDK::UObject* ObjectProperty(SDK::UObject* object, const char* name) {
    const auto property = FindProperty(object, name);
    if (property.offset < 0 || property.kind != "ObjectProperty" || property.size != sizeof(void*)) return nullptr;
    const auto address = reinterpret_cast<const std::uint8_t*>(object) + property.offset;
    if (!Readable(address, sizeof(void*))) return nullptr;
    SDK::UObject* result{};
    std::memcpy(&result, address, sizeof(result));
    return IndexedObject(result) ? result : nullptr;
}

bool Scalar(SDK::UObject* object, const char* name, double& value) {
    const auto property = FindProperty(object, name);
    if (property.offset < 0) return false;
    const auto address = reinterpret_cast<const std::uint8_t*>(object) + property.offset;
    if (property.kind == "BoolProperty" && property.mask && Readable(address + property.byteOffset, 1)) {
        value = (address[property.byteOffset] & property.mask) != 0;
        return true;
    }
    if (property.size != 4 || !Readable(address, 4)) return false;
    if (property.kind == "FloatProperty") { float number{}; std::memcpy(&number, address, 4); value = number; return std::isfinite(value); }
    if (property.kind == "IntProperty") { int number{}; std::memcpy(&number, address, 4); value = number; return true; }
    return false;
}

}
