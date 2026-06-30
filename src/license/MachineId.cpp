#include "license/MachineId.h"

#include <QCryptographicHash>
#include <QHostInfo>
#include <QSysInfo>

namespace NFSScanner::License {

QString MachineId::generate()
{
    const QString seed = QStringList{
        QSysInfo::machineUniqueId(),
        QSysInfo::productType(),
        QSysInfo::productVersion(),
        QHostInfo::localHostName(),
    }.join(QLatin1Char('|'));

    const QByteArray hash = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex().left(32));
}

} // namespace NFSScanner::License
