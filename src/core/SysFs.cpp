#include "SysFs.h"
#include "OmenDevice.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QProcess>
#include <QDebug>
#include <QDir>

QString SysFs::lastError;
bool SysFs::s_fixAttempted = false;
bool SysFs::s_fixSuccess = false;

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

QString SysFs::shellEscape(const QString &s) {
    // Escape for single-quoted shell string: ' -> '\''
    QString out;
    out.reserve(s.size() + 10);
    for (QChar c : s) {
        if (c == QLatin1Char('\'')) out += QStringLiteral("'\\''");
        else out += c;
    }
    return out;
}

bool SysFs::fixPermissions(QString *error) {
    if (s_fixAttempted) {
        if (s_fixSuccess) return true;
        if (error) *error = lastError;
        return false;
    }
    s_fixAttempted = true;

    // Mock environment: OMEN_RGB_ROOT points to writable temp dir, just chmod directly
    QByteArray mockRoot = qgetenv("OMEN_RGB_ROOT");
    if (!mockRoot.isEmpty()) {
        QString base = QString::fromUtf8(mockRoot);
        if (!base.endsWith("/")) base += "/";
        QStringList paths;
        paths << base + "rgb_zones/zone00" << base + "rgb_zones/zone01" << base + "rgb_zones/zone02"
              << base + "rgb_zones/zone03" << base + "rgb_zones/all" << base + "rgb_zones/brightness"
              << base + "rgb_zones/animation_mode" << base + "rgb_zones/animation_speed"
              << base + "rgb_zones/gradient_config" << base + "rgb_zones/mute_led"
              << base + "rgb_zones/mute_state" << base + "fan/cpu_fan_rpm" << base + "fan/gpu_fan_rpm"
              << base + "fan/max_fan" << base + "fan/thermal_profile" << base + "fan/fan_curve"
              << base + "fan/fan_curve_enable" << base + "fan/fan_temp_zone";
        for (const QString &p : paths) {
            QFileInfo fi(p);
            if (fi.exists()) {
                QFile::setPermissions(p, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther);
            }
        }
        s_fixSuccess = true;
        return true;
    }

    QString platform = OmenDevice::platformBase(); // ends with /
    if (!platform.endsWith("/")) platform += "/";
    QString rgb = platform + "rgb_zones";
    QString fan = platform + "fan";
    QString ledBrightness = "/sys/class/leds/omen::kbd_backlight/brightness";

    // Build bash command that fixes permissions and ensures udev rule exists for future boots
    // Uses chgrp input + chmod g+w. Handles missing fan dir gracefully.
    QString cmd;
    cmd += QStringLiteral("set -e; ");
    cmd += QStringLiteral("RGB='") + shellEscape(rgb) + QStringLiteral("'; ");
    cmd += QStringLiteral("FAN='") + shellEscape(fan) + QStringLiteral("'; ");
    cmd += QStringLiteral("LED='") + shellEscape(ledBrightness) + QStringLiteral("'; ");
    // Fix rgb_zones
    cmd += QStringLiteral("if [ -d \"$RGB\" ]; then chgrp -R input \"$RGB\" 2>/dev/null || true; chmod -R g+w \"$RGB\" 2>/dev/null || true; for f in \"$RGB\"/*; do [ -e \"$f\" ] && chgrp input \"$f\" 2>/dev/null || true; chmod g+w \"$f\" 2>/dev/null || true; done; fi; ");
    // Fix fan
    cmd += QStringLiteral("if [ -d \"$FAN\" ]; then chgrp -R input \"$FAN\" 2>/dev/null || true; chmod -R g+w \"$FAN\" 2>/dev/null || true; for f in \"$FAN\"/*; do [ -e \"$f\" ] && chgrp input \"$f\" 2>/dev/null || true; chmod g+w \"$f\" 2>/dev/null || true; done; fi; ");
    // Fix LED
    cmd += QStringLiteral("if [ -e \"$LED\" ]; then chgrp input \"$LED\" 2>/dev/null || true; chmod g+w \"$LED\" 2>/dev/null || true; fi; ");
    // Ensure udev rule exists and is up-to-date for persistence across reboots
    // Overwrite if missing or old/broken (no RUN)
    cmd += QStringLiteral("if [ ! -f /etc/udev/rules.d/99-omen-rgb-keyboard.rules ] || ! grep -q 'RUN' /etc/udev/rules.d/99-omen-rgb-keyboard.rules 2>/dev/null; then ");
    cmd += QStringLiteral("cat > /etc/udev/rules.d/99-omen-rgb-keyboard.rules <<'OMENUDEV'\n");
    cmd += QStringLiteral("# HP OMEN RGB Keyboard udev rules - auto-installed by omen-rgb to avoid repeated pkexec\n");
    cmd += QStringLiteral("ACTION==\"add\", SUBSYSTEM==\"platform\", KERNEL==\"omen-rgb-keyboard\", RUN+=\"/bin/sh -c 'chgrp -R input /sys/devices/platform/omen-rgb-keyboard/rgb_zones 2>/dev/null; chmod -R g+w /sys/devices/platform/omen-rgb-keyboard/rgb_zones 2>/dev/null; for f in /sys/devices/platform/omen-rgb-keyboard/rgb_zones/*; do [ -e \\\"$f\\\" ] && chgrp input \\\"$f\\\" 2>/dev/null; chmod g+w \\\"$f\\\" 2>/dev/null; done'\"\n");
    cmd += QStringLiteral("ACTION==\"add\", SUBSYSTEM==\"platform\", KERNEL==\"omen-rgb-keyboard\", RUN+=\"/bin/sh -c 'chgrp -R input /sys/devices/platform/omen-rgb-keyboard/fan 2>/dev/null; chmod -R g+w /sys/devices/platform/omen-rgb-keyboard/fan 2>/dev/null; for f in /sys/devices/platform/omen-rgb-keyboard/fan/*; do [ -e \\\"$f\\\" ] && chgrp input \\\"$f\\\" 2>/dev/null; chmod g+w \\\"$f\\\" 2>/dev/null; done'\"\n");
    cmd += QStringLiteral("ACTION==\"add\", SUBSYSTEM==\"leds\", KERNEL==\"omen::kbd_backlight\", RUN+=\"/bin/sh -c 'chgrp input /sys/class/leds/omen::kbd_backlight/brightness 2>/dev/null; chmod g+w /sys/class/leds/omen::kbd_backlight/brightness 2>/dev/null'\"\n");
    cmd += QStringLiteral("SUBSYSTEM==\"platform\", KERNEL==\"omen-rgb-keyboard\", GROUP=\"input\", MODE=\"0664\"\n");
    cmd += QStringLiteral("SUBSYSTEM==\"leds\", KERNEL==\"omen::kbd_backlight\", GROUP=\"input\", MODE=\"0664\"\n");
    cmd += QStringLiteral("OMENUDEV\n");
    cmd += QStringLiteral("chmod 644 /etc/udev/rules.d/99-omen-rgb-keyboard.rules; udevadm control --reload-rules 2>/dev/null || true; udevadm trigger --subsystem-match=platform --attr-match=kernel=omen-rgb-keyboard 2>/dev/null || true; ");
    cmd += QStringLiteral("fi; ");
    cmd += QStringLiteral("echo fixed");

    QProcess proc;
    // Use /usr/bin/sh to match fixperm policy (distinct from install's /usr/bin/bash)
    proc.start("pkexec", {"/usr/bin/sh", "-c", cmd});
    if (!proc.waitForStarted(3000)) {
        proc.start("pkexec", {"/usr/bin/bash", "-c", cmd});
        if (!proc.waitForStarted(3000)) {
            proc.start("pkexec", {"sh", "-c", cmd});
            if (!proc.waitForStarted(3000)) {
                QString err = "pkexec failed to start: " + proc.errorString();
                lastError = err;
                if (error) *error = err;
                s_fixSuccess = false;
                return false;
            }
        }
    }
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        QString err = "fixPermissions timeout";
        lastError = err;
        if (error) *error = err;
        s_fixSuccess = false;
        return false;
    }
    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (err.isEmpty()) err = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (err.isEmpty()) err = QString("fixPermissions failed (code %1)").arg(proc.exitCode());
        lastError = err;
        if (error) *error = err;
        s_fixSuccess = false;
        return false;
    }
    s_fixSuccess = true;
    return true;
}

void SysFs::resetPermissionsCache() {
    s_fixAttempted = false;
    s_fixSuccess = false;
}

bool SysFs::writeViaPkexec(const QString &path, const QString &data, QString *error) {
    QString payload = data;
    if (!payload.endsWith("\n")) payload += "\n";
    QProcess proc;
    // Use absolute path to match polkit policy annotation
    proc.start("pkexec", {"/usr/bin/tee", path});
    if (!proc.waitForStarted(3000)) {
        // Fallback to plain tee if /usr/bin/tee not available
        proc.start("pkexec", {"tee", path});
        if (!proc.waitForStarted(3000)) {
            QString err = "pkexec failed to start: " + proc.errorString();
            lastError = err;
            if (error) *error = err;
            return false;
        }
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

bool SysFs::writeBatchViaPkexec(const QMap<QString, QString> &writes, QString *error) {
    if (writes.isEmpty()) return true;
    // Build single bash command that writes all files atomically in one pkexec
    QString cmd;
    for (auto it = writes.cbegin(); it != writes.cend(); ++it) {
        QString path = it.key();
        QString data = it.value().trimmed();
        // Use printf '%s\n' to ensure newline without embedding literal newline in the shell command
        cmd += QStringLiteral("printf '%s\\n' '") + shellEscape(data) + QStringLiteral("' > '") + shellEscape(path) + QStringLiteral("'; ");
        cmd += QStringLiteral("if [ $? -ne 0 ]; then echo \"failed ") + shellEscape(path) + QStringLiteral("\" >&2; exit 1; fi; ");
    }
    cmd += QStringLiteral("echo ok");

    QProcess proc;
    proc.start("pkexec", {"/usr/bin/bash", "-c", cmd});
    if (!proc.waitForStarted(3000)) {
        proc.start("pkexec", {"bash", "-c", cmd});
        if (!proc.waitForStarted(3000)) {
            QString err = "pkexec failed to start: " + proc.errorString();
            lastError = err;
            if (error) *error = err;
            return false;
        }
    }
    if (!proc.waitForFinished(8000)) {
        proc.kill();
        QString err = "pkexec batch timeout";
        lastError = err;
        if (error) *error = err;
        return false;
    }
    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (err.isEmpty()) err = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (err.isEmpty()) err = QString("pkexec batch failed (code %1)").arg(proc.exitCode());
        lastError = err;
        if (error) *error = err;
        return false;
    }
    return true;
}

bool SysFs::write(const QString &path, const QString &data, QString *error) {
    QString payload = data;
    if (!payload.endsWith("\n")) payload += "\n";

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << payload;
        f.close();
        if (f.error() == QFile::NoError) {
            return true;
        }
        lastError = f.errorString();
        if (error) *error = lastError;
        // fall through to permission fix
    } else {
        lastError = f.errorString();
        if (error) *error = lastError;
    }

    // Try one-time permission fix (chgrp/chmod + udev) to make future writes passwordless
    if (!s_fixAttempted) {
        QString fixErr;
        if (fixPermissions(&fixErr)) {
            // Retry direct write after fixing permissions
            QFile f2(path);
            if (f2.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&f2);
                ts << payload;
                f2.close();
                if (f2.error() == QFile::NoError) {
                    return true;
                }
                lastError = f2.errorString();
                if (error) *error = lastError;
            }
        } else {
            // fix failed, will fall back to pkexec; preserve fix error if no other error
            if (error && error->isEmpty()) *error = fixErr;
        }
    } else if (s_fixSuccess) {
        // Already fixed successfully but direct still failed (e.g., file appeared after fix)
        // try direct once more quickly before pkexec
        QFile f2(path);
        if (f2.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f2);
            ts << payload;
            f2.close();
            if (f2.error() == QFile::NoError) return true;
        }
    }

    // Final fallback: single pkexec tee (will use cached polkit auth if available)
    return writeViaPkexec(path, data, error);
}

bool SysFs::writeBatch(const QMap<QString, QString> &writes, QString *error) {
    if (writes.isEmpty()) return true;
    // First, try direct writes for all entries
    QMap<QString, QString> failed;
    for (auto it = writes.cbegin(); it != writes.cend(); ++it) {
        QString payload = it.value();
        if (!payload.endsWith("\n")) payload += "\n";
        QFile f(it.key());
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << payload;
            f.close();
            if (f.error() == QFile::NoError) continue;
            lastError = f.errorString();
        } else {
            lastError = f.errorString();
        }
        failed.insert(it.key(), it.value());
    }
    if (failed.isEmpty()) return true;

    // Attempt one-time permission fix to make remaining writes direct
    if (!s_fixAttempted) {
        QString fixErr;
        if (fixPermissions(&fixErr)) {
            QMap<QString, QString> stillFailed;
            for (auto it = failed.cbegin(); it != failed.cend(); ++it) {
                QString payload = it.value();
                if (!payload.endsWith("\n")) payload += "\n";
                QFile f(it.key());
                if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream ts(&f);
                    ts << payload;
                    f.close();
                    if (f.error() == QFile::NoError) continue;
                }
                stillFailed.insert(it.key(), it.value());
            }
            failed = stillFailed;
            if (failed.isEmpty()) return true;
        }
    } else if (s_fixSuccess) {
        // Already fixed, retry direct once before pkexec
        QMap<QString, QString> stillFailed;
        for (auto it = failed.cbegin(); it != failed.cend(); ++it) {
            QString payload = it.value();
            if (!payload.endsWith("\n")) payload += "\n";
            QFile f(it.key());
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&f);
                ts << payload;
                f.close();
                if (f.error() == QFile::NoError) continue;
            }
            stillFailed.insert(it.key(), it.value());
        }
        failed = stillFailed;
        if (failed.isEmpty()) return true;
    }

    // Single pkexec for all remaining failures
    return writeBatchViaPkexec(failed, error);
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
