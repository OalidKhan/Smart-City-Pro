#pragma once

#include "core/models/CityLocation.h"
#include "core/models/ConstructionCandidate.h"
#include "core/models/Road.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace smartcity::algorithms {

class WeightedGraph {
public:
    struct Edge {
        int roadId;
        int destination;
        double distance;
        double travelTime;
        double constructionCost;
        bool closed;
    };

    [[nodiscard]] int nextLocationId() noexcept;
    [[nodiscard]] int nextRoadId() noexcept;
    [[nodiscard]] int nextCandidateId() noexcept;

    bool addLocation(const core::CityLocation& location);
    bool updateLocation(const core::CityLocation& location);
    bool moveLocation(int locationId, double x, double y);
    bool removeLocation(int locationId);

    bool addRoad(const core::Road& road);
    bool updateRoad(const core::Road& road);
    bool removeRoad(int roadId);

    bool addConstructionCandidate(const core::ConstructionCandidate& candidate);
    bool updateConstructionCandidate(const core::ConstructionCandidate& candidate);
    bool removeConstructionCandidate(int candidateId);

    [[nodiscard]] bool containsVertex(int vertexId) const noexcept;
    [[nodiscard]] std::size_t vertexCount() const noexcept;
    [[nodiscard]] std::size_t roadCount() const noexcept;
    [[nodiscard]] std::size_t constructionCandidateCount() const noexcept;
    [[nodiscard]] const core::CityLocation* location(int locationId) const noexcept;
    [[nodiscard]] const core::Road* road(int roadId) const noexcept;
    [[nodiscard]] const core::ConstructionCandidate* constructionCandidate(int candidateId) const noexcept;
    [[nodiscard]] const std::unordered_map<int, core::CityLocation>& locations() const noexcept;
    [[nodiscard]] const std::unordered_map<int, core::Road>& roads() const noexcept;
    [[nodiscard]] const std::unordered_map<int, core::ConstructionCandidate>& constructionCandidates() const noexcept;
    [[nodiscard]] const std::vector<Edge>& neighbors(int vertexId) const;
    [[nodiscard]] std::uint64_t revision() const noexcept;

    void replaceWith(const WeightedGraph& other);

private:
    void rebuildAdjacency();
    void touch() noexcept;

    std::unordered_map<int, core::CityLocation> locations_;
    std::unordered_map<int, core::Road> roads_;
    std::unordered_map<int, core::ConstructionCandidate> constructionCandidates_;
    std::unordered_map<int, std::vector<Edge>> adjacencyList_;
    int nextLocationId_{0};
    int nextRoadId_{0};
    int nextCandidateId_{0};
    std::uint64_t revision_{0};
};

} // namespace smartcity::algorithms
