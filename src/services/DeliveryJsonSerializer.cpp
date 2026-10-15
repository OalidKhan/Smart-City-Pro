#include "services/DeliveryJsonSerializer.h"

#include "algorithms/WeightedGraph.h"
#include "core/models/DeliveryPackage.h"
#include "services/DeliveryScenario.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace smartcity::services {
namespace {
bool integerField(const QJsonObject& object, const char* key, int& value, int minimum)
{
    const auto item = object.value(QLatin1String(key));
    const double number = item.toDouble(-1.0);
    if (!item.isDouble() || !std::isfinite(number) || std::floor(number) != number
        || number < minimum || number > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(number);
    return true;
}
}

bool DeliveryJsonSerializer::save(const QString& fileName, const DeliveryScenario& scenario,
                                  QString* errorMessage)
{
    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("depotLocation", scenario.depotLocationId());
    root.insert("vehicleCapacity", scenario.vehicleCapacity());
    std::vector<int> ids;
    for (const auto& entry : scenario.packages()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    QJsonArray packages;
    for (int id : ids) {
        const auto& package = scenario.packages().at(id);
        packages.append(QJsonObject{{"id", package.id},
                                    {"destination", package.destinationLocationId},
                                    {"weight", package.weight},
                                    {"value", package.value},
                                    {"priority", QLatin1String(core::packagePriorityName(package.priority))}});
    }
    root.insert("packages", packages);

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return true;
}

bool DeliveryJsonSerializer::load(const QString& fileName, const algorithms::WeightedGraph& graph,
                                  DeliveryScenario& scenario, QString* errorMessage)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) *errorMessage = parseError.error == QJsonParseError::NoError
            ? "Delivery JSON root must be an object." : "JSON parse error: " + parseError.errorString();
        return false;
    }
    const auto root = document.object();
    int depot = -1;
    int capacity = 0;
    if (root.value("schemaVersion").toInt(-1) != 1
        || !integerField(root, "depotLocation", depot, 0)
        || !integerField(root, "vehicleCapacity", capacity, 1)
        || !root.value("packages").isArray()) {
        if (errorMessage) *errorMessage = "Expected delivery schemaVersion 1, a depot, positive capacity, and packages array.";
        return false;
    }

    std::vector<core::DeliveryPackage> packages;
    const auto array = root.value("packages").toArray();
    for (qsizetype index = 0; index < array.size(); ++index) {
        if (!array.at(index).isObject()) {
            if (errorMessage) *errorMessage = QString("Package %1 must be an object.").arg(index);
            return false;
        }
        const auto object = array.at(index).toObject();
        core::DeliveryPackage package;
        const auto valueItem = object.value("value");
        const std::string priorityName = object.value("priority").toString().toStdString();
        if (!integerField(object, "id", package.id, 0)
            || !integerField(object, "destination", package.destinationLocationId, 0)
            || !integerField(object, "weight", package.weight, 1)
            || !valueItem.isDouble() || !std::isfinite(valueItem.toDouble()) || valueItem.toDouble() < 0.0
            || !core::packagePriorityFromName(priorityName.c_str(), package.priority)) {
            if (errorMessage) *errorMessage = QString("Package %1 has invalid fields.").arg(index);
            return false;
        }
        package.value = valueItem.toDouble();
        packages.push_back(package);
    }
    std::string error;
    if (!scenario.replaceData(depot, capacity, packages, graph, &error)) {
        if (errorMessage) *errorMessage = QString::fromStdString(error) + " Current delivery data was not changed.";
        return false;
    }
    return true;
}

} // namespace smartcity::services
