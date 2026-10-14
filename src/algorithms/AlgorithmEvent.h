#pragma once

namespace smartcity::algorithms {

enum class AlgorithmEventType {
    Started,
    NodeVisited,
    EdgeExplored,
    DistanceUpdated,
    FinalRouteNode,
    FinalRouteEdge,
    ComponentStarted,
    CandidateConsidered,
    CandidateAccepted,
    CandidateRejected,
    Finished
};

struct AlgorithmEvent {
    AlgorithmEventType type{AlgorithmEventType::Started};
    int locationId{-1};
    int fromLocationId{-1};
    int toLocationId{-1};
    int roadId{-1};
    double value{0.0};
    int candidateId{-1};
};

} // namespace smartcity::algorithms
