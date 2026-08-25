#include "FanController.h"
#include "OmenDevice.h"
#include "SysFs.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

FanController::FanController(QObject *parent) : QObject(parent) {}

int FanController::cpuRpm(bool *ok) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::CpuFanRpm);
    bool rOk;
    QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return -1; }
    data = data.trimmed();
    if (data=="n/a" || data.isEmpty()) { if (ok) *ok=false; return -1; }
    bool convOk;
    int v = data.toInt(&convOk);
    if (!convOk) { if (ok) *ok=false; return -1; }
    if (ok) *ok=true;
    return v;
}
int FanController::gpuRpm(bool *ok) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::GpuFanRpm);
    bool rOk;
    QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return -1; }
    data = data.trimmed();
    if (data=="n/a" || data.isEmpty()) { if (ok) *ok=false; return -1; }
    bool convOk;
    int v = data.toInt(&convOk);
    if (!convOk) { if (ok) *ok=false; return -1; }
    if (ok) *ok=true;
    return v;
}

bool FanController::setMaxFan(bool enable, QString *error) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::MaxFan);
    return SysFs::write(path, enable ? "1" : "0", error);
}
bool FanController::maxFan(bool *ok) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::MaxFan);
    bool rOk; QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return false; }
    if (ok) *ok=true;
    return data.trimmed()=="1";
}

bool FanController::setThermalProfile(const QString &profile, QString *error) {
    if (!availableProfiles().contains(profile)) {
        if (error) *error="Invalid profile (silent/normal/performance)";
        return false;
    }
    QString path = OmenDevice::fanPath(OmenDevice::Files::ThermalProfile);
    return SysFs::write(path, profile, error);
}
QString FanController::thermalProfile(bool *ok) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::ThermalProfile);
    bool rOk; QString data = SysFs::read(path, &rOk);
    if (ok) *ok=rOk;
    return data.trimmed();
}

QString FanController::pointsToString(const QList<FanPoint> &pts) {
    QStringList parts;
    for (auto &p : pts) parts << QString("%1:%2").arg(p.temp).arg(p.percent);
    return parts.join(" ");
}

QList<FanPoint> FanController::stringToPoints(const QString &str, bool *ok) {
    QList<FanPoint> out;
    QString trimmed = str.trimmed();
    if (trimmed.isEmpty() || trimmed=="(unset)") { if (ok) *ok=true; return out; }
    QStringList tokens = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    bool allOk=true;
    for (auto &tok : tokens) {
        int colon = tok.indexOf(":");
        if (colon==-1) { allOk=false; continue; }
        bool aOk,bOk;
        int t = tok.left(colon).toInt(&aOk);
        int pct = tok.mid(colon+1).toInt(&bOk);
        if (!aOk || !bOk) { allOk=false; continue; }
        out.append({t,pct});
    }
    if (ok) *ok=allOk;
    std::sort(out.begin(), out.end());
    return out;
}

bool FanController::validatePoints(const QList<FanPoint> &pts, QString *error) {
    if (pts.size()<2) { if (error) *error="Need at least 2 points"; return false; }
    if (pts.size()>8) { if (error) *error="Max 8 points"; return false; }
    for (auto &p: pts) {
        if (p.temp<0 || p.temp>120) { if (error) *error=QString("Temp %1 out of range 0-120").arg(p.temp); return false; }
        if (p.percent<0 || p.percent>100) { if (error) *error=QString("Percent %1 out of range 0-100").arg(p.percent); return false; }
    }
    return true;
}

bool FanController::setFanCurve(const QList<FanPoint> &points, QString *error) {
    if (!validatePoints(points, error)) return false;
    QString str = pointsToString(points);
    QString path = OmenDevice::fanPath(OmenDevice::Files::FanCurve);
    return SysFs::write(path, str, error);
}
bool FanController::setFanCurveString(const QString &str, QString *error) {
    bool ok;
    auto pts = stringToPoints(str, &ok);
    if (!ok) { if (error) *error="Invalid fan curve format (expected temp:percent ...)"; return false; }
    if (str.trimmed()!="(unset)" && !validatePoints(pts, error)) return false;
    QString path = OmenDevice::fanPath(OmenDevice::Files::FanCurve);
    return SysFs::write(path, str, error);
}

QList<FanPoint> FanController::fanCurve(bool *ok) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::FanCurve);
    bool rOk; QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return {}; }
    bool pOk;
    auto pts = stringToPoints(data, &pOk);
    if (ok) *ok=pOk;
    return pts;
}
QString FanController::fanCurveString(bool *ok) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::FanCurve);
    bool rOk; QString data = SysFs::read(path, &rOk);
    if (ok) *ok=rOk;
    return data.trimmed();
}

bool FanController::setCurveEnable(bool enable, QString *error) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::FanCurveEnable);
    return SysFs::write(path, enable ? "1" : "0", error);
}
bool FanController::curveEnabled(bool *ok) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::FanCurveEnable);
    bool rOk; QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return false; }
    if (ok) *ok=true;
    return data.trimmed()=="1";
}

bool FanController::setTempZone(const QString &zone, QString *error) {
    // Allow "(auto)" to mean empty? But driver expects name; we treat "(auto)" as not setting?
    // For now write zone as given; if "(auto)" we do nothing or write empty?
    // Valid zones checked via availableTempZones
    if (zone=="(auto)" || zone.isEmpty()) {
        // Writing empty not allowed; we just return true without write to indicate auto
        return true;
    }
    QStringList avail = availableTempZones();
    if (!avail.contains(zone)) {
        if (error) *error=QString("Unknown thermal zone: %1").arg(zone);
        return false;
    }
    QString path = OmenDevice::fanPath(OmenDevice::Files::FanTempZone);
    return SysFs::write(path, zone, error);
}
QString FanController::tempZone(bool *ok) {
    QString path = OmenDevice::fanPath(OmenDevice::Files::FanTempZone);
    bool rOk; QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return {}; }
    data = data.trimmed();
    if (data.isEmpty()) data="(auto)";
    if (ok) *ok=true;
    return data;
}

QStringList FanController::availableTempZones() {
    QStringList out;
    QDir dir(OmenDevice::thermalZonesBase());
    auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (auto &e : entries) {
        // each thermal_zoneN has file type
        QString typePath = OmenDevice::thermalZonesBase() + e + "/type";
        bool ok; QString t = SysFs::read(typePath, &ok);
        if (ok && !t.isEmpty()) out << t;
    }
    // also include common names even if not present? better only present
    // Ensure common are included for user visibility
    QStringList common = {"x86_pkg_temp","acpitz","k10temp","pch_cannonlake","pch_skylake","iwlwifi_1"};
    for (auto &c: common) if (!out.contains(c)) {
        // check if any zone type matches?
        // keep but maybe grey out? For now add if not present to aid manual
    }
    std::sort(out.begin(), out.end());
    out.removeDuplicates();
    return out;
}

QList<FanPoint> FanController::presetSilent() {
    return {{35,12},{50,20},{65,34},{80,52},{95,72}};
}
QList<FanPoint> FanController::presetNormal() {
    return {{35,22},{52,36},{68,55},{82,78},{98,95}};
}
QList<FanPoint> FanController::presetPerformance() {
    return {{30,35},{48,50},{62,68},{76,88},{92,100}};
}

bool FanController::isFanAvailable(bool *ok) {
    bool cpuOk, gpuOk;
    cpuRpm(&cpuOk);
    gpuRpm(&gpuOk);
    bool avail = cpuOk || gpuOk;
    if (ok) *ok=avail;
    return avail;
}
