#pragma once

#include <QWidget>

class QListWidget;
class QPlainTextEdit;

namespace NFSScanner::UI {

class ReportPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ReportPage(QWidget *parent = nullptr);

    QListWidget *reportList() const;
    QPlainTextEdit *previewEditor() const;

private:
    QListWidget *reportList_ = nullptr;
    QPlainTextEdit *previewEditor_ = nullptr;
};

} // namespace NFSScanner::UI
