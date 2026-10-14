#pragma once

#include <QWidget>

namespace smartcity::services {
class CityRepository;
}

class DashboardPage final : public QWidget {
public:
    explicit DashboardPage(const smartcity::services::CityRepository& repository,
                           QWidget* parent = nullptr);
};

