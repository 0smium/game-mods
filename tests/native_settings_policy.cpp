#include "../mods/smtvv/common/settings_policy.hpp"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
    using namespace CameraTuning;
    const auto d = Decode(Defaults);
    assert(d.eye == 40 && d.fov == 110 && d.gait == 5);
    for (unsigned field = 0; field < 3; ++field) {
        for (bool coarse : {false, true}) {
            const auto increased = Adjust(Defaults, field, false, coarse);
            assert(Adjust(increased, field, true, coarse) == Defaults);
        }
    }
    assert(Decode(Adjust(Defaults, 0, false, false)).eye == 41);
    assert(Decode(Adjust(Defaults, 1, true, true)).fov == 105);
    assert(Decode(Adjust(Defaults, 2, true, false)).gait == 4.75f);
    const auto low = Encode(-1e30f, -1e30f, -1e30f), high = Encode(1e30f, 1e30f, 1e30f);
    for (unsigned i = 0; i < 3; ++i) {
        assert(Adjust(low, i, true, true) == low);
        assert(Adjust(high, i, false, true) == high);
    }
    assert(Encode(NAN, INFINITY, -INFINITY) == Defaults);
    for (unsigned e = 0; e <= 120; ++e)
        for (unsigned f = 0; f <= 80; ++f)
            for (unsigned g = 0; g <= 40; ++g) {
                const auto packed = e | (f << 7) | (g << 14), value = packed;
                const auto v = Decode(value);
                assert(Encode(v.eye, v.fov, v.gait) == packed);
            }
    assert(Chord(0, false, false, false) == Action::Toggle);
    assert(Chord(0, false, true, false) == Action::Reset);
    assert(Chord(0, true, true, false) == Action::None);
    assert(Chord(1, true, true, false) == Action::Eye);
    assert(Chord(2, false, false, true) == Action::None);
    assert(Chord(4, false, false, false) == Action::None);
    std::cout << "PASS tuning defaults, fine/coarse steps, clamping, nonfinite input, 401841 roundtrips and key chords\n";
}
