#include "MuteLedController.h"
#include "OmenDevice.h"
#include "SysFs.h"
#include <QProcess>

MuteLedController::MuteLedController(QObject *parent) : QObject(parent) {}

bool MuteLedController::isAvailable(bool *ok) {
    bool exists = SysFs::exists(OmenDevice::rgbPath(OmenDevice::Files::MuteLed));
    if (ok) *ok = exists;
    return exists;
}

bool MuteLedController::setMuteLed(bool on, QString *error) {
    QString path = OmenDevice::rgbPath(OmenDevice::Files::MuteLed);
    return SysFs::write(path, on ? "1" : "0", error);
}

bool MuteLedController::setMuteState(bool muted, QString *error) {
    QString path = OmenDevice::rgbPath(OmenDevice::Files::MuteState);
    return SysFs::write(path, muted ? "1" : "0", error);
}

bool MuteLedController::isServiceActive(bool *ok) {
    QProcess p;
    p.start("systemctl", {"--user","is-active","omen-mute-monitor.service"});
    p.waitForFinished(1000);
    QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    bool active = (out=="active");
    if (ok) *ok = (p.exitCode()==0 || out=="active" || out=="inactive");
    Q_UNUSED(err);
    return active;
}
bool MuteLedController::isServiceEnabled(bool *ok) {
    QProcess p;
    p.start("systemctl", {"--user","is-enabled","omen-mute-monitor.service"});
    p.waitForFinished(1000);
    QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    bool enabled = (out=="enabled");
    if (ok) *ok = (p.exitCode()==0 || out=="enabled" || out=="disabled");
    return enabled;
}
QString MuteLedController::serviceStatus() {
    QProcess p;
    p.start("systemctl", {"--user","status","omen-mute-monitor.service"});
    p.waitForFinished(1000);
    return QString::fromUtf8(p.readAllStandardOutput());
}
