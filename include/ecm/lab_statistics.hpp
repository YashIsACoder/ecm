#pragma once

#include <array>
#include <cstdint>

namespace ecm::lab {

struct Interval95 {
    double estimate;
    double low;
    double high;
};

// Wilson score interval for one binary outcome rate.
[[nodiscard]] Interval95 wilson95(std::uint64_t successes, std::uint64_t trials);

// Stream matched binary outcomes in O(1) space. Add one pair per common weather draw.
class PairedOutcomes {
public:
    void add(bool comparison, bool reference) noexcept;
    // Comparison rate minus reference rate. Uses Newcombe's method 10.
    [[nodiscard]] Interval95 difference95() const;

private:
    std::array<std::uint64_t, 4> counts_{};
};

}  // namespace ecm::lab
