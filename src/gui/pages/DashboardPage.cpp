#include "gui/pages/DashboardPage.h"

#include "services/CityRepository.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

QFrame* createMetricCard(const QString& value,
                         const QString& label,
                         const QString& detail,
                         QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName("metricCard");
    card->setMinimumSize(210, 135);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(4);

    auto* valueLabel = new QLabel(value, card);
    valueLabel->setObjectName("metricValue");
    auto* nameLabel = new QLabel(label, card);
    nameLabel->setObjectName("metricName");
    auto* detailLabel = new QLabel(detail, card);
    detailLabel->setObjectName("metricDetail");
    detailLabel->setWordWrap(true);

    layout->addWidget(valueLabel);
    layout->addWidget(nameLabel);
    layout->addStretch();
    layout->addWidget(detailLabel);
    return card;
}

} // namespace

DashboardPage::DashboardPage(const smartcity::services::CityRepository& repository,
                             QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(22);

    auto* title = new QLabel("Dashboard", this);
    title->setObjectName("pageTitle");
    auto* subtitle = new QLabel(
        "Welcome to SmartCity Pro — a workspace for exploring city algorithms and services.", this);
    subtitle->setObjectName("pageDescription");
    subtitle->setWordWrap(true);

    root->addWidget(title);
    root->addWidget(subtitle);

    auto* cards = new QGridLayout;
    cards->setHorizontalSpacing(16);
    cards->setVerticalSpacing(16);
    cards->addWidget(createMetricCard(QString::number(repository.city().vertexCount()),
                                      "Sample locations", "Stored in the Qt-free city model", this), 0, 0);
    cards->addWidget(createMetricCard(QString::number(repository.city().roadCount()),
                                      "Sample roads", "Undirected weighted graph edges", this), 0, 1);
    cards->addWidget(createMetricCard("9", "Project modules",
                                      "Ready for future algorithm features", this), 0, 2);
    cards->setColumnStretch(0, 1);
    cards->setColumnStretch(1, 1);
    cards->setColumnStretch(2, 1);
    root->addLayout(cards);

    auto* notice = new QFrame(this);
    notice->setObjectName("infoPanel");
    auto* noticeLayout = new QVBoxLayout(notice);
    noticeLayout->setContentsMargins(22, 18, 22, 18);
    auto* noticeTitle = new QLabel("Part 3 shortest routes", notice);
    noticeTitle->setObjectName("sectionTitle");
    auto* noticeText = new QLabel(
        "The City Map editor remains available, and Route Finder now provides animated Dijkstra, BFS, and DFS demonstrations. "
        "All-pairs routing, dispatch, optimization, and performance analysis remain intentionally unimplemented.",
        notice);
    noticeText->setObjectName("pageDescription");
    noticeText->setWordWrap(true);
    noticeLayout->addWidget(noticeTitle);
    noticeLayout->addWidget(noticeText);
    root->addWidget(notice);
    root->addStretch();
}

