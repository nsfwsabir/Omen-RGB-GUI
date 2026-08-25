#pragma once
#include <QString>
#include <QObject>

class SysFs : public QObject {
    Q_OBJECT
public:
    explicit SysFs(QObject *parent = nullptr);

    // Read sysfs file, trimmed. Returns empty string on error. Sets *ok if provided.
    static QString read(const QString &path, bool *ok = nullptr);
    // Write sysfs file. If not writable, fallback to pkexec tee.
    // Returns true on success, false otherwise. Error string filled if provided.
    static bool write(const QString &path, const QString &data, QString *error = nullptr);

    static bool exists(const QString &path);
    static bool isWritable(const QString &path);
    static QString lastError;

    // Helpers
    static bool isValidHexColor(const QString &hex); // 6 hex chars, no #
    static QString normalizeHex(const QString &hex); // uppercase, no #, 6 chars
    static QString stripHash(const QString &s);
};
