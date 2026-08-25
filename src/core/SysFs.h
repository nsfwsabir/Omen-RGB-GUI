#pragma once
#include <QString>
#include <QMap>
#include <QObject>

class SysFs : public QObject {
    Q_OBJECT
public:
    explicit SysFs(QObject *parent = nullptr);

    // Read sysfs file, trimmed. Returns empty string on error. Sets *ok if provided.
    static QString read(const QString &path, bool *ok = nullptr);
    // Write sysfs file. Tries direct write; if not writable attempts one-time permission fix
    // via pkexec (chgrp/chmod + udev), then retries direct, then falls back to pkexec tee.
    // This ensures only one authentication prompt per session instead of per-setting.
    static bool write(const QString &path, const QString &data, QString *error = nullptr);
    // Batch write multiple files in a single pkexec invocation (preset optimization)
    static bool writeBatch(const QMap<QString, QString> &writes, QString *error = nullptr);

    static bool exists(const QString &path);
    static bool isWritable(const QString &path);
    static QString lastError;

    // One-time permission fix: chgrp input + chmod g+w for all sysfs nodes, install udev rule
    static bool fixPermissions(QString *error = nullptr);
    static void resetPermissionsCache();

    // Helpers
    static bool isValidHexColor(const QString &hex); // 6 hex chars, no #
    static QString normalizeHex(const QString &hex); // uppercase, no #, 6 chars
    static QString stripHash(const QString &s);

private:
    static bool s_fixAttempted;
    static bool s_fixSuccess;
    static bool writeViaPkexec(const QString &path, const QString &data, QString *error);
    static bool writeBatchViaPkexec(const QMap<QString, QString> &writes, QString *error);
    static QString shellEscape(const QString &s);
};
