#include "AnimationController.h"
#include "OmenDevice.h"
#include "SysFs.h"

const QStringList AnimationController::Modes = {
    "static","breathing","rainbow","wave","pulse","chase","sparkle","candle","aurora","disco","gradient"
};

AnimationController::AnimationController(QObject *parent) : QObject(parent) {}

bool AnimationController::isValidMode(const QString &mode) {
    return Modes.contains(mode);
}

bool AnimationController::setMode(const QString &mode, QString *error) {
    if (!isValidMode(mode)) {
        if (error) *error = "Invalid animation mode";
        return false;
    }
    QString path = OmenDevice::rgbPath(OmenDevice::Files::AnimationMode);
    return SysFs::write(path, mode, error);
}

QString AnimationController::mode(bool *ok) {
    QString path = OmenDevice::rgbPath(OmenDevice::Files::AnimationMode);
    bool rOk;
    QString data = SysFs::read(path, &rOk);
    if (ok) *ok = rOk;
    return data.trimmed();
}

bool AnimationController::setSpeed(int speed, QString *error) {
    if (!isValidSpeed(speed)) {
        if (error) *error = "Speed must be 1-10";
        return false;
    }
    QString path = OmenDevice::rgbPath(OmenDevice::Files::AnimationSpeed);
    return SysFs::write(path, QString::number(speed), error);
}

int AnimationController::speed(bool *ok) {
    QString path = OmenDevice::rgbPath(OmenDevice::Files::AnimationSpeed);
    bool rOk;
    QString data = SysFs::read(path, &rOk);
    if (!rOk) { if (ok) *ok=false; return -1; }
    bool convOk;
    int v = data.toInt(&convOk);
    if (!convOk) { if (ok) *ok=false; return -1; }
    if (ok) *ok=true;
    return v;
}

bool AnimationController::setGradientConfig(const QString &cfg, QString *error) {
    // Validate via parsing; driver limits 512 bytes, 4 groups, 16 colors per group
    if (cfg.size() > 512) {
        if (error) *error = "Gradient config too long (max 512)";
        return false;
    }
    // allow empty? driver returns EINVAL for empty? We allow empty to clear
    QString path = OmenDevice::rgbPath(OmenDevice::Files::GradientConfig);
    return SysFs::write(path, cfg, error);
}

QString AnimationController::gradientConfig(bool *ok) {
    QString path = OmenDevice::rgbPath(OmenDevice::Files::GradientConfig);
    bool rOk;
    QString data = SysFs::read(path, &rOk);
    if (ok) *ok = rOk;
    return data.trimmed();
}

QString GradientGroup::toString() const {
    QStringList zones;
    for (int i=0;i<4;++i) if (zoneMask & (1<<i)) zones << QString::number(i);
    return zones.join(",") + ":" + colors.join(",");
}

QString AnimationController::buildGradientConfig(const QList<GradientGroup> &groups) {
    QStringList parts;
    for (auto &g : groups) {
        if (g.zoneMask==0 || g.colors.isEmpty()) continue;
        parts << g.toString();
    }
    return parts.join(";");
}

QList<GradientGroup> AnimationController::parseGradientConfig(const QString &cfg, bool *ok) {
    QList<GradientGroup> out;
    if (cfg.trimmed().isEmpty()) { if (ok) *ok=true; return out; }
    QStringList groups = cfg.split(";", Qt::SkipEmptyParts);
    bool allOk = true;
    for (auto &grp : groups) {
        int colon = grp.indexOf(":");
        if (colon==-1) { allOk=false; continue; }
        QString zonePart = grp.left(colon).trimmed();
        QString colorPart = grp.mid(colon+1).trimmed();
        GradientGroup g;
        QStringList zTokens = zonePart.split(",", Qt::SkipEmptyParts);
        for (auto &zt : zTokens) {
            bool convOk; int z = zt.trimmed().toInt(&convOk);
            if (!convOk || z<0 || z>3) { allOk=false; continue; }
            g.zoneMask |= (1<<z);
        }
        QStringList cTokens = colorPart.split(",", Qt::SkipEmptyParts);
        for (auto &ct : cTokens) {
            QString h = SysFs::normalizeHex(ct);
            if (!SysFs::isValidHexColor(h)) { allOk=false; continue; }
            g.colors << h;
        }
        if (g.zoneMask!=0 && !g.colors.isEmpty()) out << g;
        else allOk=false;
    }
    if (ok) *ok = allOk;
    return out;
}
