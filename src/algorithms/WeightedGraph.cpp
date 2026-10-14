#include "algorithms/WeightedGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace smartcity::algorithms {

int WeightedGraph::nextLocationId() noexcept
{
    while (locations_.find(nextLocationId_) != locations_.end()) ++nextLocationId_;
    return nextLocationId_++;
}

int WeightedGraph::nextRoadId() noexcept
{
    while (roads_.find(nextRoadId_) != roads_.end()) ++nextRoadId_;
    return nextRoadId_++;
}

int WeightedGraph::nextCandidateId() noexcept
{
    while (constructionCandidates_.find(nextCandidateId_) != constructionCandidates_.end()) {
        ++nextCandidateId_;
    }
    return nextCandidateId_++;
}

bool WeightedGraph::addLocation(const core::CityLocation& location)
{
    if (!location.isValid() || location.id == std::numeric_limits<int>::max()
        || locations_.find(location.id) != locations_.end()) return false;
    locations_.emplace(location.id, location);
    adjacencyList_.try_emplace(location.id);
    nextLocationId_ = std::max(nextLocationId_, location.id + 1);
    touch();
    return true;
}

bool WeightedGraph::updateLocation(const core::CityLocation& location)
{
    auto found = locations_.find(location.id);
    if (!location.isValid() || found == locations_.end()) return false;
    found->second = location;
    touch();
    return true;
}

bool WeightedGraph::moveLocation(int locationId, double x, double y)
{
    auto found = locations_.find(locationId);
    if (found == locations_.end() || !std::isfinite(x) || !std::isfinite(y)) return false;
    if (found->second.x == x && found->second.y == y) return true;
    found->second.x = x;
    found->second.y = y;
    touch();
    return true;
}

bool WeightedGraph::removeLocation(int locationId)
{
    if (locations_.erase(locationId) == 0) return false;
    for (auto iterator = roads_.begin(); iterator != roads_.end();) {
        const auto& road = iterator->second;
        if (road.fromLocationId == locationId || road.toLocationId == locationId) {
            iterator = roads_.erase(iterator);
        } else {
            ++iterator;
        }
    }
    for (auto iterator = constructionCandidates_.begin();
         iterator != constructionCandidates_.end();) {
        const auto& candidate = iterator->second;
        if (candidate.firstLocationId == locationId || candidate.secondLocationId == locationId) {
            iterator = constructionCandidates_.erase(iterator);
        } else {
            ++iterator;
        }
    }
    rebuildAdjacency();
    touch();
    return true;
}

bool WeightedGraph::addRoad(const core::Road& road)
{
    if (road.id < 0 || road.id == std::numeric_limits<int>::max()
        || road.fromLocationId == road.toLocationId || !road.hasValidWeights()
        || !containsVertex(road.fromLocationId) || !containsVertex(road.toLocationId)
        || roads_.find(road.id) != roads_.end()) return false;
    roads_.emplace(road.id, road);
    nextRoadId_ = std::max(nextRoadId_, road.id + 1);
    rebuildAdjacency();
    touch();
    return true;
}

bool WeightedGraph::updateRoad(const core::Road& road)
{
    auto found = roads_.find(road.id);
    if (found == roads_.end() || road.fromLocationId == road.toLocationId
        || !road.hasValidWeights() || !containsVertex(road.fromLocationId)
        || !containsVertex(road.toLocationId)) return false;
    found->second = road;
    rebuildAdjacency();
    touch();
    return true;
}

bool WeightedGraph::removeRoad(int roadId)
{
    if (roads_.erase(roadId) == 0) return false;
    rebuildAdjacency();
    touch();
    return true;
}

bool WeightedGraph::addConstructionCandidate(const core::ConstructionCandidate& candidate)
{
    if (candidate.id < 0 || candidate.id == std::numeric_limits<int>::max()
        || candidate.firstLocationId == candidate.secondLocationId || !candidate.hasValidCost()
        || !containsVertex(candidate.firstLocationId) || !containsVertex(candidate.secondLocationId)
        || constructionCandidates_.find(candidate.id) != constructionCandidates_.end()) return false;
    constructionCandidates_.emplace(candidate.id, candidate);
    nextCandidateId_ = std::max(nextCandidateId_, candidate.id + 1);
    touch();
    return true;
}

bool WeightedGraph::updateConstructionCandidate(const core::ConstructionCandidate& candidate)
{
    auto found = constructionCandidates_.find(candidate.id);
    if (found == constructionCandidates_.end()
        || candidate.firstLocationId == candidate.secondLocationId || !candidate.hasValidCost()
        || !containsVertex(candidate.firstLocationId) || !containsVertex(candidate.secondLocationId)) return false;
    found->second = candidate;
    touch();
    return true;
}

bool WeightedGraph::removeConstructionCandidate(int candidateId)
{
    if (constructionCandidates_.erase(candidateId) == 0) return false;
    touch();
    return true;
}

bool WeightedGraph::containsVertex(int vertexId) const noexcept
{
    return locations_.find(vertexId) != locations_.end();
}

std::size_t WeightedGraph::vertexCount() const noexcept { return locations_.size(); }
std::size_t WeightedGraph::roadCount() const noexcept { return roads_.size(); }
std::size_t WeightedGraph::constructionCandidateCount() const noexcept
{
    return constructionCandidates_.size();
}

const core::CityLocation* WeightedGraph::location(int locationId) const noexcept
{
    const auto found = locations_.find(locationId);
    return found == locations_.end() ? nullptr : &found->second;
}

const core::Road* WeightedGraph::road(int roadId) const noexcept
{
    const auto found = roads_.find(roadId);
    return found == roads_.end() ? nullptr : &found->second;
}

const core::ConstructionCandidate* WeightedGraph::constructionCandidate(int candidateId) const noexcept
{
    const auto found = constructionCandidates_.find(candidateId);
    return found == constructionCandidates_.end() ? nullptr : &found->second;
}

const std::unordered_map<int, core::CityLocation>& WeightedGraph::locations() const noexcept
{
    return locations_;
}

const std::unordered_map<int, core::Road>& WeightedGraph::roads() const noexcept
{
    return roads_;
}

const std::unordered_map<int, core::ConstructionCandidate>&
WeightedGraph::constructionCandidates() const noexcept
{
    return constructionCandidates_;
}

const std::vector<WeightedGraph::Edge>& WeightedGraph::neighbors(int vertexId) const
{
    const auto found = adjacencyList_.find(vertexId);
    if (found == adjacencyList_.end()) throw std::out_of_range("Vertex does not exist in the graph");
    return found->second;
}

std::uint64_t WeightedGraph::revision() const noexcept { return revision_; }

void WeightedGraph::replaceWith(const WeightedGraph& other)
{
    const auto nextRevision = revision_ + 1;
    locations_ = other.locations_;
    roads_ = other.roads_;
    constructionCandidates_ = other.constructionCandidates_;
    adjacencyList_ = other.adjacencyList_;
    nextLocationId_ = other.nextLocationId_;
    nextRoadId_ = other.nextRoadId_;
    nextCandidateId_ = other.nextCandidateId_;
    revision_ = nextRevision;
}

void WeightedGraph::rebuildAdjacency()
{
    adjacencyList_.clear();
    for (const auto& entry : locations_) adjacencyList_.try_emplace(entry.first);
    for (const auto& [id, road] : roads_) {
        adjacencyList_.at(road.fromLocationId).push_back(
            {id, road.toLocationId, road.distance, road.travelTime, road.constructionCost, road.closed});
        if (road.direction == core::RoadDirection::TwoWay) {
            adjacencyList_.at(road.toLocationId).push_back(
                {id, road.fromLocationId, road.distance, road.travelTime, road.constructionCost, road.closed});
        }
    }
}

void WeightedGraph::touch() noexcept { ++revision_; }

} // namespace smartcity::algorithms
