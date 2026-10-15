#pragma once

#include "algorithms/DeliveryTourPlanner.h"

#include <QWidget>

#include <cstdint>
#include <unordered_map>

class QComboBox;
class QFutureWatcherBase;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QShowEvent;
class QTextEdit;
template<typename T> class QFutureWatcher;

namespace smartcity::services { class CityRepository; }

class AdvancedPlanningPage final : public QWidget {
public:
    explicit AdvancedPlanningPage(smartcity::services::CityRepository& repository,
                                  QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void calculateMaintenance();
    void refreshGraph(bool force = false);
    void updateDepotChoice();
    void computeTour();
    void tourFinished();
    void rebuildMap(const smartcity::algorithms::DeliveryTourResult* result = nullptr);
    [[nodiscard]] int depotId() const;

    smartcity::services::CityRepository& repository_;
    QLineEdit* intervalsEdit_;
    QLabel* maintenanceResultLabel_;
    QTextEdit* maintenanceSteps_;
    QComboBox* depotCombo_;
    QListWidget* stopsList_;
    QPushButton* computeButton_;
    QProgressBar* progressBar_;
    QLabel* tourStatusLabel_;
    QLabel* tourOrderLabel_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QFutureWatcher<smartcity::algorithms::DeliveryTourResult>* watcher_;
    std::uint64_t observedRevision_{static_cast<std::uint64_t>(-1)};
    std::unordered_map<int, QGraphicsLineItem*> roadItems_;
    std::unordered_map<int, QGraphicsEllipseItem*> nodeItems_;
};
