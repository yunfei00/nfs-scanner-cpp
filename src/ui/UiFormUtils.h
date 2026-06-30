#pragma once

#include <QString>

class QLineEdit;
class QWidget;

namespace NFSScanner::UI {

QLineEdit *createDoubleEdit(const QString &value, QWidget *parent);
QLineEdit *createIntegerEdit(const QString &value, QWidget *parent);

} // namespace NFSScanner::UI
