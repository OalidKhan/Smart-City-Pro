#include "services/DispatchJsonSerializer.h"

#include "algorithms/WeightedGraph.h"
#include "core/models/EmergencyModels.h"
#include "services/EmergencyDispatchSimulation.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace smartcity::services {
namespace {

bool integerField(const QJsonObject& object, const char* name, int& value, int minimum = 0)
{
    const auto item = object.value(QLatin1String(name));
    if (!item.isDouble() || !std::isfinite(item.toDouble())
        || std::floor(item.toDouble()) != item.toDouble()
        || item.toDouble() < minimum || item.toDouble() > std::numeric_limits<int>::max()) return false;
    value = item.toInt(minimum - 1);
    return value >= minimum;
}

bool numberField(const QJsonObject& object, const char* name, double& value, double minimum)
{
    const auto item = object.value(QLatin1String(name));
    if (!item.isDouble() || !std::isfinite(item.toDouble()) || item.toDouble() < minimum) return false;
    value = item.toDouble();
    return true;
}

QString indexedError(const char* collection, qsizetype index, const QString& detail)
{
    return QString("Invalid %1 at index %2: %3")
        .arg(QLatin1String(collection)).arg(index).arg(detail);
}

} // namespace

bool DispatchJsonSerializer::save(const QString& fileName,
                                  const EmergencyDispatchSimulation& simulation,
                                  QString* errorMessage)
{
    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("simulationClockMinutes", simulation.clockMinutes());

    std::vector<int> ambulanceIds;
    for (const auto& entry : simulation.ambulances()) ambulanceIds.push_back(entry.first);
    std::sort(ambulanceIds.begin(), ambulanceIds.end());
    QJsonArray ambulances;
    for (const int id : ambulanceIds) {
        const auto& ambulance = simulation.ambulances().at(id);
        QJsonObject object;
        object.insert("id", ambulance.id);
        object.insert("currentLocation", ambulance.currentLocationId);
        object.insert("available", ambulance.available);
        object.insert("assignedRequest", ambulance.assignedRequestId);
        ambulances.append(object);
    }
    root.insert("ambulances", ambulances);

    std::vector<int> hospitalIds;
    for (const auto& entry : simulation.hospitals()) hospitalIds.push_back(entry.first);
    std::sort(hospitalIds.begin(), hospitalIds.end());
    QJsonArray hospitals;
    for (const int locationId : hospitalIds) {
        const auto& hospital = simulation.hospitals().at(locationId);
        QJsonObject object;
        object.insert("location", hospital.locationId);
        object.insert("availableBeds", hospital.availableBeds);
        QJsonArray services;
        for (const auto service : hospital.supportedServices)
            services.append(QLatin1String(core::emergencyServiceName(service)));
        object.insert("supportedServices", services);
        hospitals.append(object);
    }
    root.insert("hospitals", hospitals);

    std::vector<int> requestIds;
    for (const auto& entry : simulation.requests()) requestIds.push_back(entry.first);
    std::sort(requestIds.begin(), requestIds.end());
    QJsonArray requests;
    for (const int id : requestIds) {
        const auto& request = simulation.requests().at(id);
        QJsonObject object;
        object.insert("id", request.id);
        object.insert("location", request.locationId);
        object.insert("priority", QLatin1String(core::priorityName(request.priority)));
        object.insert("creationTimeMinutes", request.creationTimeMinutes);
        object.insert("requiredService", QLatin1String(core::emergencyServiceName(request.requiredService)));
        object.insert("status", QLatin1String(core::requestStatusName(request.status)));
        object.insert("assignedAmbulance", request.assignedAmbulanceId);
        object.insert("assignedHospital", request.assignedHospitalLocationId);
        object.insert("responseTimeMinutes", request.responseTimeMinutes);
        object.insert("transportTimeMinutes", request.transportTimeMinutes);
        object.insert("accumulatedTransportMinutes", request.accumulatedTransportMinutes);
        object.insert("note", QString::fromStdString(request.note));
        requests.append(object);
    }
    root.insert("requests", requests);

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

bool DispatchJsonSerializer::load(const QString& fileName,
                                  const algorithms::WeightedGraph& graph,
                                  EmergencyDispatchSimulation& simulation,
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
        if (errorMessage) *errorMessage = "The dispatch JSON root must be an object.";
        return false;
    }
    const auto root = document.object();
    double clock = 0.0;
    if (root.value("schemaVersion").toInt(-1) != 1
        || !numberField(root, "simulationClockMinutes", clock, 0.0)
        || !root.value("ambulances").isArray()
        || !root.value("hospitals").isArray()
        || !root.value("requests").isArray()) {
        if (errorMessage) *errorMessage = "Expected dispatch schemaVersion 1 and complete data arrays.";
        return false;
    }

    std::vector<core::Ambulance> ambulances;
    const auto ambulanceArray = root.value("ambulances").toArray();
    for (qsizetype index = 0; index < ambulanceArray.size(); ++index) {
        if (!ambulanceArray.at(index).isObject()) {
            if (errorMessage) *errorMessage = indexedError("ambulance", index, "expected an object");
            return false;
        }
        const auto object = ambulanceArray.at(index).toObject();
        core::Ambulance ambulance;
        if (!integerField(object, "id", ambulance.id)
            || !integerField(object, "currentLocation", ambulance.currentLocationId)
            || !object.value("available").isBool()
            || !integerField(object, "assignedRequest", ambulance.assignedRequestId, -1)) {
            if (errorMessage) *errorMessage = indexedError("ambulance", index, "invalid fields");
            return false;
        }
        ambulance.available = object.value("available").toBool();
        ambulances.push_back(ambulance);
    }

    std::vector<core::HospitalResource> hospitals;
    const auto hospitalArray = root.value("hospitals").toArray();
    for (qsizetype index = 0; index < hospitalArray.size(); ++index) {
        if (!hospitalArray.at(index).isObject()) {
            if (errorMessage) *errorMessage = indexedError("hospital", index, "expected an object");
            return false;
        }
        const auto object = hospitalArray.at(index).toObject();
        core::HospitalResource hospital;
        if (!integerField(object, "location", hospital.locationId)
            || !integerField(object, "availableBeds", hospital.availableBeds)
            || !object.value("supportedServices").isArray()) {
            if (errorMessage) *errorMessage = indexedError("hospital", index, "invalid fields");
            return false;
        }
        for (const auto& value : object.value("supportedServices").toArray()) {
            core::EmergencyService service;
            if (!value.isString()
                || !core::emergencyServiceFromName(value.toString().toStdString(), service)) {
                if (errorMessage) *errorMessage = indexedError("hospital", index, "unknown service");
                return false;
            }
            hospital.supportedServices.push_back(service);
        }
        hospitals.push_back(hospital);
    }

    std::vector<core::EmergencyRequest> requests;
    const auto requestArray = root.value("requests").toArray();
    for (qsizetype index = 0; index < requestArray.size(); ++index) {
        if (!requestArray.at(index).isObject()) {
            if (errorMessage) *errorMessage = indexedError("request", index, "expected an object");
            return false;
        }
        const auto object = requestArray.at(index).toObject();
        core::EmergencyRequest request;
        if (!integerField(object, "id", request.id)
            || !integerField(object, "location", request.locationId)
            || !numberField(object, "creationTimeMinutes", request.creationTimeMinutes, 0.0)
            || !integerField(object, "assignedAmbulance", request.assignedAmbulanceId, -1)
            || !integerField(object, "assignedHospital", request.assignedHospitalLocationId, -1)
            || !numberField(object, "responseTimeMinutes", request.responseTimeMinutes, -1.0)
            || !numberField(object, "transportTimeMinutes", request.transportTimeMinutes, -1.0)
            || !numberField(object, "accumulatedTransportMinutes", request.accumulatedTransportMinutes, 0.0)
            || !core::priorityFromName(object.value("priority").toString().toStdString(), request.priority)
            || !core::emergencyServiceFromName(
                object.value("requiredService").toString().toStdString(), request.requiredService)
            || !core::requestStatusFromName(
                object.value("status").toString().toStdString(), request.status)
            || !object.value("note").isString()) {
            if (errorMessage) *errorMessage = indexedError("request", index, "invalid fields or enum value");
            return false;
        }
        request.note = object.value("note").toString().toStdString();
        requests.push_back(request);
    }

    std::string validationError;
    if (!simulation.replaceData(clock, ambulances, hospitals, requests, graph, &validationError)) {
        if (errorMessage) *errorMessage = QString::fromStdString(validationError)
            + " The current simulation was not changed.";
        return false;
    }
    return true;
}

} // namespace smartcity::services
