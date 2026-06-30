#pragma once

#include <QWidget>

namespace NFSScanner::UI {

class DevicePage final : public QWidget
{
    Q_OBJECT

public:
    explicit DevicePage(QWidget *parent = nullptr);

    QWidget *contentHost() const;

private:
    QWidget *contentHost_ = nullptr;
};

} // namespace NFSScanner::UI
