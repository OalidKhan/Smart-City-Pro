#pragma once

#include "services/CityRepository.h"

#include <QMainWindow>
#include <QString>
#include <vector>

class QButtonGroup;
class QStackedWidget;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void buildInterface();
    void applyTheme();
    void addNavigationItem(const QString& label, QWidget* page);

    smartcity::services::CityRepository repository_;
    QButtonGroup* navigationGroup_{nullptr};
    QStackedWidget* pageStack_{nullptr};
    QWidget* navigationContainer_{nullptr};
    std::vector<QString> pageNames_;
};

