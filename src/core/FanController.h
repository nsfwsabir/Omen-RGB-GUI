#pragma once
#include <QObject>
#include <QList>
#include <QPair>

struct FanPoint {
    int temp; // 0-120
    int percent; // 0-100
    bool operator<(const FanPoint &o) const { return temp < o.temp; }
};

class FanController : public QObject {
    Q_OBJECT
public:
    explicit FanController(QObject *parent = nullptr);

    // RPM: returns -1 if n/a or error, sets *ok false if not available
    int cpuRpm(bool *ok = nullptr);
    int gpuRpm(bool *ok = nullptr);

    bool setMaxFan(bool enable, QString *error = nullptr);
    bool maxFan(bool *ok = nullptr);

    // thermal_profile: silent/normal/performance
    bool setThermalProfile(const QString &profile, QString *error = nullptr);
    QString thermalProfile(bool *ok = nullptr);
    static QStringList availableProfiles() { return {"silent","normal","performance"}; }

    // fan_curve: space-separated "temp:percent"
    bool setFanCurve(const QList<FanPoint> &points, QString *error = nullptr);
    bool setFanCurveString(const QString &str, QString *error = nullptr);
    QList<FanPoint> fanCurve(bool *ok = nullptr);
    QString fanCurveString(bool *ok = nullptr);
    static QString pointsToString(const QList<FanPoint> &pts);
    static QList<FanPoint> stringToPoints(const QString &str, bool *ok = nullptr);
    static bool validatePoints(const QList<FanPoint> &pts, QString *error = nullptr);

    bool setCurveEnable(bool enable, QString *error = nullptr);
    bool curveEnabled(bool *ok = nullptr);

    bool setTempZone(const QString &zone, QString *error = nullptr);
    QString tempZone(bool *ok = nullptr);
    QStringList availableTempZones(); // from /sys/class/thermal

    // Helpers for preset curves (built-in)
    static QList<FanPoint> presetSilent();
    static QList<FanPoint> presetNormal();
    static QList<FanPoint> presetPerformance();

    bool isFanAvailable(bool *ok = nullptr); // checks if RPM readable at all

    // Fan sysfs support (directory + files exist) - driver v1.3 has no fan, v1.5+ on Victus does
    static bool isFanSupported();
    static QString notSupportedMessage();
    bool isSupported() const { return isFanSupported(); }
};
