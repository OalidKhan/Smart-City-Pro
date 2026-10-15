#pragma once

#include <QString>

namespace smartcity::algorithms { class WeightedGraph; }
namespace smartcity::services {
class DeliveryScenario;

class DeliveryJsonSerializer {
public:
    static bool save(const QString& fileName, const DeliveryScenario& scenario,
                     QString* errorMessage = nullptr);
    static bool load(const QString& fileName, const algorithms::WeightedGraph& graph,
                     DeliveryScenario& scenario, QString* errorMessage = nullptr);
};
}
