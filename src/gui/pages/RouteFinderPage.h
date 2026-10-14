#pragma once

#include "algorithms/RouteAlgorithms.h"

#include <QWidget>

#include <cstdint>
#include <unordered_map>

class AlgorithmAnimationController;
class QComboBox;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QListWidget;
class QShowEvent;
class QSlider;

namespace smartcity::services { class CityRepository; }

class RouteFinderPage final : public QWidget {
public:
    explicit RouteFinderPage(smartcity::services::CityRepository& repository,
                             QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refreshFromGraph(bool force = false);
    void rebuildMap();
    void resetVisualization();
    void applyEvent(const smartcity::algorithms::AlgorithmEvent& event);
    void runDijkstra();
    void runTraversal(smartcity::algorithms::TraversalKind kind);
    void showRouteResult(const smartcity::algorithms::RouteResult& result,
                         smartcity::algorithms::RouteMetric metric);
    void showTraversalResult(const smartcity::algorithms::TraversalResult& result,
                             smartcity::algorithms::TraversalKind kind);
    int selectedLocationId(const QComboBox* combo) const;

    smartcity::services::CityRepository& repository_;
    QComboBox* startCombo_;
    QComboBox* destinationCombo_;
    QComboBox* metricCombo_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QListWidget* orderedList_;
    QLabel* resultLabel_;
    QLabel* eventLabel_;
    QLabel* speedLabel_;
    AlgorithmAnimationController* animation_;
    std::uint64_t observedRevision_{static_cast<std::uint64_t>(-1)};
    std::unordered_map<int, QGraphicsEllipseItem*> nodeItems_;
    std::unordered_map<int, QGraphicsLineItem*> roadItems_;
};
