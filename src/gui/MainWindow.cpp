#include "gui/MainWindow.h"

#include "gui/pages/AllPairsRoutesPage.h"
#include "gui/pages/AdvancedPlanningPage.h"
#include "gui/pages/AlgorithmLabPage.h"
#include "gui/pages/CityMapPage.h"
#include "gui/pages/DashboardPage.h"
#include "gui/pages/DeliveryOptimizerPage.h"
#include "gui/pages/EmergencyDispatchPage.h"
#include "gui/pages/InfrastructurePage.h"
#include "gui/pages/PlaceholderPage.h"
#include "gui/pages/RouteFinderPage.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("SmartCity Pro");
    setMinimumSize(1050, 680);
    resize(1280, 780);
    buildInterface();
    applyTheme();
    statusBar()->showMessage("Ready — Part 9 advanced planning loaded");
}

void MainWindow::buildInterface()
{
    auto* central = new QWidget(this);
    auto* shell = new QHBoxLayout(central);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    navigationContainer_ = new QFrame(central);
    navigationContainer_->setObjectName("sidebar");
    navigationContainer_->setFixedWidth(245);
    auto* navigationLayout = new QVBoxLayout(navigationContainer_);
    navigationLayout->setContentsMargins(18, 26, 18, 22);
    navigationLayout->setSpacing(7);

    auto* brand = new QLabel("SMARTCITY", navigationContainer_);
    brand->setObjectName("brand");
    auto* brandSuffix = new QLabel("PRO", navigationContainer_);
    brandSuffix->setObjectName("brandSuffix");
    navigationLayout->addWidget(brand);
    navigationLayout->addWidget(brandSuffix);
    navigationLayout->addSpacing(22);

    navigationGroup_ = new QButtonGroup(this);
    navigationGroup_->setExclusive(true);
    pageStack_ = new QStackedWidget(central);
    pageStack_->setObjectName("pageStack");

    addNavigationItem("Dashboard", new DashboardPage(repository_, pageStack_));
    addNavigationItem("City Map", new CityMapPage(repository_, pageStack_));
    addNavigationItem("Route Finder", new RouteFinderPage(repository_, pageStack_));
    addNavigationItem("All-Pairs Routes", new AllPairsRoutesPage(repository_, pageStack_));
    addNavigationItem("Infrastructure", new InfrastructurePage(repository_, pageStack_));
    addNavigationItem("Emergency Dispatch", new EmergencyDispatchPage(repository_, pageStack_));
    addNavigationItem("Delivery Optimizer", new DeliveryOptimizerPage(repository_, pageStack_));
    addNavigationItem("Algorithm Lab", new AlgorithmLabPage(repository_, pageStack_));
    addNavigationItem("Advanced Planning", new AdvancedPlanningPage(repository_, pageStack_));
    addNavigationItem("Performance", new PlaceholderPage(
        "Performance", "Benchmark collection and complexity charts are planned for a later part.", pageStack_));

    navigationLayout->addStretch();
    auto* version = new QLabel("University Algorithms Project\nPart 9 · v0.9", navigationContainer_);
    version->setObjectName("sidebarFooter");
    navigationLayout->addWidget(version);

    shell->addWidget(navigationContainer_);
    shell->addWidget(pageStack_, 1);
    setCentralWidget(central);

    if (auto* firstButton = navigationGroup_->button(0)) {
        firstButton->setChecked(true);
    }
    pageStack_->setCurrentIndex(0);
}

void MainWindow::addNavigationItem(const QString& label, QWidget* page)
{
    const int pageIndex = pageStack_->addWidget(page);
    pageNames_.push_back(label);

    auto* button = new QPushButton(label, navigationContainer_);
    button->setObjectName("navigationButton");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(42);
    navigationGroup_->addButton(button, pageIndex);

    auto* layout = qobject_cast<QVBoxLayout*>(navigationContainer_->layout());
    layout->addWidget(button);

    connect(button, &QPushButton::clicked, this, [this, pageIndex] {
        pageStack_->setCurrentIndex(pageIndex);
        statusBar()->showMessage(pageNames_.at(static_cast<std::size_t>(pageIndex)) + " selected", 2500);
    });
}

void MainWindow::applyTheme()
{
    setStyleSheet(R"(
        QMainWindow, QWidget#pageStack {
            background: #f1f5f9;
            color: #0f172a;
        }
        QFrame#sidebar {
            background: #0f172a;
            border: none;
        }
        QLabel#brand {
            color: #f8fafc;
            font-size: 21px;
            font-weight: 700;
            letter-spacing: 2px;
        }
        QLabel#brandSuffix {
            color: #60a5fa;
            font-size: 13px;
            font-weight: 700;
            letter-spacing: 5px;
        }
        QPushButton#navigationButton {
            background: transparent;
            border: none;
            border-radius: 7px;
            color: #cbd5e1;
            font-size: 13px;
            font-weight: 500;
            padding: 10px 13px;
            text-align: left;
        }
        QPushButton#navigationButton:hover {
            background: #1e293b;
            color: #ffffff;
        }
        QPushButton#navigationButton:checked {
            background: #2563eb;
            color: #ffffff;
            font-weight: 600;
        }
        QLabel#sidebarFooter {
            color: #64748b;
            font-size: 11px;
            line-height: 1.4;
        }
        QLabel#pageTitle {
            color: #0f172a;
            font-size: 28px;
            font-weight: 700;
        }
        QLabel#pageDescription {
            color: #64748b;
            font-size: 14px;
        }
        QLabel#placeholderBadge {
            background: #dbeafe;
            border: 1px solid #bfdbfe;
            border-radius: 14px;
            color: #1d4ed8;
            font-size: 12px;
            font-weight: 600;
            padding: 7px 12px;
        }
        QFrame#metricCard, QFrame#infoPanel {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 10px;
        }
        QLabel#metricValue {
            color: #2563eb;
            font-size: 27px;
            font-weight: 700;
        }
        QLabel#metricName, QLabel#sectionTitle {
            color: #0f172a;
            font-size: 15px;
            font-weight: 650;
        }
        QLabel#metricDetail {
            color: #94a3b8;
            font-size: 11px;
        }
        QGraphicsView#mapView {
            background: #f8fafc;
            border: 1px solid #dbe3ee;
            border-radius: 10px;
        }
        QPushButton#editorButton {
            background: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            color: #1e293b;
            font-weight: 600;
            padding: 7px 10px;
        }
        QPushButton#editorButton:hover {
            background: #eff6ff;
            border-color: #60a5fa;
            color: #1d4ed8;
        }
        QFrame#legendPanel {
            background: #ffffff;
            border: 1px solid #dbe3ee;
            border-radius: 10px;
        }
        QLabel#mapSummary {
            color: #64748b;
            font-size: 11px;
        }
        QFrame#routePanel {
            background: #ffffff;
            border: 1px solid #dbe3ee;
            border-radius: 9px;
        }
        QPushButton#primaryButton {
            background: #2563eb;
            border: none;
            border-radius: 6px;
            color: #ffffff;
            font-weight: 600;
            padding: 8px 12px;
        }
        QPushButton#primaryButton:hover { background: #1d4ed8; }
        QLabel#algorithmCaveat {
            color: #92400e;
            font-size: 11px;
        }
        QLabel#routeResult {
            color: #334155;
            font-size: 12px;
        }
        QListWidget#routeList {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
        }
        QGraphicsView#routeView {
            background: #f8fafc;
            border: 1px solid #dbe3ee;
            border-radius: 9px;
        }
        QGraphicsView#allPairsView {
            background: #f8fafc;
            border: 1px solid #dbe3ee;
            border-radius: 7px;
        }
        QGraphicsView#infrastructureView {
            background: #f8fafc;
            border: 1px solid #dbe3ee;
            border-radius: 7px;
        }
        QGraphicsView#dispatchView {
            background: #f8fafc;
            border: 1px solid #dbe3ee;
            border-radius: 7px;
        }
        QGraphicsView#deliveryView {
            background: #f8fafc;
            border: 1px solid #dbe3ee;
            border-radius: 7px;
        }
        QGraphicsView#algorithmLabView {
            background: #f8fafc;
            border: 1px solid #dbe3ee;
            border-radius: 7px;
        }
        QGraphicsView#advancedPlanningView {
            background: #f8fafc;
            border: 1px solid #dbe3ee;
            border-radius: 7px;
        }
        QTableWidget#candidateTable {
            background: #ffffff;
            alternate-background-color: #f8fafc;
            border: 1px solid #dbe3ee;
            gridline-color: #e2e8f0;
        }
        QTableWidget#dispatchTable {
            background: #ffffff;
            alternate-background-color: #f8fafc;
            border: 1px solid #dbe3ee;
            gridline-color: #e2e8f0;
        }
        QTableWidget#deliveryTable {
            background: #ffffff;
            alternate-background-color: #f8fafc;
            border: 1px solid #dbe3ee;
            gridline-color: #e2e8f0;
        }
        QTableWidget#matrixTable {
            background: #ffffff;
            alternate-background-color: #f8fafc;
            border: 1px solid #dbe3ee;
            gridline-color: #e2e8f0;
        }
        QStatusBar {
            background: #ffffff;
            border-top: 1px solid #e2e8f0;
            color: #64748b;
            font-size: 11px;
        }
    )");
}

