#pragma once

#include "algorithms/WeightedGraph.h"
#include "services/EmergencyDispatchSimulation.h"

namespace smartcity::services {

class CityRepository {
public:
    CityRepository();

    [[nodiscard]] algorithms::WeightedGraph& city() noexcept;
    [[nodiscard]] const algorithms::WeightedGraph& city() const noexcept;
    [[nodiscard]] EmergencyDispatchSimulation& dispatchSimulation() noexcept;
    [[nodiscard]] const EmergencyDispatchSimulation& dispatchSimulation() const noexcept;

private:
    algorithms::WeightedGraph city_;
    EmergencyDispatchSimulation dispatchSimulation_;
};

} // namespace smartcity::services
