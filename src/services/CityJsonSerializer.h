#pragma once

#include <QString>

namespace smartcity::algorithms {
class WeightedGraph;
}

namespace smartcity::services {

class CityJsonSerializer {
public:
    static bool save(const QString& fileName,
                     const algorithms::WeightedGraph& graph,
                     QString* errorMessage);
    static bool load(const QString& fileName,
                     algorithms::WeightedGraph& graph,
                     QString* errorMessage);
};

} // namespace smartcity::services
