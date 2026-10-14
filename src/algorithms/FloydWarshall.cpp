#include "algorithms/FloydWarshall.h"

#include "algorithms/WeightedGraph.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <unordered_map>

namespace smartcity::algorithms {
namespace {

double selectedWeight(const WeightedGraph::Edge& edge, RouteMetric metric)
{
    return metric == RouteMetric::Distance ? edge.distance : edge.travelTime;
}

double safeAdd(double left, double right)
{
    const double infinity = std::numeric_limits<double>::infinity();
    if (!std::isfinite(left) || !std::isfinite(right)) return infinity;
    if (left > std::numeric_limits<double>::max() - right) return infinity;
    return left + right;
}

std::size_t findIndex(const std::vector<int>& ids, int id)
{
    const auto found = std::lower_bound(ids.begin(), ids.end(), id);
    return found == ids.end() || *found != id
        ? ids.size() : static_cast<std::size_t>(std::distance(ids.begin(), found));
}

} // namespace

AllPairsPath AllPairsResult::reconstructPath(int sourceLocationId,
                                             int destinationLocationId) const
{
    AllPairsPath path;
    const std::size_t source = findIndex(locationIds, sourceLocationId);
    const std::size_t destination = findIndex(locationIds, destinationLocationId);
    if (source >= locationIds.size() || destination >= locationIds.size()
        || nextHop.empty() || nextHop[source][destination] < 0) return path;

    path.totalWeight = weights[source][destination];
    path.locationIds.push_back(sourceLocationId);
    if (source == destination) {
        path.reachable = true;
        return path;
    }

    std::size_t current = source;
    for (std::size_t steps = 0; steps < locationIds.size(); ++steps) {
        const int nextIndex = nextHop[current][destination];
        const int roadId = nextRoad[current][destination];
        if (nextIndex < 0 || roadId < 0
            || static_cast<std::size_t>(nextIndex) >= locationIds.size()) return {};
        path.roadIds.push_back(roadId);
        current = static_cast<std::size_t>(nextIndex);
        path.locationIds.push_back(locationIds[current]);
        if (current == destination) {
            path.reachable = true;
            return path;
        }
    }
    return {};
}

bool AllPairsResult::isReachable(int sourceLocationId, int destinationLocationId) const
{
    const std::size_t source = findIndex(locationIds, sourceLocationId);
    const std::size_t destination = findIndex(locationIds, destinationLocationId);
    return source < locationIds.size() && destination < locationIds.size()
        && !nextHop.empty() && nextHop[source][destination] >= 0;
}

AllPairsResult FloydWarshall::compute(const WeightedGraph& graph, RouteMetric metric)
{
    AllPairsResult result;
    result.graphRevision = graph.revision();
    result.metric = metric;
    for (const auto& entry : graph.locations()) result.locationIds.push_back(entry.first);
    std::sort(result.locationIds.begin(), result.locationIds.end());

    const std::size_t count = result.locationIds.size();
    const double infinity = std::numeric_limits<double>::infinity();
    result.weights.assign(count, std::vector<double>(count, infinity));
    result.nextHop.assign(count, std::vector<int>(count, -1));
    result.nextRoad.assign(count, std::vector<int>(count, -1));

    std::unordered_map<int, std::size_t> indexById;
    for (std::size_t index = 0; index < count; ++index) {
        indexById.emplace(result.locationIds[index], index);
        result.weights[index][index] = 0.0;
        result.nextHop[index][index] = static_cast<int>(index);
    }

    for (std::size_t from = 0; from < count; ++from) {
        for (const auto& edge : graph.neighbors(result.locationIds[from])) {
            if (edge.closed) continue;
            const auto destination = indexById.find(edge.destination);
            if (destination == indexById.end()) continue;
            const std::size_t to = destination->second;
            const double weight = selectedWeight(edge, metric);
            if (weight < result.weights[from][to]
                || (weight == result.weights[from][to]
                    && (result.nextRoad[from][to] < 0 || edge.roadId < result.nextRoad[from][to]))) {
                result.weights[from][to] = weight;
                result.nextHop[from][to] = static_cast<int>(to);
                result.nextRoad[from][to] = edge.roadId;
            }
        }
    }

    for (std::size_t via = 0; via < count; ++via) {
        for (std::size_t from = 0; from < count; ++from) {
            if (!std::isfinite(result.weights[from][via])) continue;
            for (std::size_t to = 0; to < count; ++to) {
                const double candidate = safeAdd(result.weights[from][via],
                                                 result.weights[via][to]);
                if (candidate < result.weights[from][to]) {
                    result.weights[from][to] = candidate;
                    result.nextHop[from][to] = result.nextHop[from][via];
                    result.nextRoad[from][to] = result.nextRoad[from][via];
                }
            }
        }
    }
    return result;
}

} // namespace smartcity::algorithms
