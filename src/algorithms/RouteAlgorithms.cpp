#include "algorithms/RouteAlgorithms.h"

#include "algorithms/WeightedGraph.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace smartcity::algorithms {
namespace {

double edgeWeight(const WeightedGraph::Edge& edge, RouteMetric metric)
{
    return metric == RouteMetric::Distance ? edge.distance : edge.travelTime;
}

std::vector<WeightedGraph::Edge> sortedOpenEdges(const WeightedGraph& graph, int locationId)
{
    std::vector<WeightedGraph::Edge> edges;
    for (const auto& edge : graph.neighbors(locationId)) {
        if (!edge.closed) edges.push_back(edge);
    }
    std::sort(edges.begin(), edges.end(), [](const auto& first, const auto& second) {
        if (first.destination != second.destination) return first.destination < second.destination;
        return first.roadId < second.roadId;
    });
    return edges;
}

AlgorithmEvent event(AlgorithmEventType type, int location = -1, int from = -1,
                     int to = -1, int road = -1, double value = 0.0)
{
    return {type, location, from, to, road, value};
}

} // namespace

RouteResult RouteAlgorithms::dijkstra(const WeightedGraph& graph, int startLocationId,
                                      int destinationLocationId, RouteMetric metric,
                                      bool recordEvents)
{
    RouteResult result;
    const auto emit = [&result, recordEvents](AlgorithmEvent algorithmEvent) {
        if (recordEvents) result.events.push_back(std::move(algorithmEvent));
    };
    if (!graph.containsVertex(startLocationId) || !graph.containsVertex(destinationLocationId)) {
        return result;
    }

    if (recordEvents) emit(event(AlgorithmEventType::Started, startLocationId));
    if (startLocationId == destinationLocationId) {
        result.status = RouteStatus::Trivial;
        result.locationIds.push_back(startLocationId);
        if (recordEvents) {
            emit(event(AlgorithmEventType::NodeVisited, startLocationId, -1, -1, -1, 0.0));
            emit(event(AlgorithmEventType::FinalRouteNode, startLocationId));
            emit(event(AlgorithmEventType::Finished, destinationLocationId, -1, -1, -1, 0.0));
        }
        return result;
    }

    constexpr double infinity = std::numeric_limits<double>::infinity();
    struct Parent { int previous{-1}; int roadId{-1}; };
    std::unordered_map<int, double> distances;
    std::unordered_map<int, Parent> parents;
    for (const auto& entry : graph.locations()) distances.emplace(entry.first, infinity);
    distances[startLocationId] = 0.0;

    using QueueEntry = std::pair<double, int>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> frontier;
    frontier.emplace(0.0, startLocationId);
    std::unordered_set<int> settled;

    while (!frontier.empty()) {
        const auto [currentDistance, current] = frontier.top();
        frontier.pop();
        if (currentDistance != distances.at(current) || settled.find(current) != settled.end()) continue;
        settled.insert(current);
        if (recordEvents)
            emit(event(AlgorithmEventType::NodeVisited, current, -1, -1, -1, currentDistance));
        if (current == destinationLocationId) break;

        for (const auto& edge : sortedOpenEdges(graph, current)) {
            if (recordEvents)
                emit(event(AlgorithmEventType::EdgeExplored, -1, current,
                           edge.destination, edge.roadId, edgeWeight(edge, metric)));
            const double candidate = currentDistance + edgeWeight(edge, metric);
            if (candidate < distances.at(edge.destination)) {
                distances[edge.destination] = candidate;
                parents[edge.destination] = {current, edge.roadId};
                frontier.emplace(candidate, edge.destination);
                if (recordEvents)
                    emit(event(AlgorithmEventType::DistanceUpdated, edge.destination,
                               current, edge.destination, edge.roadId, candidate));
            }
        }
    }

    if (settled.find(destinationLocationId) == settled.end()) {
        result.status = RouteStatus::Unreachable;
        if (recordEvents) emit(event(AlgorithmEventType::Finished, destinationLocationId));
        return result;
    }

    result.status = RouteStatus::Found;
    result.totalWeight = distances.at(destinationLocationId);
    int current = destinationLocationId;
    result.locationIds.push_back(current);
    while (current != startLocationId) {
        const auto parent = parents.at(current);
        result.roadIds.push_back(parent.roadId);
        current = parent.previous;
        result.locationIds.push_back(current);
    }
    std::reverse(result.locationIds.begin(), result.locationIds.end());
    std::reverse(result.roadIds.begin(), result.roadIds.end());

    if (recordEvents)
        emit(event(AlgorithmEventType::FinalRouteNode, result.locationIds.front()));
    for (std::size_t index = 0; index < result.roadIds.size(); ++index) {
        if (recordEvents) {
            emit(event(AlgorithmEventType::FinalRouteEdge, -1,
                       result.locationIds[index], result.locationIds[index + 1],
                       result.roadIds[index]));
            emit(event(AlgorithmEventType::FinalRouteNode, result.locationIds[index + 1]));
        }
    }
    if (recordEvents)
        emit(event(AlgorithmEventType::Finished, destinationLocationId,
                   -1, -1, -1, result.totalWeight));
    return result;
}

TraversalResult RouteAlgorithms::traverse(const WeightedGraph& graph, int startLocationId,
                                          int destinationLocationId, TraversalKind kind)
{
    TraversalResult result;
    if (!graph.containsVertex(startLocationId) || !graph.containsVertex(destinationLocationId)) return result;
    result.validInput = true;
    result.events.push_back(event(AlgorithmEventType::Started, startLocationId));

    std::unordered_set<int> discovered;
    if (kind == TraversalKind::BreadthFirst) {
        std::queue<int> frontier;
        frontier.push(startLocationId);
        discovered.insert(startLocationId);
        while (!frontier.empty()) {
            const int current = frontier.front();
            frontier.pop();
            result.visitOrder.push_back(current);
            result.events.push_back(event(AlgorithmEventType::NodeVisited, current));
            for (const auto& edge : sortedOpenEdges(graph, current)) {
                result.events.push_back(event(AlgorithmEventType::EdgeExplored, -1, current,
                                              edge.destination, edge.roadId));
                if (discovered.insert(edge.destination).second) frontier.push(edge.destination);
            }
        }
    } else {
        std::stack<int> frontier;
        frontier.push(startLocationId);
        while (!frontier.empty()) {
            const int current = frontier.top();
            frontier.pop();
            if (!discovered.insert(current).second) continue;
            result.visitOrder.push_back(current);
            result.events.push_back(event(AlgorithmEventType::NodeVisited, current));
            auto edges = sortedOpenEdges(graph, current);
            for (auto iterator = edges.rbegin(); iterator != edges.rend(); ++iterator) {
                result.events.push_back(event(AlgorithmEventType::EdgeExplored, -1, current,
                                              iterator->destination, iterator->roadId));
                if (discovered.find(iterator->destination) == discovered.end()) {
                    frontier.push(iterator->destination);
                }
            }
        }
    }

    result.destinationReachable = discovered.find(destinationLocationId) != discovered.end();
    result.events.push_back(event(AlgorithmEventType::Finished, destinationLocationId));
    return result;
}

} // namespace smartcity::algorithms
