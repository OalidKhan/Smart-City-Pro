#include "gui/pages/AllPairsRoutesPage.h"

#include "algorithms/WeightedGraph.h"
#include "core/models/Road.h"
#include "services/CityRepository.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QFutureWatcher>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace {

using smartcity::algorithms::AllPairsResult;
using smartcity::algorithms::RouteMetric;

QString locationLabel(const smartcity::algorithms::WeightedGraph& graph, int id)
{
    const auto* location = graph.location(id);
    return location ? QString("L%1 — %2").arg(id).arg(QString::fromStdString(location->name))
                    : QString("L%1").arg(id);
}

QString matrixHeader(const smartcity::algorithms::WeightedGraph& graph, int id)
{
    const auto* location = graph.location(id);
    return location ? QString("L%1\n%2").arg(id).arg(QString::fromStdString(location->name))
                    : QString("L%1").arg(id);
}

} // namespace

AllPairsRoutesPage::AllPairsRoutesPage(smartcity::services::CityRepository& repository,
                                       QWidget* parent)
    : QWidget(parent), repository_(repository), modeCombo_(new QComboBox(this)),
      sourceCombo_(new QComboBox(this)), destinationCombo_(new QComboBox(this)),
      computeButton_(new QPushButton("Compute matrix", this)),
      progressBar_(new QProgressBar(this)), matrixTable_(new QTableWidget(this)),
      scene_(new QGraphicsScene(this)), view_(new QGraphicsView(scene_, this)),
      statusLabel_(new QLabel(this)), routeLabel_(new QLabel(this)),
      watcher_(new QFutureWatcher<AllPairsResult>(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(10);
    auto* title = new QLabel("All-Pairs Route Planner", this);
    title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "Floyd–Warshall computes every open, direction-valid route from an immutable graph snapshot. Results are cached by graph revision and weight mode.", this);
    description->setObjectName("pageDescription");
    description->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(description);

    auto* controls = new QFrame(this);
    controls->setObjectName("routePanel");
    auto* grid = new QGridLayout(controls);
    grid->setContentsMargins(14, 12, 14, 12);
    modeCombo_->addItem("Distance (km)", static_cast<int>(RouteMetric::Distance));
    modeCombo_->addItem("Travel time (minutes)", static_cast<int>(RouteMetric::TravelTime));
    computeButton_->setObjectName("primaryButton");
    progressBar_->setRange(0, 0);
    progressBar_->setMaximumWidth(130);
    progressBar_->hide();
    grid->addWidget(new QLabel("Weight mode", controls), 0, 0);
    grid->addWidget(modeCombo_, 0, 1);
    grid->addWidget(computeButton_, 0, 2);
    grid->addWidget(progressBar_, 0, 3);
    grid->addWidget(new QLabel("Source", controls), 1, 0);
    grid->addWidget(sourceCombo_, 1, 1);
    grid->addWidget(new QLabel("Destination", controls), 1, 2);
    grid->addWidget(destinationCombo_, 1, 3);
    statusLabel_->setObjectName("mapSummary");
    statusLabel_->setWordWrap(true);
    grid->addWidget(statusLabel_, 2, 0, 1, 4);
    root->addWidget(controls);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    matrixTable_->setObjectName("matrixTable");
    matrixTable_->setAlternatingRowColors(true);
    matrixTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    matrixTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    matrixTable_->setSelectionBehavior(QAbstractItemView::SelectItems);
    matrixTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    matrixTable_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    splitter->addWidget(matrixTable_);

    auto* mapPanel = new QFrame(splitter);
    mapPanel->setObjectName("routePanel");
    auto* mapLayout = new QVBoxLayout(mapPanel);
    auto* mapTitle = new QLabel("Selected route", mapPanel);
    mapTitle->setObjectName("sectionTitle");
    routeLabel_->setObjectName("routeResult");
    routeLabel_->setWordWrap(true);
    view_->setObjectName("allPairsView");
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setMinimumSize(330, 300);
    mapLayout->addWidget(mapTitle);
    mapLayout->addWidget(routeLabel_);
    mapLayout->addWidget(view_, 1);
    splitter->addWidget(mapPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    connect(computeButton_, &QPushButton::clicked, this, [this] { requestComputation(); });
    connect(modeCombo_, &QComboBox::currentIndexChanged, this, [this] { activateCachedResult(); });
    connect(sourceCombo_, &QComboBox::currentIndexChanged, this, [this] { updateSelectedRoute(); });
    connect(destinationCombo_, &QComboBox::currentIndexChanged, this, [this] { updateSelectedRoute(); });
    connect(watcher_, &QFutureWatcher<AllPairsResult>::finished,
            this, [this] { computationFinished(); });
    refreshFromGraph(true);
}

void AllPairsRoutesPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshFromGraph();
}

int AllPairsRoutesPage::selectedLocationId(const QComboBox* combo) const
{
    return combo->currentIndex() < 0 ? -1 : combo->currentData().toInt();
}

void AllPairsRoutesPage::refreshFromGraph(bool force)
{
    const auto& graph = repository_.city();
    if (!force && observedRevision_ == graph.revision()) return;
    const int previousSource = selectedLocationId(sourceCombo_);
    const int previousDestination = selectedLocationId(destinationCombo_);
    observedRevision_ = graph.revision();
    cache_.clear();
    currentKey_.reset();
    matrixTable_->clear();
    matrixTable_->setRowCount(0);
    matrixTable_->setColumnCount(0);
    sourceCombo_->clear();
    destinationCombo_->clear();
    std::vector<int> ids;
    for (const auto& entry : graph.locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    for (const int id : ids) {
        const QString label = locationLabel(graph, id);
        sourceCombo_->addItem(label, id);
        destinationCombo_->addItem(label, id);
    }
    const int sourceIndex = sourceCombo_->findData(previousSource);
    const int destinationIndex = destinationCombo_->findData(previousDestination);
    if (sourceIndex >= 0) sourceCombo_->setCurrentIndex(sourceIndex);
    if (destinationIndex >= 0) destinationCombo_->setCurrentIndex(destinationIndex);
    else if (destinationCombo_->count() > 1) destinationCombo_->setCurrentIndex(1);
    statusLabel_->setText(QString("Graph revision %1. Compute a matrix for the selected mode.")
                              .arg(static_cast<qulonglong>(observedRevision_)));
    routeLabel_->setText("No all-pairs result is active.");
    rebuildMap();
}

void AllPairsRoutesPage::requestComputation()
{
    refreshFromGraph();
    if (watcher_->isRunning()) {
        statusLabel_->setText("An all-pairs computation is already running.");
        return;
    }
    const auto& graph = repository_.city();
    if (graph.vertexCount() == 0) {
        statusLabel_->setText("The city has no locations.");
        return;
    }
    if (graph.vertexCount() > ComputationLimit) {
        statusLabel_->setText(QString("Computation is limited to %1 locations to keep memory and runtime sensible.")
                                  .arg(static_cast<qulonglong>(ComputationLimit)));
        return;
    }
    const auto metric = static_cast<RouteMetric>(modeCombo_->currentData().toInt());
    const CacheKey key{graph.revision(), static_cast<int>(metric)};
    const auto cached = cache_.find(key);
    if (cached != cache_.end()) {
        currentKey_ = key;
        displayResult(cached->second);
        statusLabel_->setText("Loaded the cached result for this revision and weight mode.");
        return;
    }

    const auto snapshot = graph;
    computeButton_->setEnabled(false);
    progressBar_->show();
    statusLabel_->setText(QString("Computing %1 × %1 matrix from immutable graph revision %2…")
                              .arg(static_cast<qulonglong>(snapshot.vertexCount()))
                              .arg(static_cast<qulonglong>(snapshot.revision())));
    watcher_->setFuture(QtConcurrent::run([snapshot, metric] {
        return smartcity::algorithms::FloydWarshall::compute(snapshot, metric);
    }));
}

void AllPairsRoutesPage::computationFinished()
{
    computeButton_->setEnabled(true);
    progressBar_->hide();
    AllPairsResult result = watcher_->result();
    if (repository_.city().revision() != result.graphRevision) {
        statusLabel_->setText("Discarded a stale result because the city graph changed during computation.");
        return;
    }
    const CacheKey key{result.graphRevision, static_cast<int>(result.metric)};
    cache_.insert_or_assign(key, std::move(result));
    if (modeCombo_->currentData().toInt() == key.second) {
        currentKey_ = key;
        displayResult(cache_.at(key));
        statusLabel_->setText("All-pairs matrix computed and cached.");
    } else {
        statusLabel_->setText("The completed result was cached; select its weight mode to display it.");
    }
}

void AllPairsRoutesPage::activateCachedResult()
{
    refreshFromGraph();
    const CacheKey key{repository_.city().revision(), modeCombo_->currentData().toInt()};
    const auto cached = cache_.find(key);
    if (cached == cache_.end()) {
        currentKey_.reset();
        matrixTable_->clear(); matrixTable_->setRowCount(0); matrixTable_->setColumnCount(0);
        routeLabel_->setText("No result is cached for this mode. Choose Compute matrix.");
        rebuildMap();
        return;
    }
    currentKey_ = key;
    displayResult(cached->second);
    statusLabel_->setText("Loaded the cached result for this revision and weight mode.");
}

const AllPairsResult* AllPairsRoutesPage::currentResult() const
{
    if (!currentKey_) return nullptr;
    const auto found = cache_.find(*currentKey_);
    return found == cache_.end() ? nullptr : &found->second;
}

void AllPairsRoutesPage::displayResult(const AllPairsResult& result)
{
    displayMatrix(result);
    updateSelectedRoute();
}

void AllPairsRoutesPage::displayMatrix(const AllPairsResult& result)
{
    const std::size_t count = result.locationIds.size();
    matrixTable_->clear();
    if (count > MatrixVisualizationLimit) {
        matrixTable_->setRowCount(1);
        matrixTable_->setColumnCount(1);
        matrixTable_->setItem(0, 0, new QTableWidgetItem(
            QString("Matrix display is limited to %1 locations. Pair routing remains available.")
                .arg(static_cast<qulonglong>(MatrixVisualizationLimit))));
        return;
    }
    matrixTable_->setRowCount(static_cast<int>(count));
    matrixTable_->setColumnCount(static_cast<int>(count));
    QStringList headers;
    for (const int id : result.locationIds) headers.append(matrixHeader(repository_.city(), id));
    matrixTable_->setHorizontalHeaderLabels(headers);
    matrixTable_->setVerticalHeaderLabels(headers);
    for (std::size_t row = 0; row < count; ++row) {
        for (std::size_t column = 0; column < count; ++column) {
            const double value = result.weights[row][column];
            auto* item = new QTableWidgetItem(std::isfinite(value)
                ? QString::number(value, 'f', 2) : QString("Unreachable"));
            item->setTextAlignment(Qt::AlignCenter);
            if (!std::isfinite(value)) item->setForeground(QBrush(QColor("#94a3b8")));
            else if (row == column) item->setBackground(QBrush(QColor("#eff6ff")));
            matrixTable_->setItem(static_cast<int>(row), static_cast<int>(column), item);
        }
    }
}

void AllPairsRoutesPage::updateSelectedRoute()
{
    const auto* result = currentResult();
    if (!result) return;
    rebuildMap();
    const auto path = result->reconstructPath(selectedLocationId(sourceCombo_),
                                              selectedLocationId(destinationCombo_));
    if (!path.reachable) {
        routeLabel_->setText("Unreachable using open roads in the permitted direction.");
        return;
    }
    for (const int roadId : path.roadIds) {
        const auto road = roadItems_.find(roadId);
        if (road != roadItems_.end())
            road->second->setPen(QPen(QColor("#2563eb"), 7.0, Qt::SolidLine, Qt::RoundCap));
    }
    for (const int locationId : path.locationIds) {
        const auto node = nodeItems_.find(locationId);
        if (node != nodeItems_.end()) node->second->setBrush(QBrush(QColor("#2563eb")));
    }
    QStringList names;
    for (const int id : path.locationIds) names.append(locationLabel(repository_.city(), id));
    const QString unit = result->metric == RouteMetric::Distance ? "km" : "minutes";
    routeLabel_->setText(QString("%1\nTotal: %2 %3")
        .arg(names.join(QString::fromUtf8(" → ")))
        .arg(path.totalWeight, 0, 'f', 2).arg(unit));
}

void AllPairsRoutesPage::rebuildMap()
{
    scene_->clear(); nodeItems_.clear(); roadItems_.clear();
    const auto& graph = repository_.city();
    scene_->setBackgroundBrush(QColor("#f8fafc"));
    for (const auto& entry : graph.roads()) {
        const auto& road = entry.second;
        const auto* from = graph.location(road.fromLocationId);
        const auto* to = graph.location(road.toLocationId);
        if (!from || !to) continue;
        auto* line = scene_->addLine(from->x, from->y, to->x, to->y,
            QPen(road.closed ? QColor("#ef4444") : QColor("#cbd5e1"), 4.0,
                 road.closed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap));
        roadItems_.emplace(road.id, line);
        auto* label = scene_->addSimpleText(QString("R%1 %2").arg(road.id)
            .arg(road.direction == smartcity::core::RoadDirection::OneWay
                ? QString::fromUtf8("→") : QString::fromUtf8("↔")));
        label->setBrush(QBrush(QColor(road.closed ? "#dc2626" : "#64748b")));
        label->setPos((from->x + to->x) / 2.0 + 4.0, (from->y + to->y) / 2.0 - 18.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    }
    for (const auto& entry : graph.locations()) {
        const auto& location = entry.second;
        auto* node = scene_->addEllipse(location.x - 10.0, location.y - 10.0, 20.0, 20.0,
            QPen(Qt::white, 3.0), QBrush(QColor("#475569")));
        node->setZValue(2);
        nodeItems_.emplace(location.id, node);
        auto* label = scene_->addSimpleText(QString("%1 [L%2]")
            .arg(QString::fromStdString(location.name)).arg(location.id));
        label->setBrush(QBrush(QColor("#0f172a")));
        label->setPos(location.x + 13.0, location.y - 14.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        label->setZValue(3);
    }
    if (!scene_->items().isEmpty())
        view_->fitInView(scene_->itemsBoundingRect().adjusted(-45, -45, 45, 45), Qt::KeepAspectRatio);
}
