#pragma once

#include <QString>
#include <QWidget>

class PlaceholderPage final : public QWidget {
public:
    PlaceholderPage(const QString& title,
                    const QString& description,
                    QWidget* parent = nullptr);
};

