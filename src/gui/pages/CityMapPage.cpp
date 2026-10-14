#include "gui/pages/CityMapPage.h"

#include "algorithms/WeightedGraph.h"
#include "core/models/CityLocation.h"
#include "core/models/Road.h"
#include "services/CityJsonSerializer.h"
#include "services/CityRepository.h"

#include <QCheckBox>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QPolygonF>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

namespace {

using smartcity::core::CityLocation;
using smartcity::core::LocationCategory;
using smartcity::core::Road;
using smartcity::core::RoadDirection;

QColor categoryColor(LocationCategory category)
{
    switch (category) {
    case LocationCategory::Intersection: return QColor("#64748b");
    case LocationCategory::Hospital: return QColor("#ef4444");
    case LocationCategory::University: return QColor("#8b5cf6");
    case LocationCategory::Market: return QColor("#f59e0b");
    case LocationCategory::ResidentialArea: return QColor("#22c55e");
    case LocationCategory::AmbulanceStation: return QColor("#06b6d4");
    }
    return QColor("#64748b");
}

QString displayCategory(LocationCategory category)
{
    QString value = QString::fromLatin1(smartcity::core::categoryName(category));
    value[0] = value[0].toUpper();
    return value;
}

QComboBox* createCategoryCombo(QWidget* parent, LocationCategory current)
{
    auto* combo = new QComboBox(parent);
    for (int value = static_cast<int>(LocationCategory::Intersection);
         value <= static_cast<int>(LocationCategory::AmbulanceStation); ++value) {
        const auto category = static_cast<LocationCategory>(value);
        combo->addItem(displayCategory(category), value);
    }
    combo->setCurrentIndex(combo->findData(static_cast<int>(current)));
    return combo;
}

QDoubleSpinBox* createNumberInput(QWidget* parent, double value, bool coordinate = false)
{
    auto* input = new QDoubleSpinBox(parent);
    input->setDecimals(2);
    input->setRange(coordinate ? -100000.0 : 0.0, 1000000000.0);
    input->setValue(value);
    input->setSingleStep(coordinate ? 10.0 : 0.5);
    return input;
}

class MapView final : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        const double nextScale = transform().m11() * factor;
        if (nextScale >= 0.25 && nextScale <= 4.0) scale(factor, factor);
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::MiddleButton) {
            panning_ = true;
            panOrigin_ = event->position().toPoint();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QGraphicsView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (panning_) {
            const QPoint delta = event->position().toPoint() - panOrigin_;
            panOrigin_ = event->position().toPoint();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::MiddleButton && panning_) {
            panning_ = false;
            unsetCursor();
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    void drawBackground(QPainter* painter, const QRectF& rect) override
    {
        painter->fillRect(rect, QColor("#f8fafc"));
        painter->setPen(QPen(QColor("#e8eef5"), 0));
        constexpr int grid = 40;
        const int left = static_cast<int>(std::floor(rect.left() / grid)) * grid;
        const int top = static_cast<int>(std::floor(rect.top() / grid)) * grid;
        for (int x = left; x < rect.right(); x += grid) painter->drawLine(x, rect.top(), x, rect.bottom());
        for (int y = top; y < rect.bottom(); y += grid) painter->drawLine(rect.left(), y, rect.right(), y);
    }

private:
    bool panning_{false};
    QPoint panOrigin_;
};

} // namespace

class LocationGraphicsItem final : public QGraphicsEllipseItem {
public:
    LocationGraphicsItem(const CityLocation& location,
                         std::function<void(int, const QPointF&)> moved,
                         std::function<void(int)> edit)
        : QGraphicsEllipseItem(-11.0, -11.0, 22.0, 22.0),
          id_(location.id), moved_(std::move(moved)), edit_(std::move(edit))
    {
        setPen(QPen(Qt::white, 3.0));
        setBrush(QBrush(categoryColor(location.category)));
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setZValue(2.0);
        setPos(location.x, location.y);
        setToolTip(QString("ID %1\n%2\n%3\nDrag to move; double-click to edit")
                       .arg(location.id).arg(QString::fromStdString(location.name))
                       .arg(displayCategory(location.category)));

        auto* label = new QGraphicsSimpleTextItem(
            QString("%1  [L%2]").arg(QString::fromStdString(location.name)).arg(location.id), this);
        label->setBrush(QBrush(QColor("#0f172a")));
        label->setPos(15.0, -15.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        label->setAcceptedMouseButtons(Qt::NoButton);
    }

    [[nodiscard]] int id() const noexcept { return id_; }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == ItemPositionHasChanged && moved_) moved_(id_, value.toPointF());
        return QGraphicsEllipseItem::itemChange(change, value);
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (edit_) edit_(id_);
        event->accept();
    }

private:
    int id_;
    std::function<void(int, const QPointF&)> moved_;
    std::function<void(int)> edit_;
};

class RoadGraphicsItem final : public QGraphicsLineItem {
public:
    RoadGraphicsItem(const Road& road, const CityLocation& from, const CityLocation& to,
                     std::function<void(int)> edit)
        : id_(road.id), edit_(std::move(edit)), label_(new QGraphicsSimpleTextItem(this)),
          arrow_(new QGraphicsPolygonItem(this))
    {
        setFlags(ItemIsSelectable);
        setZValue(0.0);
        label_->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        label_->setAcceptedMouseButtons(Qt::NoButton);
        label_->setBrush(QBrush(QColor("#334155")));
        arrow_->setAcceptedMouseButtons(Qt::NoButton);
        updateGeometry(road, from, to);
    }

    [[nodiscard]] int id() const noexcept { return id_; }

    void updateGeometry(const Road& road, const CityLocation& from, const CityLocation& to)
    {
        const QLineF roadLine(from.x, from.y, to.x, to.y);
        setLine(roadLine);
        setPen(QPen(road.closed ? QColor("#ef4444") : QColor("#94a3b8"),
                    road.closed ? 5.0 : 4.0,
                    road.closed ? Qt::DashLine : Qt::SolidLine,
                    Qt::RoundCap));

        const QPointF middle = roadLine.pointAt(0.5);
        label_->setText(QString("R%1  %2 km · %3 min %4%5")
                            .arg(road.id).arg(road.distance, 0, 'f', 1)
                            .arg(road.travelTime, 0, 'f', 1)
                            .arg(road.direction == RoadDirection::OneWay ? QString::fromUtf8("→") : QString::fromUtf8("↔"))
                            .arg(road.closed ? "  CLOSED" : ""));
        label_->setPos(middle + QPointF(8.0, -24.0));

        if (road.direction == RoadDirection::OneWay && roadLine.length() > 0.0) {
            const double ux = roadLine.dx() / roadLine.length();
            const double uy = roadLine.dy() / roadLine.length();
            const QPointF tip = middle + QPointF(ux * 13.0, uy * 13.0);
            const QPointF base = middle - QPointF(ux * 8.0, uy * 8.0);
            QPolygonF polygon;
            polygon << tip << base + QPointF(-uy * 7.0, ux * 7.0)
                    << base + QPointF(uy * 7.0, -ux * 7.0);
            arrow_->setPolygon(polygon);
            arrow_->setBrush(QBrush(road.closed ? QColor("#ef4444") : QColor("#475569")));
            arrow_->setPen(QPen(Qt::NoPen));
            arrow_->show();
        } else {
            arrow_->hide();
        }
        setToolTip(QString("Road ID %1\nDistance: %2 km\nTravel time: %3 min\nConstruction cost: %4\n%5, %6\nDouble-click to edit")
                       .arg(road.id).arg(road.distance).arg(road.travelTime).arg(road.constructionCost)
                       .arg(road.direction == RoadDirection::OneWay ? "One-way" : "Two-way")
                       .arg(road.closed ? "Closed" : "Open"));
    }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (edit_) edit_(id_);
        event->accept();
    }

private:
    int id_;
    std::function<void(int)> edit_;
    QGraphicsSimpleTextItem* label_;
    QGraphicsPolygonItem* arrow_;
};

CityMapPage::CityMapPage(smartcity::services::CityRepository& repository, QWidget* parent)
    : QWidget(parent), repository_(repository), scene_(new QGraphicsScene(this)),
      view_(new MapView(scene_, this)), summaryLabel_(new QLabel(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(10);

    auto* title = new QLabel("Interactive City Map Editor", this);
    title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "The graph is the source of truth. Drag locations, double-click items to edit, use the mouse wheel to zoom, and middle-drag to pan.", this);
    description->setObjectName("pageDescription");
    description->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(description);

    auto* tools = new QHBoxLayout;
    const std::vector<std::pair<QString, std::function<void()>>> actions = {
        {"+ Location", [this] { addLocation(); }}, {"+ Road", [this] { addRoad(); }},
        {"Edit selected", [this] { editSelected(); }}, {"Delete selected", [this] { deleteSelected(); }},
        {"Save JSON", [this] { saveCity(); }}, {"Load JSON", [this] { loadCity(); }},
        {"Fit map", [this] { view_->fitInView(scene_->itemsBoundingRect().adjusted(-60, -60, 60, 60), Qt::KeepAspectRatio); }}
    };
    for (const auto& action : actions) {
        auto* button = new QPushButton(action.first, this);
        button->setObjectName("editorButton");
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, action.second);
        tools->addWidget(button);
    }
    tools->addStretch();
    root->addLayout(tools);

    auto* content = new QHBoxLayout;
    view_->setObjectName("mapView");
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::NoDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setSceneRect(-100, -100, 950, 650);
    view_->setMinimumHeight(430);
    content->addWidget(view_, 1);
    content->addWidget(createLegend());
    root->addLayout(content, 1);

    summaryLabel_->setObjectName("mapSummary");
    root->addWidget(summaryLabel_);
    rebuildScene();
}

QWidget* CityMapPage::createLegend()
{
    auto* panel = new QFrame(this);
    panel->setObjectName("legendPanel");
    panel->setFixedWidth(175);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(9);
    auto* title = new QLabel("Map legend", panel);
    title->setObjectName("sectionTitle");
    layout->addWidget(title);
    for (int value = 0; value <= static_cast<int>(LocationCategory::AmbulanceStation); ++value) {
        const auto category = static_cast<LocationCategory>(value);
        auto* label = new QLabel(QString("●  %1").arg(displayCategory(category)), panel);
        label->setStyleSheet(QString("color: %1;").arg(categoryColor(category).name()));
        label->setWordWrap(true);
        layout->addWidget(label);
    }
    layout->addSpacing(8);
    layout->addWidget(new QLabel(QString::fromUtf8("→  One-way road"), panel));
    layout->addWidget(new QLabel(QString::fromUtf8("↔  Two-way road"), panel));
    auto* closed = new QLabel("- -  Closed road", panel);
    closed->setStyleSheet("color: #ef4444;");
    layout->addWidget(closed);
    layout->addStretch();
    return panel;
}

void CityMapPage::rebuildScene()
{
    scene_->clear();
    locationItems_.clear();
    roadItems_.clear();
    auto& graph = repository_.city();

    for (const auto& entry : graph.roads()) {
        const auto& road = entry.second;
        const auto* from = graph.location(road.fromLocationId);
        const auto* to = graph.location(road.toLocationId);
        if (!from || !to) continue;
        auto* item = new RoadGraphicsItem(road, *from, *to, [this](int id) { editRoad(id); });
        scene_->addItem(item);
        roadItems_.emplace(road.id, item);
    }
    for (const auto& entry : graph.locations()) {
        const auto& location = entry.second;
        auto* item = new LocationGraphicsItem(
            location,
            [this](int id, const QPointF& position) {
                if (repository_.city().moveLocation(id, position.x(), position.y())) {
                    updateRoadDrawings(id);
                    updateSummary("Location moved");
                }
            },
            [this](int id) { editLocation(id); });
        scene_->addItem(item);
        locationItems_.emplace(location.id, item);
    }
    updateSummary();
}

void CityMapPage::updateRoadDrawings(int locationId)
{
    const auto& graph = repository_.city();
    for (const auto& entry : graph.roads()) {
        const auto& road = entry.second;
        if (road.fromLocationId != locationId && road.toLocationId != locationId) continue;
        const auto* from = graph.location(road.fromLocationId);
        const auto* to = graph.location(road.toLocationId);
        const auto item = roadItems_.find(road.id);
        if (from && to && item != roadItems_.end()) item->second->updateGeometry(road, *from, *to);
    }
}

void CityMapPage::updateSummary(const QString& message)
{
    const auto& graph = repository_.city();
    summaryLabel_->setText(QString("%1 locations · %2 roads · graph revision %3%4")
                               .arg(static_cast<qulonglong>(graph.vertexCount()))
                               .arg(static_cast<qulonglong>(graph.roadCount()))
                               .arg(static_cast<qulonglong>(graph.revision()))
                               .arg(message.isEmpty() ? QString() : "  —  " + message));
}

void CityMapPage::addLocation()
{
    auto& graph = repository_.city();
    const QPointF center = view_->mapToScene(view_->viewport()->rect().center());
    CityLocation location{graph.nextLocationId(), "New location", center.x(), center.y(), LocationCategory::Intersection};

    QDialog dialog(this);
    dialog.setWindowTitle("Add location");
    auto* form = new QFormLayout(&dialog);
    auto* id = new QLabel(QString::number(location.id), &dialog);
    auto* name = new QLineEdit(QString::fromStdString(location.name), &dialog);
    auto* x = createNumberInput(&dialog, location.x, true);
    auto* y = createNumberInput(&dialog, location.y, true);
    auto* category = createCategoryCombo(&dialog, location.category);
    form->addRow("Stable ID", id); form->addRow("Name", name); form->addRow("X coordinate", x);
    form->addRow("Y coordinate", y); form->addRow("Category", category);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    if (name->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Invalid location", "The location name cannot be empty.");
        return;
    }
    location.name = name->text().trimmed().toStdString();
    location.x = x->value(); location.y = y->value();
    location.category = static_cast<LocationCategory>(category->currentData().toInt());
    if (!graph.addLocation(location)) {
        QMessageBox::warning(this, "Invalid location", "The location could not be added.");
        return;
    }
    rebuildScene(); updateSummary("Location added");
}

void CityMapPage::editLocation(int locationId)
{
    auto& graph = repository_.city();
    const auto* existing = graph.location(locationId);
    if (!existing) return;
    CityLocation location = *existing;
    QDialog dialog(this); dialog.setWindowTitle("Edit location");
    auto* form = new QFormLayout(&dialog);
    auto* name = new QLineEdit(QString::fromStdString(location.name), &dialog);
    auto* x = createNumberInput(&dialog, location.x, true);
    auto* y = createNumberInput(&dialog, location.y, true);
    auto* category = createCategoryCombo(&dialog, location.category);
    form->addRow("Stable ID", new QLabel(QString::number(location.id), &dialog));
    form->addRow("Name", name); form->addRow("X coordinate", x); form->addRow("Y coordinate", y);
    form->addRow("Category", category);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    if (name->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Invalid location", "The location name cannot be empty."); return;
    }
    location.name = name->text().trimmed().toStdString(); location.x = x->value(); location.y = y->value();
    location.category = static_cast<LocationCategory>(category->currentData().toInt());
    if (!graph.updateLocation(location)) {
        QMessageBox::warning(this, "Invalid location", "The location could not be updated."); return;
    }
    rebuildScene(); updateSummary("Location updated");
}

void CityMapPage::addRoad()
{
    auto& graph = repository_.city();
    if (graph.vertexCount() < 2) {
        QMessageBox::information(this, "Add road", "Add at least two locations first."); return;
    }
    Road road; road.id = graph.nextRoadId(); road.distance = 1.0; road.travelTime = 2.0;
    std::vector<int> ids; for (const auto& entry : graph.locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end()); road.fromLocationId = ids[0]; road.toLocationId = ids[1];

    QDialog dialog(this); dialog.setWindowTitle("Add road"); auto* form = new QFormLayout(&dialog);
    auto* from = new QComboBox(&dialog); auto* to = new QComboBox(&dialog);
    for (int locationId : ids) {
        const auto* location = graph.location(locationId);
        const QString text = QString("L%1 — %2").arg(locationId).arg(QString::fromStdString(location->name));
        from->addItem(text, locationId); to->addItem(text, locationId);
    }
    to->setCurrentIndex(1);
    auto* distance = createNumberInput(&dialog, road.distance);
    auto* time = createNumberInput(&dialog, road.travelTime);
    auto* cost = createNumberInput(&dialog, road.constructionCost);
    auto* direction = new QComboBox(&dialog); direction->addItem("Two-way", static_cast<int>(RoadDirection::TwoWay));
    direction->addItem("One-way (from → to)", static_cast<int>(RoadDirection::OneWay));
    auto* closed = new QCheckBox("Road is closed", &dialog);
    form->addRow("Stable ID", new QLabel(QString::number(road.id), &dialog)); form->addRow("From", from);
    form->addRow("To", to); form->addRow("Distance (km)", distance); form->addRow("Travel time (min)", time);
    form->addRow("Construction cost", cost); form->addRow("Direction", direction); form->addRow("Status", closed);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    road.fromLocationId = from->currentData().toInt(); road.toLocationId = to->currentData().toInt();
    road.distance = distance->value(); road.travelTime = time->value(); road.constructionCost = cost->value();
    road.direction = static_cast<RoadDirection>(direction->currentData().toInt()); road.closed = closed->isChecked();
    if (!graph.addRoad(road)) {
        QMessageBox::warning(this, "Invalid road", "Choose two different endpoints and use non-negative road values."); return;
    }
    rebuildScene(); updateSummary("Road added");
}

void CityMapPage::editRoad(int roadId)
{
    auto& graph = repository_.city(); const auto* existing = graph.road(roadId); if (!existing) return;
    Road road = *existing;
    std::vector<int> ids; for (const auto& entry : graph.locations()) ids.push_back(entry.first); std::sort(ids.begin(), ids.end());
    QDialog dialog(this); dialog.setWindowTitle("Edit road"); auto* form = new QFormLayout(&dialog);
    auto* from = new QComboBox(&dialog); auto* to = new QComboBox(&dialog);
    for (int locationId : ids) {
        const auto* location = graph.location(locationId);
        const QString text = QString("L%1 — %2").arg(locationId).arg(QString::fromStdString(location->name));
        from->addItem(text, locationId); to->addItem(text, locationId);
    }
    from->setCurrentIndex(from->findData(road.fromLocationId)); to->setCurrentIndex(to->findData(road.toLocationId));
    auto* distance = createNumberInput(&dialog, road.distance); auto* time = createNumberInput(&dialog, road.travelTime);
    auto* cost = createNumberInput(&dialog, road.constructionCost); auto* direction = new QComboBox(&dialog);
    direction->addItem("Two-way", static_cast<int>(RoadDirection::TwoWay));
    direction->addItem("One-way (from → to)", static_cast<int>(RoadDirection::OneWay));
    direction->setCurrentIndex(direction->findData(static_cast<int>(road.direction)));
    auto* closed = new QCheckBox("Road is closed", &dialog); closed->setChecked(road.closed);
    form->addRow("Stable ID", new QLabel(QString::number(road.id), &dialog)); form->addRow("From", from);
    form->addRow("To", to); form->addRow("Distance (km)", distance); form->addRow("Travel time (min)", time);
    form->addRow("Construction cost", cost); form->addRow("Direction", direction); form->addRow("Status", closed);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    road.fromLocationId = from->currentData().toInt(); road.toLocationId = to->currentData().toInt();
    road.distance = distance->value(); road.travelTime = time->value(); road.constructionCost = cost->value();
    road.direction = static_cast<RoadDirection>(direction->currentData().toInt()); road.closed = closed->isChecked();
    if (!graph.updateRoad(road)) {
        QMessageBox::warning(this, "Invalid road", "Choose two different endpoints and use non-negative road values."); return;
    }
    rebuildScene(); updateSummary("Road updated");
}

void CityMapPage::editSelected()
{
    const auto selected = scene_->selectedItems();
    if (selected.isEmpty()) { QMessageBox::information(this, "Edit", "Select a location or road first."); return; }
    if (auto* location = dynamic_cast<LocationGraphicsItem*>(selected.front())) editLocation(location->id());
    else if (auto* road = dynamic_cast<RoadGraphicsItem*>(selected.front())) editRoad(road->id());
}

void CityMapPage::deleteSelected()
{
    const auto selected = scene_->selectedItems();
    if (selected.isEmpty()) { QMessageBox::information(this, "Delete", "Select a location or road first."); return; }
    auto& graph = repository_.city();
    if (auto* location = dynamic_cast<LocationGraphicsItem*>(selected.front())) {
        std::size_t connected = 0;
        for (const auto& entry : graph.roads())
            if (entry.second.fromLocationId == location->id() || entry.second.toLocationId == location->id()) ++connected;
        if (QMessageBox::question(this, "Delete location",
                QString("Delete this location and its %1 connected road(s)?").arg(connected)) != QMessageBox::Yes) return;
        graph.removeLocation(location->id()); rebuildScene(); updateSummary("Location and connected roads deleted");
    } else if (auto* road = dynamic_cast<RoadGraphicsItem*>(selected.front())) {
        if (QMessageBox::question(this, "Delete road", "Delete the selected road?") != QMessageBox::Yes) return;
        graph.removeRoad(road->id()); rebuildScene(); updateSummary("Road deleted");
    }
}

void CityMapPage::saveCity()
{
    const QString fileName = QFileDialog::getSaveFileName(this, "Save city", "smartcity.json", "JSON files (*.json)");
    if (fileName.isEmpty()) return;
    QString error;
    if (!smartcity::services::CityJsonSerializer::save(fileName, repository_.city(), &error))
        QMessageBox::critical(this, "Save failed", error);
    else updateSummary("Saved to " + fileName);
}

void CityMapPage::loadCity()
{
    const QString fileName = QFileDialog::getOpenFileName(this, "Load city", {}, "JSON files (*.json)");
    if (fileName.isEmpty()) return;
    QString error;
    if (!smartcity::services::CityJsonSerializer::load(fileName, repository_.city(), &error)) {
        QMessageBox::critical(this, "Load failed", error + "\n\nThe current city was not changed."); return;
    }
    rebuildScene();
    if (!scene_->items().isEmpty()) view_->fitInView(scene_->itemsBoundingRect().adjusted(-60, -60, 60, 60), Qt::KeepAspectRatio);
    updateSummary("Loaded " + fileName);
}
