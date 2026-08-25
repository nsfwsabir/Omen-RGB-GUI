#pragma once
#include <QObject>

class MuteLedController : public QObject {
    Q_OBJECT
public:
    explicit MuteLedController(QObject *parent = nullptr);

    bool setMuteLed(bool on, QString *error = nullptr);
    // reading mute_led returns help text, not state; we treat as not readable for state

    bool setMuteState(bool muted, QString *error = nullptr);
    // mute_state also not reliably readable? but we can read last written
    bool isServiceActive(bool *ok = nullptr);
    bool isServiceEnabled(bool *ok = nullptr);
    QString serviceStatus();

    bool isAvailable(bool *ok = nullptr); // checks sysfs exists
};
