#pragma once

namespace smartcity::algorithms {

enum class AlgorithmEventType {
    Started,
    NodeVisited,
    EdgeExplored,
    DistanceUpdated,
    FinalRouteNode,
    FinalRouteEdge,
    Finished
};

struct AlgorithmEvent {
    AlgorithmEventType type{AlgorithmEventType::Started};
    int locationId{-1};
    int fromLocationId{-1};
    int toLocationId{-1};
    int roadId{-1};
    double value{0.0};
};

} // namespace smartcity::algorithms
