#include "SysFs.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QProcess>
#include <QDebug>

QString SysFs::lastError;

SysFs::SysFs(QObject *parent) : QObject(parent) {}

QString SysFs::read(const QString &path, bool *ok) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok) *ok = false;
        lastError = f.errorString();
        return QString();
    }
    QTextStream ts(&f);
    QString data = ts.readAll().trimmed();
    // kernel often adds newline; also strips leading '#'
    f.close();
    if (ok) *ok = true;
    return data;
}

bool SysFs::exists(const QString &path) {
    return QFileInfo::exists(path);
}

bool SysFs::isWritable(const QString &path) {
    QFileInfo fi(path);
    if (!fi.exists()) return false;
    return fi.isWritable();
}

bool SysFs::write(const QString &path, const QString &data, QString *error) {
    // Ensure data ends with newline (kernel expects newline terminated)
    QString payload = data;
    if (!payload.endsWith("\n")) payload += "\n";

    QFile f(path);
    // Try direct write first
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << payload;
        f.close();
        if (f.error() == QFile::NoError) {
            return true;
        }
        // fall through to pkexec if error
        lastError = f.errorString();
        if (error) *error = lastError;
    }

    // Fallback via pkexec tee
    // Use pkexec + tee to handle non-writable due to permissions (input group vs root)
    // Command: pkexec tee <path> >/dev/null
    // Write payload via stdin
    QProcess proc;
    // Use sh -c "pkexec tee ..." to handle redirect properly, or use tee directly via pkexec
    // pkexec handles auth dialog (polkit)
    proc.start("pkexec", {"tee", path});
    if (!proc.waitForStarted(3000)) {
        QString err = "pkexec failed to start: " + proc.errorString();
        lastError = err;
        if (error) *error = err;
        return false;
    }
    proc.write(payload.toUtf8());
    proc.closeWriteChannel();
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        QString err = "pkexec tee timeout";
        lastError = err;
        if (error) *error = err;
        return false;
    }
    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (err.isEmpty()) err = QString("pkexec tee failed (code %1)").arg(proc.exitCode());
        lastError = err;
        if (error) *error = err;
        return false;
    }
    return true;
}

bool SysFs::isValidHexColor(const QString &hex) {
    static QRegularExpression re("^[0-9a-fA-F]{6}$");
    QString h = stripHash(hex.trimmed());
    return re.match(h).hasMatch();
}

QString SysFs::stripHash(const QString &s) {
    QString t = s.trimmed();
    if (t.startsWith("#")) return t.mid(1);
    return t;
}

QString SysFs::normalizeHex(const QString &hex) {
    QString h = stripHash(hex.trimmed()).toUpper();
    return h;
}
