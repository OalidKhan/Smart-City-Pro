#pragma once

#include "algorithms/AlgorithmEvent.h"

#include <vector>

namespace smartcity::algorithms {

class WeightedGraph;

enum class RouteMetric { Distance, TravelTime };
enum class RouteStatus { Found, Unreachable, Trivial, InvalidInput };

struct RouteResult {
    RouteStatus status{RouteStatus::InvalidInput};
    std::vector<int> locationIds;
    std::vector<int> roadIds;
    double totalWeight{0.0};
    std::vector<AlgorithmEvent> events;
};

enum class TraversalKind { BreadthFirst, DepthFirst };

struct TraversalResult {
    bool validInput{false};
    bool destinationReachable{false};
    std::vector<int> visitOrder;
    std::vector<AlgorithmEvent> events;
};

class RouteAlgorithms {
public:
    [[nodiscard]] static RouteResult dijkstra(const WeightedGraph& graph,
                                               int startLocationId,
                                               int destinationLocationId,
                                               RouteMetric metric);

    [[nodiscard]] static TraversalResult traverse(const WeightedGraph& graph,
                                                   int startLocationId,
                                                   int destinationLocationId,
                                                   TraversalKind kind);
};

} // namespace smartcity::algorithms
