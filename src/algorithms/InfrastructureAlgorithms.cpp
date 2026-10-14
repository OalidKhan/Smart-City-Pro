#include "algorithms/InfrastructureAlgorithms.h"

#include "algorithms/DisjointSetUnion.h"
#include "algorithms/WeightedGraph.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace smartcity::algorithms {
namespace {

AlgorithmEvent candidateEvent(AlgorithmEventType type, int candidateId,
                              int first, int second, double cost)
{
    AlgorithmEvent event;
    event.type = type;
    event.fromLocationId = first;
    event.toLocationId = second;
    event.value = cost;
    event.candidateId = candidateId;
    return event;
}

std::vector<int> sortedLocationIds(const WeightedGraph& graph)
{
    std::vector<int> ids;
    ids.reserve(graph.vertexCount());
    for (const auto& entry : graph.locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

SpanningForestResult InfrastructureAlgorithms::minimumSpanningForest(
    const WeightedGraph& graph, InfrastructureAlgorithm algorithm)
{
    return algorithm == InfrastructureAlgorithm::Prim ? prim(graph) : kruskal(graph);
}

SpanningForestResult InfrastructureAlgorithms::prim(const WeightedGraph& graph)
{
    SpanningForestResult result;
    result.events.push_back({AlgorithmEventType::Started});
    const auto locationIds = sortedLocationIds(graph);
    if (locationIds.empty()) {
        result.events.push_back({AlgorithmEventType::Finished});
        return result;
    }

    std::unordered_map<int, std::vector<int>> incident;
    for (const int id : locationIds) incident.emplace(id, std::vector<int>{});
    for (const auto& entry : graph.constructionCandidates()) {
        const auto& candidate = entry.second;
        incident.at(candidate.firstLocationId).push_back(candidate.id);
        incident.at(candidate.secondLocationId).push_back(candidate.id);
    }

    using QueueItem = std::tuple<double, int, int, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
    std::unordered_set<int> visited;

    for (const int start : locationIds) {
        if (visited.find(start) != visited.end()) continue;
        ++result.componentCount;
        AlgorithmEvent component;
        component.type = AlgorithmEventType::ComponentStarted;
        component.locationId = start;
        result.events.push_back(component);
        visited.insert(start);
        result.events.push_back({AlgorithmEventType::NodeVisited, start});

        const auto addIncident = [&](int locationId) {
            for (const int candidateId : incident.at(locationId)) {
                const auto* candidate = graph.constructionCandidate(candidateId);
                if (!candidate) continue;
                const int other = candidate->firstLocationId == locationId
                    ? candidate->secondLocationId : candidate->firstLocationId;
                if (visited.find(other) == visited.end()) {
                    queue.emplace(candidate->constructionCost, candidateId, locationId, other);
                }
            }
        };
        addIncident(start);

        while (!queue.empty()) {
            const auto [cost, candidateId, from, to] = queue.top();
            queue.pop();
            result.events.push_back(candidateEvent(
                AlgorithmEventType::CandidateConsidered, candidateId, from, to, cost));
            if (visited.find(to) != visited.end()) {
                result.events.push_back(candidateEvent(
                    AlgorithmEventType::CandidateRejected, candidateId, from, to, cost));
                continue;
            }
            visited.insert(to);
            result.candidateIds.push_back(candidateId);
            result.totalCost += cost;
            result.events.push_back(candidateEvent(
                AlgorithmEventType::CandidateAccepted, candidateId, from, to, cost));
            result.events.push_back({AlgorithmEventType::NodeVisited, to});
            addIncident(to);
        }
    }
    result.events.push_back({AlgorithmEventType::Finished});
    return result;
}

SpanningForestResult InfrastructureAlgorithms::kruskal(const WeightedGraph& graph)
{
    SpanningForestResult result;
    result.events.push_back({AlgorithmEventType::Started});
    const auto locationIds = sortedLocationIds(graph);
    result.componentCount = static_cast<int>(locationIds.size());
    if (locationIds.empty()) {
        result.events.push_back({AlgorithmEventType::Finished});
        return result;
    }

    std::unordered_map<int, int> indexById;
    for (std::size_t index = 0; index < locationIds.size(); ++index)
        indexById.emplace(locationIds[index], static_cast<int>(index));

    std::vector<const core::ConstructionCandidate*> candidates;
    candidates.reserve(graph.constructionCandidateCount());
    for (const auto& entry : graph.constructionCandidates()) candidates.push_back(&entry.second);
    std::sort(candidates.begin(), candidates.end(), [](const auto* left, const auto* right) {
        if (left->constructionCost != right->constructionCost)
            return left->constructionCost < right->constructionCost;
        return left->id < right->id;
    });

    DisjointSetUnion sets(locationIds.size());
    for (const auto* candidate : candidates) {
        result.events.push_back(candidateEvent(AlgorithmEventType::CandidateConsidered,
            candidate->id, candidate->firstLocationId, candidate->secondLocationId,
            candidate->constructionCost));
        if (sets.unite(indexById.at(candidate->firstLocationId),
                       indexById.at(candidate->secondLocationId))) {
            result.candidateIds.push_back(candidate->id);
            result.totalCost += candidate->constructionCost;
            result.events.push_back(candidateEvent(AlgorithmEventType::CandidateAccepted,
                candidate->id, candidate->firstLocationId, candidate->secondLocationId,
                candidate->constructionCost));
        } else {
            result.events.push_back(candidateEvent(AlgorithmEventType::CandidateRejected,
                candidate->id, candidate->firstLocationId, candidate->secondLocationId,
                candidate->constructionCost));
        }
    }
    result.componentCount = static_cast<int>(sets.componentCount());
    result.events.push_back({AlgorithmEventType::Finished});
    return result;
}

} // namespace smartcity::algorithms
