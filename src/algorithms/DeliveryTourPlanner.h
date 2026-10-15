#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace smartcity::algorithms {

class WeightedGraph;

enum class DeliveryTourStatus { Solved, Trivial, InvalidInput, TooManyStops, Unreachable };

struct DeliveryTourLeg {
    int fromLocationId{-1};
    int toLocationId{-1};
    double travelTime{0.0};
    std::vector<int> locationIds;
    std::vector<int> roadIds;
};

struct DeliveryTourResult {
    DeliveryTourStatus status{DeliveryTourStatus::InvalidInput};
    std::uint64_t graphRevision{0};
    double totalTravelTime{0.0};
    std::vector<int> serviceOrder;
    std::vector<int> expandedLocationIds;
    std::vector<int> roadIds;
    std::vector<DeliveryTourLeg> legs;
    std::vector<int> unreachableStopIds;
};

class DeliveryTourPlanner {
public:
    static constexpr std::size_t MaximumDeliveryStops = 12;

    [[nodiscard]] static DeliveryTourResult compute(const WeightedGraph& graph,
                                                    int depotLocationId,
                                                    const std::vector<int>& stopLocationIds);
};

} // namespace smartcity::algorithms
