#pragma once
#include <QObject>
#include <QStringList>
#include <QMap>

struct GradientGroup {
    int zoneMask = 0; // bitmask 0-15
    QStringList colors; // hex strings RRGGBB
    QString toString() const; // zones:colors
};

class AnimationController : public QObject {
    Q_OBJECT
public:
    explicit AnimationController(QObject *parent = nullptr);

    static const QStringList Modes; // 11 modes
    static bool isValidMode(const QString &mode);
    static bool isValidSpeed(int s) { return s>=1 && s<=10; }

    bool setMode(const QString &mode, QString *error = nullptr);
    QString mode(bool *ok = nullptr);

    bool setSpeed(int speed, QString *error = nullptr);
    int speed(bool *ok = nullptr);

    // Gradient: format "0,1,2:FF0000,00FF00;3:800080,FFA500"
    bool setGradientConfig(const QString &cfg, QString *error = nullptr);
    QString gradientConfig(bool *ok = nullptr);

    // Helpers for building gradient strings
    static QString buildGradientConfig(const QList<GradientGroup> &groups);
    static QList<GradientGroup> parseGradientConfig(const QString &cfg, bool *ok = nullptr);
    static QStringList availableModes() { return Modes; }
};
