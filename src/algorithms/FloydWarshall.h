#pragma once

#include "algorithms/RouteAlgorithms.h"

#include <cstdint>
#include <vector>

namespace smartcity::algorithms {

class WeightedGraph;

struct AllPairsPath {
    bool reachable{false};
    std::vector<int> locationIds;
    std::vector<int> roadIds;
    double totalWeight{0.0};
};

struct AllPairsResult {
    std::uint64_t graphRevision{0};
    RouteMetric metric{RouteMetric::Distance};
    std::vector<int> locationIds;
    std::vector<std::vector<double>> weights;
    std::vector<std::vector<int>> nextHop;
    std::vector<std::vector<int>> nextRoad;

    [[nodiscard]] AllPairsPath reconstructPath(int sourceLocationId,
                                               int destinationLocationId) const;
    [[nodiscard]] bool isReachable(int sourceLocationId,
                                   int destinationLocationId) const;
};

class FloydWarshall {
public:
    [[nodiscard]] static AllPairsResult compute(const WeightedGraph& graph,
                                                RouteMetric metric);
};

} // namespace smartcity::algorithms
