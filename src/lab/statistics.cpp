#include "ecm/lab_statistics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ecm::lab {
namespace {

Interval95 proportion_interval(long double successes, long double trials) {
    constexpr long double z = 1.959963984540054L;
    constexpr long double z2 = z * z;
    const long double share = successes / trials;
    const long double denominator = 1 + z2 / trials;
    const long double centre = share + z2 / (2 * trials);
    const long double margin = z * std::sqrt((share * (1 - share) + z2 / (4 * trials)) / trials);
    return {static_cast<double>(share),
            successes == 0 ? 0.0 : static_cast<double>(std::max(0.0L, (centre - margin) / denominator)),
            successes == trials ? 1.0 : static_cast<double>(std::min(1.0L, (centre + margin) / denominator))};
}

}  // namespace

Interval95 wilson95(std::uint64_t successes, std::uint64_t trials) {
    if (trials == 0 || successes > trials) {
        throw std::invalid_argument("Wilson interval requires a positive trial count and valid successes");
    }
    return proportion_interval(successes, trials);
}

void PairedOutcomes::add(bool comparison, bool reference) noexcept {
    ++counts_[2 * static_cast<unsigned>(comparison) + static_cast<unsigned>(reference)];
}

Interval95 PairedOutcomes::difference95() const {
    const long double neither = counts_[0];
    const long double reference_only = counts_[1];
    const long double comparison_only = counts_[2];
    const long double both = counts_[3];
    const long double count = neither + reference_only + comparison_only + both;
    if (count == 0) throw std::invalid_argument("paired interval requires at least one matched trial");

    const long double comparison = both + comparison_only;
    const long double reference = both + reference_only;
    long double numerator = both * neither - comparison_only * reference_only;
    if (numerator > 0) numerator = std::max(0.0L, numerator - count / 2);
    const long double denominator = std::sqrt(comparison * (count - comparison) * reference * (count - reference));
    const double phi = denominator > 0 ? std::clamp(static_cast<double>(numerator / denominator), -1.0, 1.0) : 0.0;
    const auto a = proportion_interval(comparison, count);
    const auto b = proportion_interval(reference, count);
    const double estimate = static_cast<double>((comparison_only - reference_only) / count);
    const auto margin = [phi](double first, double second) {
        return std::sqrt(std::max(0.0, first * first - 2 * phi * first * second + second * second));
    };
    return {estimate, std::max(-1.0, estimate - margin(a.estimate - a.low, b.high - b.estimate)),
            std::min(1.0, estimate + margin(a.high - a.estimate, b.estimate - b.low))};
}

}  // namespace ecm::lab
