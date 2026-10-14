#pragma once

#include <QString>
#include <QWidget>

#include <unordered_map>

class QLabel;
class QGraphicsScene;
class QGraphicsView;

namespace smartcity::services { class CityRepository; }

class LocationGraphicsItem;
class RoadGraphicsItem;

class CityMapPage final : public QWidget {
public:
    explicit CityMapPage(smartcity::services::CityRepository& repository,
                         QWidget* parent = nullptr);

private:
    void rebuildScene();
    void updateRoadDrawings(int locationId);
    void updateSummary(const QString& message = {});
    void addLocation();
    void addRoad();
    void editSelected();
    void deleteSelected();
    void editLocation(int locationId);
    void editRoad(int roadId);
    void saveCity();
    void loadCity();
    QWidget* createLegend();

    smartcity::services::CityRepository& repository_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QLabel* summaryLabel_;
    std::unordered_map<int, LocationGraphicsItem*> locationItems_;
    std::unordered_map<int, RoadGraphicsItem*> roadItems_;
};
