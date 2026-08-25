#include "DkmsInstaller.h"
#include <QProcess>
#include <QFileInfo>
#include <QDebug>
#include <QCoreApplication>
#include "OmenDevice.h"
#include "SysFs.h"

DkmsInstaller::DkmsInstaller(QObject *parent): QObject(parent) {}

bool DkmsInstaller::isDkmsInstalled() {
    QProcess p;
    p.start("which", {"dkms"});
    p.waitForFinished(1000);
    return p.exitCode()==0;
}
bool DkmsInstaller::isDriverInstalled(bool *loaded) {
    bool l = OmenDevice::isDriverLoaded();
    if (loaded) *loaded = l;
    // check dkms status
    QProcess p;
    p.start("dkms", {"status"});
    p.waitForFinished(2000);
    QString out = p.readAllStandardOutput();
    return out.contains("omen-rgb-keyboard") || l;
}
bool DkmsInstaller::isHpWmiLoaded() {
    QProcess p;
    p.start("lsmod", {});
    p.waitForFinished(1000);
    return QString::fromUtf8(p.readAllStandardOutput()).contains("hp_wmi");
}
QString DkmsInstaller::dkmsStatus() {
    QProcess p;
    p.start("dkms", {"status"});
    p.waitForFinished(2000);
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

static bool runPkexec(const QString &bashCmd, QString *error, QString *output = nullptr) {
    QProcess p;
    // Use absolute path to match polkit policy annotation
    p.start("pkexec", {"/usr/bin/bash","-c", bashCmd});
    if (!p.waitForStarted(3000)) {
        p.start("pkexec", {"bash","-c", bashCmd});
        if (!p.waitForStarted(3000)) {
            if (error) *error = "pkexec failed to start: " + p.errorString();
            return false;
        }
    }
    if (!p.waitForFinished(60000)) {
        p.kill();
        if (error) *error = "pkexec command timeout";
        return false;
    }
    QString out = QString::fromUtf8(p.readAllStandardOutput());
    QString err = QString::fromUtf8(p.readAllStandardError());
    if (output) *output = out + err;
    if (p.exitCode()!=0) {
        if (error) *error = err.isEmpty() ? QString("Command failed (code %1): %2").arg(p.exitCode()).arg(out) : err;
        return false;
    }
    return true;
}

bool DkmsInstaller::installDkmsPackage(QString *error) {
    emit progress("Installing DKMS...");
    // Try pacman, apt, dnf in order
    QString cmd = "if command -v pacman >/dev/null 2>&1; then pacman -S --noconfirm dkms; "
                  "elif command -v apt >/dev/null 2>&1; then apt update && apt install -y dkms; "
                  "elif command -v dnf >/dev/null 2>&1; then dnf install -y dkms; "
                  "else echo 'No supported package manager'; exit 1; fi";
    return runPkexec(cmd, error);
}

bool DkmsInstaller::blacklistHpWmi(QString *error) {
    emit logMessage("Blacklisting hp_wmi...");
    QString cmd = "echo 'blacklist hp_wmi' > /etc/modprobe.d/blacklist-hp.conf && "
                  "echo 'hp_wmi blacklisted to /etc/modprobe.d/blacklist-hp.conf'";
    return runPkexec(cmd, error);
}

bool DkmsInstaller::unloadHpWmi(QString *error) {
    if (!isHpWmiLoaded()) return true;
    emit logMessage("Unloading hp_wmi...");
    QString cmd = "modprobe -r hp_wmi 2>&1 || true";
    return runPkexec(cmd, error);
}

bool DkmsInstaller::reloadUdev(QString *error) {
    emit logMessage("Reloading udev rules...");
    QString cmd = "udevadm control --reload-rules && udevadm trigger --subsystem-match=platform --attr-match=kernel=omen-rgb-keyboard 2>&1 || true; "
                  "echo 'udev reloaded'";
    return runPkexec(cmd, error);
}

bool DkmsInstaller::addUserToInputGroup(QString *error) {
    emit logMessage("Adding user to input group...");
    // Determine real user via SUDO_USER or logname
    QString cmd = "REAL_USER=${SUDO_USER:-$(logname 2>/dev/null || echo $USER)}; "
                  "if [ -n \"$REAL_USER\" ] && [ \"$REAL_USER\" != \"root\" ]; then "
                  "  usermod -aG input \"$REAL_USER\" && echo \"Added $REAL_USER to input\"; "
                  "else echo 'Could not determine user'; exit 1; fi";
    return runPkexec(cmd, error);
}

bool DkmsInstaller::modprobeDriver(QString *error) {
    emit logMessage("Loading omen_rgb_keyboard...");
    QString cmd = "modprobe omen_rgb_keyboard && echo 'module loaded' || (dmesg | tail -20; exit 1)";
    return runPkexec(cmd, error);
}

bool DkmsInstaller::installDriver(const QString &sourceDir, QString *error) {
    emit progress("Installing driver via DKMS...");
    QString src = sourceDir.isEmpty() ? QCoreApplication::applicationDirPath() : sourceDir;
    // Heuristic: find Makefile with dkms install. Try current dir, /usr/src/omen-*, and bundled source
    // For local cmake build, we need driver source. We check if sourceDir contains Makefile and dkms.conf
    QFileInfo fi(src + "/Makefile");
    QString actualSrc = src;
    if (!fi.exists()) {
        // Try parent of app dir? e.g. /home/.../omenRGB (where we are dev)
        // Also try /usr/src/omen-rgb-keyboard*
        // For now, try to locate via find
        QProcess find;
        find.start("bash", {"-c", "find /home -name dkms.conf -path '*omen-rgb*' 2>/dev/null | head -1 | xargs -r dirname"});
        find.waitForFinished(2000);
        QString found = QString::fromUtf8(find.readAllStandardOutput()).trimmed();
        if (!found.isEmpty() && QFileInfo::exists(found + "/Makefile")) {
            actualSrc = found;
        } else {
            // Fallback: assume driver source is not bundled; just try dkms install from current working dir if Makefile exists elsewhere
            // Try pwd
            QProcess pwd;
            pwd.start("bash", {"-c", "pwd"});
            pwd.waitForFinished(500);
            QString cwd = QString::fromUtf8(pwd.readAllStandardOutput()).trimmed();
            if (QFileInfo::exists(cwd + "/Makefile") && QFileInfo::exists(cwd + "/dkms.conf")) {
                actualSrc = cwd;
            }
        }
    }
    emit logMessage("Source dir: " + actualSrc);
    if (!QFileInfo::exists(actualSrc + "/Makefile")) {
        if (error) *error = "Driver source not found (need Makefile + dkms.conf in " + actualSrc + "). Clone https://github.com/OmenLinux/omen-rgb-keyboard first.";
        return false;
    }
    // Steps mimicking install.sh
    QString cmd = QString(
        "set -e; "
        "cd '%1'; "
        "echo '=== Installing DKMS module ==='; "
        "make install; "
        "echo '=== Creating modprobe conf ==='; "
        "if [ -f omen_rgb_keyboard.conf ]; then cp omen_rgb_keyboard.conf /etc/modprobe.d/; fi; "
        "echo 'omen_rgb_keyboard' > /etc/modules-load.d/omen_rgb_keyboard.conf; "
        "mkdir -p /var/lib/omen-rgb-keyboard; chmod 755 /var/lib/omen-rgb-keyboard; "
        "echo '=== Installing mute monitor ==='; "
        "if [ -f scripts/omen-mute-monitor.sh ]; then "
        "  cp scripts/omen-mute-monitor.sh /usr/local/bin/omen-mute-monitor; chmod +x /usr/local/bin/omen-mute-monitor; "
        "  mkdir -p /etc/systemd/user; "
        "  cat > /etc/systemd/user/omen-mute-monitor.service <<'EOSVC'\n"
        "[Unit]\n"
        "Description=HP OMEN Mute LED Monitor (PipeWire/Bluetooth)\n"
        "After=pipewire.service wireplumber.service\n"
        "Wants=pipewire.service\n"
        "[Service]\n"
        "Type=simple\n"
        "ExecStart=/usr/local/bin/omen-mute-monitor\n"
        "Restart=on-failure\n"
        "RestartSec=5\n"
        "[Install]\n"
        "WantedBy=default.target\n"
        "EOSVC\n"
        "  mkdir -p /etc/tmpfiles.d; echo 'w /sys/devices/platform/omen-rgb-keyboard/rgb_zones/mute_state - - - - 0666' > /etc/tmpfiles.d/omen-mute-monitor.conf; "
        "  if [ -f /sys/devices/platform/omen-rgb-keyboard/rgb_zones/mute_state ]; then chmod 666 /sys/devices/platform/omen-rgb-keyboard/rgb_zones/mute_state 2>/dev/null || true; fi; "
        "  systemd-tmpfiles --create /etc/tmpfiles.d/omen-mute-monitor.conf 2>/dev/null || true; "
        "fi; "
        "if [ -f 99-omen-rgb-keyboard.rules ]; then cp 99-omen-rgb-keyboard.rules /etc/udev/rules.d/ && chmod 644 /etc/udev/rules.d/99-omen-rgb-keyboard.rules; fi; "
        "if [ -f install-udev-rules.sh ]; then bash install-udev-rules.sh 2>&1 || true; fi; "
        "udevadm control --reload-rules 2>/dev/null || true; "
        "udevadm trigger --subsystem-match=platform --attr-match=kernel=omen-rgb-keyboard 2>/dev/null || true; "
        "echo '=== Modprobe ==='; "
        "modprobe omen_rgb_keyboard 2>&1 || (dmesg | tail -30; exit 1); "
        "echo 'Install complete'"
    ).arg(actualSrc);

    QString out;
    bool ok = runPkexec(cmd, error, &out);
    if (!out.isEmpty()) emit logMessage(out);
    return ok;
}
