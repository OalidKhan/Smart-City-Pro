#include "algorithms/NumberTheoryAlgorithms.h"

#include <limits>
#include <utility>

namespace smartcity::algorithms {

std::uint64_t NumberTheoryAlgorithms::gcd(std::uint64_t first, std::uint64_t second,
                                          std::vector<EuclidStep>* steps) noexcept
{
    while (second != 0) {
        const std::uint64_t remainder = first % second;
        if (steps) steps->push_back({first, second, remainder});
        first = second;
        second = remainder;
    }
    return first;
}

MaintenanceCycleResult NumberTheoryAlgorithms::maintenanceCycles(
    const std::vector<std::uint64_t>& positiveIntervals)
{
    MaintenanceCycleResult result;
    if (positiveIntervals.empty()) return result;
    for (const auto interval : positiveIntervals) if (interval == 0) return result;

    result.validInput = true;
    result.greatestCommonDivisor = positiveIntervals.front();
    std::uint64_t accumulatedLcm = positiveIntervals.front();
    for (std::size_t index = 1; index < positiveIntervals.size(); ++index) {
        const std::uint64_t next = positiveIntervals[index];
        GcdCombinationStep gcdStep;
        gcdStep.accumulated = result.greatestCommonDivisor;
        gcdStep.nextInterval = next;
        gcdStep.result = gcd(result.greatestCommonDivisor, next, &gcdStep.euclidSteps);
        result.greatestCommonDivisor = gcdStep.result;
        result.gcdSteps.push_back(std::move(gcdStep));
        LcmStep step;
        step.accumulated = accumulatedLcm;
        step.nextInterval = next;
        step.gcd = gcd(accumulatedLcm, next, &step.euclidSteps);
        step.dividedBeforeMultiply = accumulatedLcm / step.gcd;
        if (step.dividedBeforeMultiply > std::numeric_limits<std::uint64_t>::max() / next) {
            step.overflow = true;
            result.steps.push_back(std::move(step));
            result.overflow = true;
            return result;
        }
        step.result = step.dividedBeforeMultiply * next;
        accumulatedLcm = step.result;
        result.steps.push_back(std::move(step));
    }
    result.nextCoincidence = accumulatedLcm;
    return result;
}

} // namespace smartcity::algorithms
