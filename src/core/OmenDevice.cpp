#include "OmenDevice.h"
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>

namespace OmenDevice {

QString rgbBase() {
    QByteArray env = qgetenv("OMEN_RGB_ROOT");
    if (!env.isEmpty()) {
        QString base = QString::fromUtf8(env);
        if (!base.endsWith("/")) base += "/";
        return base + "rgb_zones/";
    }
    return "/sys/devices/platform/omen-rgb-keyboard/rgb_zones/";
}

QString fanBase() {
    QByteArray env = qgetenv("OMEN_RGB_ROOT");
    if (!env.isEmpty()) {
        QString base = QString::fromUtf8(env);
        if (!base.endsWith("/")) base += "/";
        return base + "fan/";
    }
    return "/sys/devices/platform/omen-rgb-keyboard/fan/";
}

QString platformBase() {
    QByteArray env = qgetenv("OMEN_RGB_ROOT");
    if (!env.isEmpty()) {
        return QString::fromUtf8(env) + "/";
    }
    return "/sys/devices/platform/omen-rgb-keyboard/";
}

bool isDriverLoaded() {
    QFileInfo fi("/sys/devices/platform/omen-rgb-keyboard");
    if (fi.exists()) return true;
    QProcess p;
    p.start("lsmod", {});
    p.waitForFinished(1000);
    QString out = p.readAllStandardOutput();
    return out.contains("omen_rgb_keyboard");
}

QString driverVersion() {
    QProcess p;
    p.start("modinfo", {"-F", "version", "omen_rgb_keyboard"});
    p.waitForFinished(1000);
    QString v = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (v.isEmpty()) v = "unknown";
    return v;
}

QString thermalZonesBase() {
    return "/sys/class/thermal/";
}

}
