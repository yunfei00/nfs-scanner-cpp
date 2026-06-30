#include "ui/UiFormUtils.h"

#include <QDoubleValidator>
#include <QIntValidator>
#include <QLineEdit>

namespace NFSScanner::UI {

QLineEdit *createDoubleEdit(const QString &value, QWidget *parent)
{
    auto *edit = new QLineEdit(value, parent);
    auto *validator = new QDoubleValidator(-1000000.0, 1000000.0, 4, edit);
    validator->setNotation(QDoubleValidator::StandardNotation);
    edit->setValidator(validator);
    edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return edit;
}

QLineEdit *createIntegerEdit(const QString &value, QWidget *parent)
{
    auto *edit = new QLineEdit(value, parent);
    edit->setValidator(new QIntValidator(1, 10000000, edit));
    edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return edit;
}

} // namespace NFSScanner::UI
