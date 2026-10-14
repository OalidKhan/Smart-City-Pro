#include "gui/pages/PlaceholderPage.h"

#include <QLabel>
#include <QVBoxLayout>

PlaceholderPage::PlaceholderPage(const QString& title,
                                 const QString& description,
                                 QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 36, 40, 36);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setObjectName("pageTitle");

    auto* descriptionLabel = new QLabel(description, this);
    descriptionLabel->setObjectName("pageDescription");
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setMaximumWidth(720);

    auto* badge = new QLabel("Planned for a later project part", this);
    badge->setObjectName("placeholderBadge");
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedWidth(230);

    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel);
    layout->addSpacing(18);
    layout->addWidget(badge);
    layout->addStretch();
}

