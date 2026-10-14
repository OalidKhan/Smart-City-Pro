#include "services/CityJsonSerializer.h"

#include "algorithms/WeightedGraph.h"
#include "core/models/CityLocation.h"
#include "core/models/ConstructionCandidate.h"
#include "core/models/Road.h"

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

bool integerField(const QJsonObject& object, const char* name, int& value)
{
    const auto item = object.value(QLatin1String(name));
    if (!item.isDouble() || !std::isfinite(item.toDouble())
        || std::floor(item.toDouble()) != item.toDouble()
        || item.toDouble() < 0.0 || item.toDouble() > std::numeric_limits<int>::max()) return false;
    value = item.toInt(-1);
    return value >= 0;
}

bool numberField(const QJsonObject& object, const char* name, double& value, bool nonNegative)
{
    const auto item = object.value(QLatin1String(name));
    if (!item.isDouble() || !std::isfinite(item.toDouble())) return false;
    value = item.toDouble();
    return !nonNegative || value >= 0.0;
}

QString describeIndex(const char* collection, qsizetype index, const QString& detail)
{
    return QString("Invalid %1 at index %2: %3").arg(QLatin1String(collection)).arg(index).arg(detail);
}

} // namespace

bool CityJsonSerializer::save(const QString& fileName,
                              const algorithms::WeightedGraph& graph,
                              QString* errorMessage)
{
    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("graphRevision", static_cast<qint64>(graph.revision()));

    std::vector<int> locationIds;
    for (const auto& item : graph.locations()) locationIds.push_back(item.first);
    std::sort(locationIds.begin(), locationIds.end());
    QJsonArray locations;
    for (const int id : locationIds) {
        const auto& location = graph.locations().at(id);
        QJsonObject object;
        object.insert("id", location.id);
        object.insert("name", QString::fromStdString(location.name));
        object.insert("x", location.x);
        object.insert("y", location.y);
        object.insert("category", QLatin1String(core::categoryName(location.category)));
        locations.append(object);
    }
    root.insert("locations", locations);

    std::vector<int> roadIds;
    for (const auto& item : graph.roads()) roadIds.push_back(item.first);
    std::sort(roadIds.begin(), roadIds.end());
    QJsonArray roads;
    for (const int id : roadIds) {
        const auto& road = graph.roads().at(id);
        QJsonObject object;
        object.insert("id", road.id);
        object.insert("from", road.fromLocationId);
        object.insert("to", road.toLocationId);
        object.insert("distance", road.distance);
        object.insert("travelTime", road.travelTime);
        object.insert("constructionCost", road.constructionCost);
        object.insert("direction", QLatin1String(core::directionName(road.direction)));
        object.insert("closed", road.closed);
        roads.append(object);
    }
    root.insert("roads", roads);

    std::vector<int> candidateIds;
    for (const auto& item : graph.constructionCandidates()) candidateIds.push_back(item.first);
    std::sort(candidateIds.begin(), candidateIds.end());
    QJsonArray constructionCandidates;
    for (const int id : candidateIds) {
        const auto& candidate = graph.constructionCandidates().at(id);
        QJsonObject object;
        object.insert("id", candidate.id);
        object.insert("first", candidate.firstLocationId);
        object.insert("second", candidate.secondLocationId);
        object.insert("constructionCost", candidate.constructionCost);
        constructionCandidates.append(object);
    }
    root.insert("constructionCandidates", constructionCandidates);

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

bool CityJsonSerializer::load(const QString& fileName,
                              algorithms::WeightedGraph& graph,
                              QString* errorMessage)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage) *errorMessage = "JSON parse error: " + parseError.errorString();
        return false;
    }
    if (!document.isObject()) {
        if (errorMessage) *errorMessage = "The JSON root must be an object.";
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value("schemaVersion").toInt(-1) != 1
        || !root.value("locations").isArray() || !root.value("roads").isArray()) {
        if (errorMessage) *errorMessage = "Expected SmartCity schemaVersion 1 with locations and roads arrays.";
        return false;
    }

    algorithms::WeightedGraph candidate;
    const QJsonArray locations = root.value("locations").toArray();
    for (qsizetype index = 0; index < locations.size(); ++index) {
        if (!locations.at(index).isObject()) {
            if (errorMessage) *errorMessage = describeIndex("location", index, "expected an object");
            return false;
        }
        const auto object = locations.at(index).toObject();
        core::CityLocation location;
        const QString name = object.value("name").toString().trimmed();
        const QString category = object.value("category").toString();
        if (!integerField(object, "id", location.id) || name.isEmpty()
            || !numberField(object, "x", location.x, false)
            || !numberField(object, "y", location.y, false)
            || !core::categoryFromName(category.toStdString(), location.category)) {
            if (errorMessage) *errorMessage = describeIndex("location", index, "invalid id, name, coordinates, or category");
            return false;
        }
        location.name = name.toStdString();
        if (!candidate.addLocation(location)) {
            if (errorMessage) *errorMessage = describeIndex("location", index, "duplicate or invalid location");
            return false;
        }
    }

    const QJsonArray roads = root.value("roads").toArray();
    for (qsizetype index = 0; index < roads.size(); ++index) {
        if (!roads.at(index).isObject()) {
            if (errorMessage) *errorMessage = describeIndex("road", index, "expected an object");
            return false;
        }
        const auto object = roads.at(index).toObject();
        core::Road road;
        const QString direction = object.value("direction").toString();
        if (!integerField(object, "id", road.id)
            || !integerField(object, "from", road.fromLocationId)
            || !integerField(object, "to", road.toLocationId)
            || !numberField(object, "distance", road.distance, true)
            || !numberField(object, "travelTime", road.travelTime, true)
            || !numberField(object, "constructionCost", road.constructionCost, true)
            || !object.value("closed").isBool()
            || !core::directionFromName(direction.toStdString(), road.direction)) {
            if (errorMessage) *errorMessage = describeIndex("road", index, "invalid fields or negative weight");
            return false;
        }
        road.closed = object.value("closed").toBool();
        if (!candidate.addRoad(road)) {
            if (errorMessage) *errorMessage = describeIndex("road", index, "duplicate id, bad endpoint, self-loop, or invalid weight");
            return false;
        }
    }

    const auto constructionValue = root.value("constructionCandidates");
    if (!constructionValue.isUndefined() && !constructionValue.isArray()) {
        if (errorMessage) *errorMessage = "constructionCandidates must be an array when present.";
        return false;
    }
    const QJsonArray constructionCandidates = constructionValue.toArray();
    for (qsizetype index = 0; index < constructionCandidates.size(); ++index) {
        if (!constructionCandidates.at(index).isObject()) {
            if (errorMessage) *errorMessage = describeIndex("construction candidate", index, "expected an object");
            return false;
        }
        const auto object = constructionCandidates.at(index).toObject();
        core::ConstructionCandidate constructionCandidate;
        if (!integerField(object, "id", constructionCandidate.id)
            || !integerField(object, "first", constructionCandidate.firstLocationId)
            || !integerField(object, "second", constructionCandidate.secondLocationId)
            || !numberField(object, "constructionCost", constructionCandidate.constructionCost, true)) {
            if (errorMessage) *errorMessage = describeIndex(
                "construction candidate", index, "invalid fields or negative cost");
            return false;
        }
        if (!candidate.addConstructionCandidate(constructionCandidate)) {
            if (errorMessage) *errorMessage = describeIndex(
                "construction candidate", index,
                "duplicate id, bad endpoint, self-loop, or invalid cost");
            return false;
        }
    }

    graph.replaceWith(candidate);
    return true;
}

} // namespace smartcity::services
