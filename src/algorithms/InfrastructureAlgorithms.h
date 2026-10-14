#pragma once

#include "algorithms/AlgorithmEvent.h"

#include <vector>

namespace smartcity::algorithms {

class WeightedGraph;

enum class InfrastructureAlgorithm { Prim, Kruskal };

struct SpanningForestResult {
    std::vector<int> candidateIds;
    double totalCost{0.0};
    int componentCount{0};
    std::vector<AlgorithmEvent> events;
};

class InfrastructureAlgorithms {
public:
    static SpanningForestResult minimumSpanningForest(
        const WeightedGraph& graph, InfrastructureAlgorithm algorithm);
    static SpanningForestResult prim(const WeightedGraph& graph);
    static SpanningForestResult kruskal(const WeightedGraph& graph);
};

} // namespace smartcity::algorithms
