#pragma once
#include <QObject>
#include <QColor>
#include <QStringList>

class RgbController : public QObject {
    Q_OBJECT
public:
    explicit RgbController(QObject *parent = nullptr);

    bool setZoneColor(int zone, const QColor &color, QString *error = nullptr);
    bool setZoneHex(int zone, const QString &hex, QString *error = nullptr);
    QColor zoneColor(int zone, bool *ok = nullptr);
    QString zoneHex(int zone, bool *ok = nullptr);

    bool setAllZones(const QColor &color, QString *error = nullptr);
    bool setAllHex(const QString &hex, QString *error = nullptr);

    bool setBrightness(int percent, QString *error = nullptr);
    int brightness(bool *ok = nullptr);

    // Helpers
    static QString colorToHex(const QColor &c);
    static QColor hexToColor(const QString &hex);
    static bool isValidZone(int z) { return z >=0 && z < 4; }
};
