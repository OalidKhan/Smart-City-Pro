#include "algorithms/InfrastructureAlgorithms.h"
#include "algorithms/RouteAlgorithms.h"
#include "algorithms/SortingAlgorithms.h"
#include "algorithms/WeightedGraph.h"
#include "services/BenchmarkSuite.h"
#include "services/CityJsonSerializer.h"
#include "services/CityRepository.h"
#include "services/DeliveryJsonSerializer.h"
#include "services/DeliveryScenario.h"
#include "services/DispatchJsonSerializer.h"
#include "services/EmergencyDispatchSimulation.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QTimer>
#include <QtConcurrent>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void writeMalformed(const QString& path)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "malformed fixture should open");
    file.write("{ this is not valid JSON");
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary integration directory should exist");
    smartcity::services::CityRepository source;
    QString error;

    const QString cityPath = temporary.filePath("city.json");
    require(smartcity::services::CityJsonSerializer::save(cityPath, source.city(), &error),
            "city save should succeed");
    smartcity::algorithms::WeightedGraph loadedCity;
    require(smartcity::services::CityJsonSerializer::load(cityPath, loadedCity, &error),
            "city load should succeed");
    require(loadedCity.vertexCount() == source.city().vertexCount()
                && loadedCity.roadCount() == source.city().roadCount()
                && loadedCity.constructionCandidateCount()
                    == source.city().constructionCandidateCount()
                && loadedCity.location(0)->name == source.city().location(0)->name
                && loadedCity.road(6)->closed == source.city().road(6)->closed
                && loadedCity.constructionCandidate(7)->constructionCost
                    == source.city().constructionCandidate(7)->constructionCost,
            "city round trip should preserve record counts");

    const QString dispatchPath = temporary.filePath("dispatch.json");
    require(smartcity::services::DispatchJsonSerializer::save(
                dispatchPath, source.dispatchSimulation(), &error),
            "dispatch save should succeed");
    smartcity::services::EmergencyDispatchSimulation loadedDispatch;
    require(smartcity::services::DispatchJsonSerializer::load(
                dispatchPath, loadedCity, loadedDispatch, &error),
            "dispatch load should succeed");
    require(loadedDispatch.ambulances().size() == source.dispatchSimulation().ambulances().size()
                && loadedDispatch.hospitals().size() == source.dispatchSimulation().hospitals().size()
                && loadedDispatch.ambulances().at(0).currentLocationId
                    == source.dispatchSimulation().ambulances().at(0).currentLocationId,
            "dispatch round trip should preserve resources");

    const QString deliveryPath = temporary.filePath("delivery.json");
    require(smartcity::services::DeliveryJsonSerializer::save(
                deliveryPath, source.deliveryScenario(), &error),
            "delivery save should succeed");
    smartcity::services::DeliveryScenario loadedDelivery;
    require(smartcity::services::DeliveryJsonSerializer::load(
                deliveryPath, loadedCity, loadedDelivery, &error),
            "delivery load should succeed");
    require(loadedDelivery.packages().size() == source.deliveryScenario().packages().size()
                && loadedDelivery.depotLocationId() == source.deliveryScenario().depotLocationId()
                && loadedDelivery.vehicleCapacity() == source.deliveryScenario().vehicleCapacity()
                && loadedDelivery.packages().at(2).value
                    == source.deliveryScenario().packages().at(2).value,
            "delivery round trip should preserve scenario data");

    const auto cityCountBeforeMalformed = loadedCity.vertexCount();
    const auto dispatchCountBeforeMalformed = loadedDispatch.ambulances().size();
    const auto deliveryCountBeforeMalformed = loadedDelivery.packages().size();
    const int deliveryDepotBeforeMalformed = loadedDelivery.depotLocationId();
    const QString malformed = temporary.filePath("malformed.json");
    writeMalformed(malformed);
    require(!smartcity::services::CityJsonSerializer::load(malformed, loadedCity, &error)
                && loadedCity.vertexCount() == cityCountBeforeMalformed,
            "malformed city JSON should not replace current data");
    require(!smartcity::services::DispatchJsonSerializer::load(
                malformed, loadedCity, loadedDispatch, &error)
                && loadedDispatch.ambulances().size() == dispatchCountBeforeMalformed,
            "malformed dispatch JSON should not replace current data");
    require(!smartcity::services::DeliveryJsonSerializer::load(
                malformed, loadedCity, loadedDelivery, &error)
                && loadedDelivery.packages().size() == deliveryCountBeforeMalformed
                && loadedDelivery.depotLocationId() == deliveryDepotBeforeMalformed,
            "malformed delivery JSON should not replace current data");

    require(smartcity::algorithms::SortingAlgorithms::sort(
                {3.0, 1.0, 2.0}, smartcity::algorithms::SortAlgorithm::Bubble,
                smartcity::algorithms::SortOrder::Ascending, false).events.empty(),
            "benchmark sorting should disable event recording");
    require(smartcity::algorithms::RouteAlgorithms::dijkstra(
                loadedCity, 0, 1, smartcity::algorithms::RouteMetric::TravelTime,
                false).events.empty(),
            "benchmark Dijkstra should disable event recording");
    require(smartcity::algorithms::InfrastructureAlgorithms::prim(
                loadedCity, false).events.empty(),
            "benchmark Prim should disable event recording");

    smartcity::services::BenchmarkConfig config;
    config.sortingDatasetSize = 800;
    config.randomSeed = 12345;
    config.warmupRuns = 1;
    config.measuredRuns = 3;
    QFutureWatcher<smartcity::services::BenchmarkReport> watcher;
    QEventLoop loop;
    QTimer heartbeat;
    heartbeat.setInterval(0);
    int eventLoopTicks = 0;
    QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++eventLoopTicks; });
    QObject::connect(&watcher, &QFutureWatcher<smartcity::services::BenchmarkReport>::finished,
                     &loop, &QEventLoop::quit);
    heartbeat.start();
    const auto snapshot = loadedCity;
    watcher.setFuture(QtConcurrent::run([snapshot, config] {
        return smartcity::services::BenchmarkSuite::run(snapshot, config);
    }));
    loop.exec();
    heartbeat.stop();
    const auto benchmark = watcher.result();
    require(eventLoopTicks > 0, "event loop should remain responsive during benchmark work");
    require(benchmark.records.size() == 10
                && benchmark.sortingAgreement
                && benchmark.mstAgreement
                && benchmark.shortestPathAgreement,
            "benchmark report should contain actual agreeing workload results");

    std::cout << "All SmartCity Qt integration tests passed.\n";
    return 0;
}
