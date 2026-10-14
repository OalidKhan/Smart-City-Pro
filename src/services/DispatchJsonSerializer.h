#pragma once

#include <QString>

namespace smartcity::algorithms { class WeightedGraph; }
namespace smartcity::services {

class EmergencyDispatchSimulation;

class DispatchJsonSerializer {
public:
    static bool save(const QString& fileName,
                     const EmergencyDispatchSimulation& simulation,
                     QString* errorMessage = nullptr);
    static bool load(const QString& fileName,
                     const algorithms::WeightedGraph& graph,
                     EmergencyDispatchSimulation& simulation,
                     QString* errorMessage = nullptr);
};

} // namespace smartcity::services
