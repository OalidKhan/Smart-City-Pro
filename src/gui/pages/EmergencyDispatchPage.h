#pragma once

#include "services/EmergencyDispatchSimulation.h"

#include <QWidget>

#include <cstdint>
#include <unordered_map>

class QComboBox;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QHideEvent;
class QShowEvent;
class QTableWidget;
class QTimer;

namespace smartcity::services { class CityRepository; }

class EmergencyDispatchPage final : public QWidget {
public:
    explicit EmergencyDispatchPage(smartcity::services::CityRepository& repository,
                                   QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void refreshAll(bool rebuildMap = true);
    void rebuildMap();
    void refreshTables();
    void refreshActiveRequests();
    void addAmbulance();
    void editAmbulance();
    void deleteAmbulance();
    void addHospital();
    void editHospital();
    void deleteHospital();
    void createRequest();
    void cancelRequest();
    void deleteRequestRecord();
    void dispatchNext();
    void stepJourney();
    void saveSimulation();
    void loadSimulation();
    void showStep(const smartcity::services::SimulationStepResult& result);
    int selectedId(const QTableWidget* table) const;

    smartcity::services::CityRepository& repository_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QTableWidget* ambulanceTable_;
    QTableWidget* hospitalTable_;
    QTableWidget* requestTable_;
    QComboBox* activeRequestCombo_;
    QLabel* clockLabel_;
    QLabel* statusLabel_;
    QLabel* speedLabel_;
    QTimer* timer_;
    std::uint64_t observedGraphRevision_{static_cast<std::uint64_t>(-1)};
    std::unordered_map<int, QGraphicsLineItem*> roadItems_;
    std::unordered_map<int, QGraphicsEllipseItem*> nodeItems_;
};
