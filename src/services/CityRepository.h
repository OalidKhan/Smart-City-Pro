#pragma once

#include "algorithms/WeightedGraph.h"

namespace smartcity::services {

class CityRepository {
public:
    CityRepository();

    [[nodiscard]] algorithms::WeightedGraph& city() noexcept;
    [[nodiscard]] const algorithms::WeightedGraph& city() const noexcept;

private:
    algorithms::WeightedGraph city_;
};

} // namespace smartcity::services
