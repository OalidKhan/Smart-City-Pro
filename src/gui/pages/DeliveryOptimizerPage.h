#pragma once

#include <QWidget>

#include <cstdint>
#include <unordered_map>
#include <vector>

class QCheckBox;
class QComboBox;
class QGraphicsLineItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QShowEvent;
class QSpinBox;
class QTableWidget;

namespace smartcity::services { class CityRepository; }

class DeliveryOptimizerPage final : public QWidget {
public:
    explicit DeliveryOptimizerPage(smartcity::services::CityRepository& repository,
                                   QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refreshFromModels(bool force = false);
    void refreshPackages();
    void rebuildMap(const std::vector<int>& highlightedRoads = {});
    void addPackage();
    void editPackage();
    void deletePackage();
    void applySettings();
    void runZeroOne();
    void runFractional();
    void saveScenario();
    void loadScenario();
    void showDpTable(const std::vector<double>& values, std::size_t rows, std::size_t columns);
    int selectedPackageId() const;

    smartcity::services::CityRepository& repository_;
    QComboBox* depotCombo_;
    QSpinBox* capacitySpin_;
    QComboBox* metricCombo_;
    QCheckBox* showDpCheck_;
    QTableWidget* packageTable_;
    QTableWidget* dpTable_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QLabel* totalsLabel_;
    QLabel* routesLabel_;
    QLabel* explanationLabel_;
    std::uint64_t observedGraphRevision_{static_cast<std::uint64_t>(-1)};
};
