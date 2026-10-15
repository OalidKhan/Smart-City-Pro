#pragma once

#include "algorithms/AlgorithmEvent.h"

#include <cstddef>
#include <string>
#include <vector>

namespace smartcity::algorithms {

enum class SortAlgorithm { Bubble, Selection, Insertion, Merge, Quick, Heap };
enum class SortOrder { Ascending, Descending };

struct OperationCounters {
    std::size_t comparisons{0};
    std::size_t swaps{0};
    std::size_t writes{0};
};

struct SortResult {
    bool validInput{false};
    std::vector<double> values;
    OperationCounters counters;
    std::vector<AlgorithmEvent> events;
    bool eventLimitReached{false};
};

struct SearchResult {
    bool validInput{false};
    bool found{false};
    int index{-1};
    std::size_t comparisons{0};
    std::vector<AlgorithmEvent> events;
    bool eventLimitReached{false};
};

struct AlgorithmProfile {
    const char* name;
    const char* timeComplexity;
    const char* auxiliarySpace;
    bool stable;
    bool divideAndConquer;
};

class SortingAlgorithms {
public:
    static constexpr std::size_t MaximumStoredEvents = 100'000;

    [[nodiscard]] static SortResult sort(const std::vector<double>& input,
                                         SortAlgorithm algorithm,
                                         SortOrder order,
                                         bool recordEvents = true);
    [[nodiscard]] static SearchResult binarySearch(const std::vector<double>& sortedInput,
                                                   double target,
                                                   SortOrder order);
    [[nodiscard]] static AlgorithmProfile profile(SortAlgorithm algorithm) noexcept;
};

} // namespace smartcity::algorithms
