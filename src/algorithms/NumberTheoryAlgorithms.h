#pragma once

#include <cstdint>
#include <vector>

namespace smartcity::algorithms {

struct EuclidStep {
    std::uint64_t dividend{0};
    std::uint64_t divisor{0};
    std::uint64_t remainder{0};
};

struct LcmStep {
    std::uint64_t accumulated{0};
    std::uint64_t nextInterval{0};
    std::uint64_t gcd{0};
    std::uint64_t dividedBeforeMultiply{0};
    std::uint64_t result{0};
    bool overflow{false};
    std::vector<EuclidStep> euclidSteps;
};

struct GcdCombinationStep {
    std::uint64_t accumulated{0};
    std::uint64_t nextInterval{0};
    std::uint64_t result{0};
    std::vector<EuclidStep> euclidSteps;
};

struct MaintenanceCycleResult {
    bool validInput{false};
    bool overflow{false};
    std::uint64_t greatestCommonDivisor{0};
    std::uint64_t nextCoincidence{0};
    std::vector<GcdCombinationStep> gcdSteps;
    std::vector<LcmStep> steps;
};

class NumberTheoryAlgorithms {
public:
    [[nodiscard]] static std::uint64_t gcd(std::uint64_t first,
                                           std::uint64_t second,
                                           std::vector<EuclidStep>* steps = nullptr) noexcept;
    [[nodiscard]] static MaintenanceCycleResult maintenanceCycles(
        const std::vector<std::uint64_t>& positiveIntervals);
};

} // namespace smartcity::algorithms
