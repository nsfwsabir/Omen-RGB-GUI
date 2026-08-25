#include "OmenDevice.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
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
    // Try sysfs first (works even when modinfo fails for DKMS mismatch)
    QFile f("/sys/module/omen_rgb_keyboard/version");
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString v = QString::fromUtf8(f.readAll()).trimmed();
        f.close();
        if (!v.isEmpty() && v != "unknown") return v;
    }
    QProcess p;
    p.start("modinfo", {"-F", "version", "omen_rgb_keyboard"});
    p.waitForFinished(1000);
    QString v = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (v.isEmpty()) {
        // fallback to modinfo without -F, or dkms
        QProcess p2;
        p2.start("dkms", {"status"});
        p2.waitForFinished(1000);
        QString out = QString::fromUtf8(p2.readAllStandardOutput());
        QRegularExpression re("omen-rgb-keyboard/([^,]+)");
        auto m = re.match(out);
        if (m.hasMatch()) return m.captured(1);
        v = "unknown";
    }
    if (v.isEmpty()) v = "unknown";
    return v;
}

QString thermalZonesBase() {
    return "/sys/class/thermal/";
}

}
