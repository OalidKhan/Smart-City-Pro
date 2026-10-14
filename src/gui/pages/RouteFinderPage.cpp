#include "gui/pages/RouteFinderPage.h"

#include "algorithms/AlgorithmEvent.h"
#include "algorithms/WeightedGraph.h"
#include "core/models/Road.h"
#include "gui/animation/AlgorithmAnimationController.h"
#include "services/CityRepository.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <vector>

namespace {

using smartcity::algorithms::AlgorithmEvent;
using smartcity::algorithms::AlgorithmEventType;
using smartcity::algorithms::RouteMetric;
using smartcity::algorithms::RouteStatus;
using smartcity::algorithms::TraversalKind;

QString locationText(const smartcity::algorithms::WeightedGraph& graph, int id)
{
    const auto* location = graph.location(id);
    return location ? QString("L%1 — %2").arg(id).arg(QString::fromStdString(location->name))
                    : QString("L%1").arg(id);
}

QString eventDescription(const AlgorithmEvent& event)
{
    switch (event.type) {
    case AlgorithmEventType::Started: return QString("Started at L%1").arg(event.locationId);
    case AlgorithmEventType::NodeVisited: return QString("Visited L%1").arg(event.locationId);
    case AlgorithmEventType::EdgeExplored:
        return QString("Explored road R%1: L%2 → L%3")
            .arg(event.roadId).arg(event.fromLocationId).arg(event.toLocationId);
    case AlgorithmEventType::DistanceUpdated:
        return QString("Updated best weight for L%1 to %2").arg(event.locationId).arg(event.value, 0, 'f', 2);
    case AlgorithmEventType::FinalRouteNode: return QString("Final route includes L%1").arg(event.locationId);
    case AlgorithmEventType::FinalRouteEdge: return QString("Final route includes road R%1").arg(event.roadId);
    case AlgorithmEventType::Finished: return "Animation complete";
    }
    return {};
}

} // namespace

RouteFinderPage::RouteFinderPage(smartcity::services::CityRepository& repository, QWidget* parent)
    : QWidget(parent), repository_(repository), startCombo_(new QComboBox(this)),
      destinationCombo_(new QComboBox(this)), metricCombo_(new QComboBox(this)),
      scene_(new QGraphicsScene(this)), view_(new QGraphicsView(scene_, this)),
      orderedList_(new QListWidget(this)), resultLabel_(new QLabel(this)),
      eventLabel_(new QLabel("Prepare an algorithm to begin.", this)),
      speedLabel_(new QLabel("Speed: 1.0×", this)),
      animation_(new AlgorithmAnimationController(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(10);
    auto* title = new QLabel("Shortest-Route Finder", this);
    title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "Compare distance or travel-time routes with Dijkstra, or demonstrate directed reachability using BFS and DFS.", this);
    description->setObjectName("pageDescription");
    description->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(description);

    auto* controls = new QFrame(this);
    controls->setObjectName("routePanel");
    auto* grid = new QGridLayout(controls);
    grid->setContentsMargins(14, 12, 14, 12);
    grid->addWidget(new QLabel("Start", controls), 0, 0);
    grid->addWidget(startCombo_, 0, 1);
    grid->addWidget(new QLabel("Destination", controls), 0, 2);
    grid->addWidget(destinationCombo_, 0, 3);
    metricCombo_->addItem("Minimize distance", static_cast<int>(RouteMetric::Distance));
    metricCombo_->addItem("Minimize travel time", static_cast<int>(RouteMetric::TravelTime));
    grid->addWidget(metricCombo_, 0, 4);
    auto* routeButton = new QPushButton("Find shortest route", controls);
    routeButton->setObjectName("primaryButton");
    grid->addWidget(routeButton, 0, 5);
    auto* bfsButton = new QPushButton("Run BFS", controls);
    auto* dfsButton = new QPushButton("Run DFS", controls);
    bfsButton->setObjectName("editorButton"); dfsButton->setObjectName("editorButton");
    grid->addWidget(bfsButton, 1, 4); grid->addWidget(dfsButton, 1, 5);
    auto* caveat = new QLabel(
        "BFS minimizes edge count only for shortest paths on an unweighted graph; DFS does not guarantee a shortest path.", controls);
    caveat->setObjectName("algorithmCaveat"); caveat->setWordWrap(true);
    grid->addWidget(caveat, 1, 0, 1, 4);
    root->addWidget(controls);

    auto* content = new QHBoxLayout;
    view_->setObjectName("routeView");
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setMinimumHeight(340);
    scene_->setBackgroundBrush(QColor("#f8fafc"));
    content->addWidget(view_, 1);
    auto* resultPanel = new QFrame(this);
    resultPanel->setObjectName("routePanel");
    resultPanel->setFixedWidth(245);
    auto* resultLayout = new QVBoxLayout(resultPanel);
    auto* resultTitle = new QLabel("Result", resultPanel); resultTitle->setObjectName("sectionTitle");
    resultLabel_->setWordWrap(true); resultLabel_->setObjectName("routeResult");
    orderedList_->setObjectName("routeList");
    resultLayout->addWidget(resultTitle); resultLayout->addWidget(resultLabel_);
    resultLayout->addWidget(new QLabel("Ordered locations", resultPanel));
    resultLayout->addWidget(orderedList_, 1);
    content->addWidget(resultPanel);
    root->addLayout(content, 1);

    auto* animationBar = new QHBoxLayout;
    const std::vector<std::pair<QString, std::function<void()>>> animationActions = {
        {"Play", [this] { animation_->play(); }}, {"Pause", [this] { animation_->pause(); }},
        {"Step", [this] { animation_->step(); }}, {"Reset", [this] { animation_->reset(); }}
    };
    for (const auto& action : animationActions) {
        auto* button = new QPushButton(action.first, this); button->setObjectName("editorButton");
        connect(button, &QPushButton::clicked, this, action.second); animationBar->addWidget(button);
    }
    auto* speed = new QSlider(Qt::Horizontal, this); speed->setRange(1, 10); speed->setValue(5);
    speed->setMaximumWidth(150);
    animationBar->addSpacing(10); animationBar->addWidget(speedLabel_); animationBar->addWidget(speed);
    eventLabel_->setObjectName("mapSummary"); animationBar->addSpacing(12); animationBar->addWidget(eventLabel_, 1);
    root->addLayout(animationBar);

    connect(routeButton, &QPushButton::clicked, this, [this] { runDijkstra(); });
    connect(bfsButton, &QPushButton::clicked, this, [this] { runTraversal(TraversalKind::BreadthFirst); });
    connect(dfsButton, &QPushButton::clicked, this, [this] { runTraversal(TraversalKind::DepthFirst); });
    connect(speed, &QSlider::valueChanged, this, [this](int value) {
        const double multiplier = value / 5.0;
        speedLabel_->setText(QString("Speed: %1×").arg(multiplier, 0, 'f', 1));
        animation_->setIntervalMilliseconds(1050 - value * 90);
    });
    animation_->setCallbacks(
        [this](const AlgorithmEvent& event) { applyEvent(event); },
        [this] { resetVisualization(); },
        [this] { eventLabel_->setText("Animation complete."); });

    refreshFromGraph(true);
}

void RouteFinderPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshFromGraph();
}

int RouteFinderPage::selectedLocationId(const QComboBox* combo) const
{
    return combo->currentIndex() < 0 ? -1 : combo->currentData().toInt();
}

void RouteFinderPage::refreshFromGraph(bool force)
{
    const auto& graph = repository_.city();
    if (!force && observedRevision_ == graph.revision()) return;
    animation_->setEvents({});
    const int oldStart = selectedLocationId(startCombo_);
    const int oldDestination = selectedLocationId(destinationCombo_);
    startCombo_->clear(); destinationCombo_->clear();
    std::vector<int> ids; for (const auto& entry : graph.locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    for (const int id : ids) {
        const QString text = locationText(graph, id);
        startCombo_->addItem(text, id); destinationCombo_->addItem(text, id);
    }
    if (startCombo_->findData(oldStart) >= 0) startCombo_->setCurrentIndex(startCombo_->findData(oldStart));
    if (destinationCombo_->findData(oldDestination) >= 0)
        destinationCombo_->setCurrentIndex(destinationCombo_->findData(oldDestination));
    else if (destinationCombo_->count() > 1) destinationCombo_->setCurrentIndex(1);
    observedRevision_ = graph.revision();
    orderedList_->clear(); resultLabel_->setText("Choose locations and run an algorithm.");
    eventLabel_->setText(QString("Graph revision %1 loaded.").arg(static_cast<qulonglong>(observedRevision_)));
    rebuildMap();
}

void RouteFinderPage::rebuildMap()
{
    scene_->clear(); nodeItems_.clear(); roadItems_.clear();
    const auto& graph = repository_.city();
    for (const auto& entry : graph.roads()) {
        const auto& road = entry.second;
        const auto* from = graph.location(road.fromLocationId); const auto* to = graph.location(road.toLocationId);
        if (!from || !to) continue;
        auto* line = scene_->addLine(from->x, from->y, to->x, to->y,
            QPen(road.closed ? QColor("#ef4444") : QColor("#cbd5e1"), 4.0,
                 road.closed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap));
        line->setZValue(0); roadItems_.emplace(road.id, line);
        auto* label = scene_->addSimpleText(QString("R%1 %2")
            .arg(road.id).arg(road.direction == smartcity::core::RoadDirection::OneWay
                ? QString::fromUtf8("→") : QString::fromUtf8("↔")));
        label->setBrush(QBrush(QColor(road.closed ? "#dc2626" : "#64748b")));
        label->setPos((from->x + to->x) / 2.0 + 5.0, (from->y + to->y) / 2.0 - 18.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    }
    for (const auto& entry : graph.locations()) {
        const auto& location = entry.second;
        auto* node = scene_->addEllipse(location.x - 10.0, location.y - 10.0, 20.0, 20.0,
                                        QPen(Qt::white, 3.0), QBrush(QColor("#475569")));
        node->setZValue(2); nodeItems_.emplace(location.id, node);
        auto* label = scene_->addSimpleText(QString("%1 [L%2]")
            .arg(QString::fromStdString(location.name)).arg(location.id));
        label->setBrush(QBrush(QColor("#0f172a"))); label->setPos(location.x + 13.0, location.y - 14.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations); label->setZValue(3);
    }
    if (!scene_->items().isEmpty())
        view_->fitInView(scene_->itemsBoundingRect().adjusted(-45, -45, 45, 45), Qt::KeepAspectRatio);
}

void RouteFinderPage::resetVisualization()
{
    rebuildMap();
    eventLabel_->setText("Animation reset. Press Play or Step.");
}

void RouteFinderPage::applyEvent(const AlgorithmEvent& event)
{
    eventLabel_->setText(eventDescription(event));
    if (event.type == AlgorithmEventType::NodeVisited || event.type == AlgorithmEventType::DistanceUpdated) {
        const auto item = nodeItems_.find(event.locationId);
        if (item != nodeItems_.end()) item->second->setBrush(QBrush(
            event.type == AlgorithmEventType::NodeVisited ? QColor("#f59e0b") : QColor("#38bdf8")));
    } else if (event.type == AlgorithmEventType::EdgeExplored) {
        const auto item = roadItems_.find(event.roadId);
        if (item != roadItems_.end()) item->second->setPen(QPen(QColor("#f59e0b"), 5.0, Qt::SolidLine, Qt::RoundCap));
    } else if (event.type == AlgorithmEventType::FinalRouteNode) {
        const auto item = nodeItems_.find(event.locationId);
        if (item != nodeItems_.end()) item->second->setBrush(QBrush(QColor("#2563eb")));
    } else if (event.type == AlgorithmEventType::FinalRouteEdge) {
        const auto item = roadItems_.find(event.roadId);
        if (item != roadItems_.end()) item->second->setPen(QPen(QColor("#2563eb"), 7.0, Qt::SolidLine, Qt::RoundCap));
    }
}

void RouteFinderPage::runDijkstra()
{
    refreshFromGraph();
    const int start = selectedLocationId(startCombo_); const int destination = selectedLocationId(destinationCombo_);
    const auto metric = static_cast<RouteMetric>(metricCombo_->currentData().toInt());
    const auto result = smartcity::algorithms::RouteAlgorithms::dijkstra(
        repository_.city(), start, destination, metric);
    showRouteResult(result, metric);
    animation_->setEvents(result.events);
    animation_->play();
}

void RouteFinderPage::runTraversal(TraversalKind kind)
{
    refreshFromGraph();
    const auto result = smartcity::algorithms::RouteAlgorithms::traverse(
        repository_.city(), selectedLocationId(startCombo_), selectedLocationId(destinationCombo_), kind);
    showTraversalResult(result, kind);
    animation_->setEvents(result.events);
    animation_->play();
}

void RouteFinderPage::showRouteResult(const smartcity::algorithms::RouteResult& result, RouteMetric metric)
{
    orderedList_->clear();
    if (result.status == RouteStatus::InvalidInput) {
        resultLabel_->setText("Select valid start and destination locations."); return;
    }
    if (result.status == RouteStatus::Unreachable) {
        resultLabel_->setText("Destination is unreachable using open roads in the allowed direction."); return;
    }
    for (std::size_t index = 0; index < result.locationIds.size(); ++index) {
        QString text = QString("%1. %2").arg(static_cast<qulonglong>(index + 1))
            .arg(locationText(repository_.city(), result.locationIds[index]));
        if (index < result.roadIds.size()) text += QString("   via R%1").arg(result.roadIds[index]);
        orderedList_->addItem(text);
    }
    const QString unit = metric == RouteMetric::Distance ? "km" : "minutes";
    if (result.status == RouteStatus::Trivial)
        resultLabel_->setText(QString("Start equals destination. Total: 0 %1.").arg(unit));
    else
        resultLabel_->setText(QString("Shortest route found. Total: %1 %2.")
                                  .arg(result.totalWeight, 0, 'f', 2).arg(unit));
}

void RouteFinderPage::showTraversalResult(const smartcity::algorithms::TraversalResult& result,
                                          TraversalKind kind)
{
    orderedList_->clear();
    if (!result.validInput) { resultLabel_->setText("Select valid locations first."); return; }
    for (std::size_t index = 0; index < result.visitOrder.size(); ++index)
        orderedList_->addItem(QString("%1. %2").arg(static_cast<qulonglong>(index + 1))
            .arg(locationText(repository_.city(), result.visitOrder[index])));
    const QString algorithm = kind == TraversalKind::BreadthFirst ? "BFS" : "DFS";
    resultLabel_->setText(QString("%1 traversal: destination is %2. %3")
        .arg(algorithm).arg(result.destinationReachable ? "reachable" : "not reachable")
        .arg(kind == TraversalKind::BreadthFirst
            ? "BFS minimizes edge count only in an unweighted graph." : "DFS does not minimize path weight."));
}
