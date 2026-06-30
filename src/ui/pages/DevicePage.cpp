#include "ui/pages/DevicePage.h"

#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

namespace NFSScanner::UI {

DevicePage::DevicePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("devicePage"));
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    contentHost_ = new QWidget(scroll);
    contentHost_->setObjectName(QStringLiteral("devicePageContent"));
    auto *hostLayout = new QVBoxLayout(contentHost_);
    hostLayout->setContentsMargins(4, 4, 4, 4);
    hostLayout->setSpacing(8);
    hostLayout->addStretch(1);
    scroll->setWidget(contentHost_);
    pageLayout->addWidget(scroll);
}

QWidget *DevicePage::contentHost() const
{
    return contentHost_;
}

} // namespace NFSScanner::UI
