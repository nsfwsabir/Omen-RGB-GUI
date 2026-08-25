#include "RgbController.h"
#include "OmenDevice.h"
#include "SysFs.h"
#include <QDebug>

RgbController::RgbController(QObject *parent) : QObject(parent) {}

QString RgbController::colorToHex(const QColor &c) {
    return QString("%1%2%3")
        .arg(c.red(), 2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.blue(), 2, 16, QChar('0')).toUpper();
}

QColor RgbController::hexToColor(const QString &hex) {
    QString h = SysFs::normalizeHex(hex);
    bool ok;
    int r = h.mid(0,2).toInt(&ok, 16);
    int g = h.mid(2,2).toInt(&ok, 16);
    int b = h.mid(4,2).toInt(&ok, 16);
    return QColor(r,g,b);
}

bool RgbController::setZoneHex(int zone, const QString &hex, QString *error) {
    if (!isValidZone(zone)) {
        if (error) *error = "Invalid zone (0-3)";
        return false;
    }
    QString h = SysFs::normalizeHex(hex);
    if (!SysFs::isValidHexColor(h)) {
        if (error) *error = "Invalid hex color (expected RRGGBB)";
        return false;
    }
    QString path = OmenDevice::rgbPath(QString("zone%1").arg(zone, 2, 10, QChar('0')));
    // driver expects bare hex without '#'
    return SysFs::write(path, h, error);
}

bool RgbController::setZoneColor(int zone, const QColor &color, QString *error) {
    return setZoneHex(zone, colorToHex(color), error);
}

QString RgbController::zoneHex(int zone, bool *ok) {
    if (!isValidZone(zone)) { if (ok) *ok=false; return {}; }
    QString path = OmenDevice::rgbPath(QString("zone%1").arg(zone, 2, 10, QChar('0')));
    bool rOk;
    QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return {}; }
    // driver returns #RRGGBB on read
    QString h = SysFs::normalizeHex(data);
    if (!SysFs::isValidHexColor(h)) { if (ok) *ok=false; return {}; }
    if (ok) *ok=true;
    return h;
}

QColor RgbController::zoneColor(int zone, bool *ok) {
    QString hex = zoneHex(zone, ok);
    if (ok && !*ok) return QColor();
    return hexToColor(hex);
}

bool RgbController::setAllHex(const QString &hex, QString *error) {
    QString h = SysFs::normalizeHex(hex);
    if (!SysFs::isValidHexColor(h)) {
        if (error) *error = "Invalid hex color";
        return false;
    }
    QString path = OmenDevice::rgbPath(OmenDevice::Files::All);
    return SysFs::write(path, h, error);
}

bool RgbController::setAllZones(const QColor &color, QString *error) {
    return setAllHex(colorToHex(color), error);
}

bool RgbController::setBrightness(int percent, QString *error) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    QString path = OmenDevice::rgbPath(OmenDevice::Files::Brightness);
    return SysFs::write(path, QString::number(percent), error);
}

int RgbController::brightness(bool *ok) {
    QString path = OmenDevice::rgbPath(OmenDevice::Files::Brightness);
    bool rOk;
    QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return -1; }
    bool convOk;
    int v = data.toInt(&convOk);
    if (!convOk) { if (ok) *ok=false; return -1; }
    if (ok) *ok=true;
    return v;
}
