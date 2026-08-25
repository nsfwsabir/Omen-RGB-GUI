#pragma once
#include <QObject>
#include <QString>

class DkmsInstaller : public QObject {
    Q_OBJECT
public:
    explicit DkmsInstaller(QObject *parent = nullptr);

    bool isDkmsInstalled();
    bool isDriverInstalled(bool *loaded = nullptr);
    bool isHpWmiLoaded();
    QString dkmsStatus();

    // These run privileged operations via pkexec bash -c "..."
    // Returns true on success, error string filled on failure. Emits log via signals.
    bool installDkmsPackage(QString *error = nullptr); // pacman -S dkms
    bool installDriver(const QString &sourceDir, QString *error = nullptr); // make install via pkexec
    bool blacklistHpWmi(QString *error = nullptr);
    bool reloadUdev(QString *error = nullptr);
    bool addUserToInputGroup(QString *error = nullptr);
    bool modprobeDriver(QString *error = nullptr);
    bool unloadHpWmi(QString *error = nullptr);

signals:
    void logMessage(const QString &msg);
    void progress(const QString &stage);
};
