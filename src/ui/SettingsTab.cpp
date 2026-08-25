#include "SettingsTab.h"
#include "core/DkmsInstaller.h"
#include "core/MuteLedController.h"
#include "core/OmenDevice.h"
#include "core/SysFs.h"
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QTextEdit>
#include <QMessageBox>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>

SettingsTab::SettingsTab(QWidget *parent) : QWidget(parent), m_installer(new DkmsInstaller(this)), m_mute(new MuteLedController(this)) {
    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(12,12,12,12);
    main->setSpacing(10);

    // Driver group
    auto *drvBox = new QGroupBox("Driver & DKMS (bundled build)", this);
    auto *drvLay = new QVBoxLayout(drvBox);
    m_driverStatus = new QLabel(drvBox);
    m_dkmsStatus = new QLabel(drvBox);
    m_hpWmiStatus = new QLabel(drvBox);
    m_permStatus = new QLabel(drvBox);
    for (auto *l : {m_driverStatus, m_dkmsStatus, m_hpWmiStatus, m_permStatus}) { l->setWordWrap(true); l->setTextFormat(Qt::RichText); }
    drvLay->addWidget(m_driverStatus);
    drvLay->addWidget(m_dkmsStatus);
    drvLay->addWidget(m_hpWmiStatus);
    drvLay->addWidget(m_permStatus);

    auto *btnRow1 = new QHBoxLayout();
    auto *checkBtn = new QPushButton("Refresh Status", drvBox);
    auto *installBtn = new QPushButton("Install Driver (DKMS)", drvBox);
    installBtn->setToolTip("Runs pkexec make install + modprobe (requires driver source)");
    auto *dmesgBtn = new QPushButton("Show dmesg", drvBox);
    btnRow1->addWidget(checkBtn); btnRow1->addWidget(installBtn); btnRow1->addWidget(dmesgBtn); btnRow1->addStretch();
    drvLay->addLayout(btnRow1);

    auto *btnRow2 = new QHBoxLayout();
    auto *udevBtn = new QPushButton("Install Udev Rules", drvBox);
    auto *reloadUdevBtn = new QPushButton("Reload Udev", drvBox);
    auto *inputBtn = new QPushButton("Add User to 'input' group", drvBox);
    auto *permBtn = new QPushButton("Check Permissions", drvBox);
    btnRow2->addWidget(udevBtn); btnRow2->addWidget(reloadUdevBtn); btnRow2->addWidget(inputBtn); btnRow2->addWidget(permBtn); btnRow2->addStretch();
    drvLay->addLayout(btnRow2);

    main->addWidget(drvBox);
    connect(checkBtn, &QPushButton::clicked, this, &SettingsTab::checkDriver);
    connect(installBtn, &QPushButton::clicked, this, &SettingsTab::installDriver);
    connect(dmesgBtn, &QPushButton::clicked, this, &SettingsTab::showDmesg);
    connect(udevBtn, &QPushButton::clicked, this, &SettingsTab::installUdev);
    connect(reloadUdevBtn, &QPushButton::clicked, this, &SettingsTab::reloadUdev);
    connect(inputBtn, &QPushButton::clicked, this, &SettingsTab::addToInputGroup);
    connect(permBtn, &QPushButton::clicked, this, &SettingsTab::checkPermissions);
    connect(m_installer, &DkmsInstaller::logMessage, this, [this](const QString &msg){
        m_log->append(msg);
    });

    // Mute LED
    auto *muteBox = new QGroupBox("Mute Button LED (HDA + PipeWire)", this);
    auto *muteLay = new QVBoxLayout(muteBox);
    m_muteStatus = new QLabel(muteBox);
    m_muteStatus->setWordWrap(true);
    muteLay->addWidget(m_muteStatus);
    auto *muteRow = new QHBoxLayout();
    m_muteLedBtn = new QPushButton("Toggle Mute LED (manual, disables auto-sync)", muteBox);
    auto *muteOn = new QPushButton("Set Muted (1)", muteBox);
    auto *muteOff = new QPushButton("Set Unmuted (0)", muteBox);
    auto *muteRefresh = new QPushButton("Refresh", muteBox);
    muteRow->addWidget(m_muteLedBtn); muteRow->addWidget(muteOn); muteRow->addWidget(muteOff); muteRow->addWidget(muteRefresh); muteRow->addStretch();
    muteLay->addLayout(muteRow);
    muteLay->addWidget(new QLabel("Auto-sync polls ALSA 200ms; PipeWire via omen-mute-monitor service (wpctl). Manual mute_led disables auto-sync until reload.", muteBox));
    main->addWidget(muteBox);
    connect(m_muteLedBtn, &QPushButton::clicked, this, &SettingsTab::toggleMuteLed);
    connect(muteOn, &QPushButton::clicked, this, [this](){ QString e; if(!m_mute->setMuteState(true,&e)) QMessageBox::critical(this,"Failed",e); else refreshMuteStatus(); });
    connect(muteOff, &QPushButton::clicked, this, [this](){ QString e; if(!m_mute->setMuteState(false,&e)) QMessageBox::critical(this,"Failed",e); else refreshMuteStatus(); });
    connect(muteRefresh, &QPushButton::clicked, this, &SettingsTab::refreshMuteStatus);

    // Omen Key + Autostart
    auto *miscBox = new QGroupBox("Omen Key & System", this);
    auto *miscLay = new QVBoxLayout(miscBox);
    auto *omenInfo = new QLabel("Omen key is mapped to <b>KEY_MSDOS</b> (XF86DOS). Bind in: GNOME Settings → Keyboard → Shortcuts, KDE Shortcuts → Custom, or i3/Sway: <code>bindsym XF86DOS exec your-command</code>. Remap by editing <code>src/wmi/omen_wmi.c</code> and rebuilding.", miscBox);
    omenInfo->setWordWrap(true); omenInfo->setTextFormat(Qt::RichText);
    miscLay->addWidget(omenInfo);
    auto *omenBtn = new QPushButton("Show Omen Key Mapping Help", miscBox);
    miscLay->addWidget(omenBtn);
    connect(omenBtn, &QPushButton::clicked, this, &SettingsTab::showOmenKeyInfo);

    m_autostartCheck = new QCheckBox("Autostart on login (system tray)", miscBox);
    QString autostartPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/omen-rgb.desktop";
    m_autostartCheck->setChecked(QFileInfo::exists(autostartPath));
    miscLay->addWidget(m_autostartCheck);
    connect(m_autostartCheck, &QCheckBox::toggled, this, [this, autostartPath](bool on){
        if (on) {
            QDir().mkpath(QFileInfo(autostartPath).path());
            QFile f(autostartPath);
            if (f.open(QIODevice::WriteOnly|QIODevice::Text)) {
                QTextStream ts(&f);
                ts << "[Desktop Entry]\nType=Application\nName=Omen RGB\nExec=" << QCoreApplication::applicationFilePath() << " --tray\n";
                ts << "Icon=omen-rgb\nComment=HP OMEN RGB Keyboard Control\nX-GNOME-Autostart-enabled=true\n";
                f.close();
                QMessageBox::information(this,"Autostart","Autostart enabled");
            }
        } else {
            QFile::remove(autostartPath);
            QMessageBox::information(this,"Autostart","Autostart disabled");
        }
    });
    main->addWidget(miscBox);

    // Log
    auto *logBox = new QGroupBox("Log", this);
    auto *logLay = new QVBoxLayout(logBox);
    m_log = new QTextEdit(logBox);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(140);
    m_log->setPlaceholderText("Operations log...");
    logLay->addWidget(m_log);
    main->addWidget(logBox);

    // Footer
    auto *footer = new QLabel("Driver: omen_rgb_keyboard v1.5 | WMI GUID 5FB7F034-2C63-45e9-BE91-3D44E2C707E4 | Buffer 128B | State: /var/lib/omen-rgb-keyboard/state | GPL-3.0", this);
    footer->setStyleSheet("color:#888; font-size: 8pt;");
    footer->setWordWrap(true);
    main->addWidget(footer);
    main->addStretch();

    refresh();
}

void SettingsTab::refresh() {
    checkDriver();
    refreshMuteStatus();
    checkPermissions();
}

void SettingsTab::checkDriver() {
    bool loaded = OmenDevice::isDriverLoaded();
    QString ver = OmenDevice::driverVersion();
    m_driverStatus->setText(loaded ? QString("<b style='color:green'>● Driver loaded</b> (%1) — /sys/devices/platform/omen-rgb-keyboard exists").arg(ver) : "<b style='color:red'>● Driver NOT loaded</b> — /sys/.../omen-rgb-keyboard missing. Install via button or sudo modprobe omen_rgb_keyboard");
    QString dkms = m_installer->dkmsStatus();
    if (dkms.isEmpty()) dkms = "(dkms status empty — maybe not installed via DKMS)";
    m_dkmsStatus->setText(QString("DKMS: <code>%1</code>").arg(dkms.toHtmlEscaped()));
    bool hp = m_installer->isHpWmiLoaded();
    m_hpWmiStatus->setText(hp ? "<b style='color:orange'>● hp_wmi is loaded — CONFLICT! Blacklist recommended: sudo modprobe -r hp_wmi && echo blacklist hp_wmi | sudo tee /etc/modprobe.d/blacklist-hp.conf</b>" : "<b style='color:green'>● hp_wmi not loaded (good)</b>");
}

void SettingsTab::checkPermissions() {
    QString rgbBase = OmenDevice::rgbBase();
    QString testFile = rgbBase + "brightness";
    bool writable = SysFs::isWritable(testFile);
    bool exists = SysFs::exists(testFile);
    QString grp;
    QProcess p; p.start("bash", {"-c", "groups 2>/dev/null || id -nG 2>/dev/null"});
    p.waitForFinished(1000);
    grp = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    bool inInput = grp.split(QRegularExpression("\\s+")).contains("input");
    if (!exists) {
        m_permStatus->setText("<b style='color:red'>Permissions:</b> sysfs not found — driver not loaded");
    } else if (writable) {
        m_permStatus->setText(QString("<b style='color:green'>Permissions: OK (writable)</b> — groups: %1").arg(grp.toHtmlEscaped()));
    } else {
        QString hint = inInput ? " (in input but need relogin? try <code>newgrp input</code>)" : " (not in input)";
        m_permStatus->setText(QString("<b style='color:orange'>Permissions: Need pkexec or udev</b> — not writable, will use <code>pkexec tee</code> fallback%1. Install udev rules for password-less control. groups: %2").arg(hint).arg(grp.toHtmlEscaped()));
    }
}

void SettingsTab::installDriver() {
    auto reply = QMessageBox::question(this, "Install Driver",
        "This will run privileged DKMS build:\n"
        "- modprobe -r hp_wmi (if loaded)\n"
        "- make install (DKMS)\n"
        "- copy configs, create /var/lib/omen-rgb-keyboard, install mute monitor, udev rules, modprobe\n"
        "\nDriver source is expected in current dir or found via search. Continue?\n"
        "You will be prompted for password via pkexec.",
        QMessageBox::Yes|QMessageBox::No);
    if (reply!=QMessageBox::Yes) return;
    m_log->append("=== Install Driver started ===");
    // Try to find source dir: look for dkms.conf near app or cwd
    QString srcDir;
    // Check current working dir first
    if (QFileInfo::exists(QDir::currentPath()+"/dkms.conf")) srcDir = QDir::currentPath();
    else if (QFileInfo::exists(QCoreApplication::applicationDirPath()+"/../dkms.conf")) srcDir = QFileInfo(QCoreApplication::applicationDirPath()+"/../").absoluteFilePath();
    else {
        // search upwards
        QProcess find; find.start("bash", {"-c", "find /home -name dkms.conf -path '*omen*' 2>/dev/null | head -1 | xargs -r dirname; echo \"---\"; pwd"});
        find.waitForFinished(2000);
        QString out = QString::fromUtf8(find.readAllStandardOutput());
        // simple
        QDir cwd = QDir::current();
        if (QFileInfo::exists(cwd.filePath("dkms.conf"))) srcDir = cwd.path();
    }
    // Unload hp_wmi first (pkexec)
    QString err;
    if (m_installer->isHpWmiLoaded()) {
        m_log->append("Unloading hp_wmi...");
        if (!m_installer->unloadHpWmi(&err)) m_log->append("unload hp_wmi warning: " + err);
        else m_log->append("hp_wmi unloaded");
        m_installer->blacklistHpWmi(&err);
    }
    if (!m_installer->isDkmsInstalled()) {
        m_log->append("DKMS not found, installing...");
        if (!m_installer->installDkmsPackage(&err)) {
            QMessageBox::critical(this,"DKMS Install Failed", err);
            m_log->append("DKMS install failed: " + err);
            return;
        }
    }
    // find actual source with fallback to /home/droidmystic/Desktop/omen-rgb-keyboard if cloned elsewhere?
    // For dev, driver source may not be bundled; try to clone if missing
    if (srcDir.isEmpty() || !QFileInfo::exists(srcDir+"/Makefile")) {
        auto cloneReply = QMessageBox::question(this, "Source not found",
            "Driver source (Makefile + dkms.conf) not found nearby.\n"
            "Clone https://github.com/OmenLinux/omen-rgb-keyboard to /tmp/omen-rgb-keyboard and build from there?",
            QMessageBox::Yes|QMessageBox::No);
        if (cloneReply==QMessageBox::Yes) {
            QProcess clone; clone.start("bash", {"-c", "rm -rf /tmp/omen-rgb-keyboard && git clone https://github.com/OmenLinux/omen-rgb-keyboard /tmp/omen-rgb-keyboard 2>&1"});
            clone.waitForFinished(15000);
            QString cloneOut = QString::fromUtf8(clone.readAllStandardOutput() + clone.readAllStandardError());
            m_log->append(cloneOut);
            if (QFileInfo::exists("/tmp/omen-rgb-keyboard/dkms.conf")) srcDir = "/tmp/omen-rgb-keyboard";
        }
    }
    if (srcDir.isEmpty()) srcDir = QDir::currentPath();
    m_log->append("Using source dir: " + srcDir);
    if (!m_installer->installDriver(srcDir, &err)) {
        QMessageBox::critical(this,"Driver Install Failed", err + "\n\nSee log. Common issues: Secure Boot (Key rejected), missing linux-headers, WMI conflict.");
        m_log->append("Install failed: " + err);
        checkDriver();
        return;
    }
    QMessageBox::information(this,"Success","Driver installed and loaded. If permissions needed, install udev rules and relogin.");
    m_log->append("=== Install complete ===");
    checkDriver();
    checkPermissions();
}

void SettingsTab::installUdev() {
    QString err;
    // Try running bundled install-udev-rules.sh via pkexec if exists, else manual chgrp
    QString srcDir = QDir::currentPath();
    if (QFileInfo::exists(srcDir+"/install-udev-rules.sh")) {
        QProcess p; p.start("pkexec", {"bash", srcDir+"/install-udev-rules.sh"});
        p.waitForFinished(10000);
        QString out = QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError());
        m_log->append(out);
        if (p.exitCode()!=0) QMessageBox::critical(this,"Udev Failed", out);
        else QMessageBox::information(this,"Udev","Udev rules installed. You may need to logout/login or run `newgrp input`.");
    } else {
        // manual
        QString cmd = "cp 99-omen-rgb-keyboard.rules /etc/udev/rules.d/ 2>/dev/null || echo 'no rules file'; udevadm control --reload-rules; udevadm trigger --subsystem-match=platform --attr-match=kernel=omen-rgb-keyboard || true";
        if (!m_installer->reloadUdev(&err)) QMessageBox::critical(this,"Udev Failed", err);
        else QMessageBox::information(this,"Udev","Udev reloaded (manual)");
    }
    checkPermissions();
}

void SettingsTab::reloadUdev() {
    QString err;
    if (!m_installer->reloadUdev(&err)) QMessageBox::critical(this,"Failed",err);
    else { m_log->append("udev reloaded"); QMessageBox::information(this,"OK","udev reloaded"); }
    checkPermissions();
}

void SettingsTab::addToInputGroup() {
    QString err;
    if (!m_installer->addUserToInputGroup(&err)) QMessageBox::critical(this,"Failed",err);
    else {
        QMessageBox::information(this,"Added","User added to 'input' group. Logout/login or `newgrp input` required.");
        m_log->append("Added to input group");
    }
    checkPermissions();
}

void SettingsTab::toggleMuteLed() {
    QString err;
    m_muteLedState = !m_muteLedState;
    if (!m_mute->setMuteLed(m_muteLedState, &err)) {
        QMessageBox::critical(this,"Mute LED Failed", err.isEmpty()? "Write failed (HDA codec not found?)" : err);
        m_muteLedState = !m_muteLedState;
        return;
    }
    QMessageBox::information(this,"Applied", QString("Mute LED %1 (manual — auto-sync disabled until reload)").arg(m_muteLedState?"ON":"OFF"));
    refreshMuteStatus();
}

void SettingsTab::sendMuteState() { /* handled via buttons */ }

void SettingsTab::refreshMuteStatus() {
    bool availOk; bool avail = m_mute->isAvailable(&availOk);
    QString svc = "unknown";
    bool active = m_mute->isServiceActive();
    bool enabled = m_mute->isServiceEnabled();
    QString extra;
    if (!availOk) extra = " (sysfs not found — driver not loaded)";
    else if (!avail) extra = " (HDA codec not found — mute_led unavailable)";
    m_muteStatus->setText(QString("Mute LED available: %1%2 | Service: %3 (%4) | wpctl: %5")
        .arg(avail?"yes":"no").arg(extra)
        .arg(active?"active":"inactive").arg(enabled?"enabled":"disabled")
        .arg(QFileInfo::exists("/usr/bin/wpctl") || QFileInfo::exists("/bin/wpctl") ? "found" : "not found — install wireplumber"));
    // also show mute_state writable?
    QString muteStatePath = OmenDevice::rgbPath("mute_state");
    bool writable = SysFs::isWritable(muteStatePath);
    m_muteStatus->setText(m_muteStatus->text() + QString(" | mute_state writable: %1").arg(writable?"yes (pkexec fallback otherwise)":"no"));
}

void SettingsTab::showDmesg() {
    QProcess p;
    p.start("bash", {"-c", "dmesg | grep -i omen | tail -50; echo '---'; dmesg | tail -20"});
    p.waitForFinished(2000);
    QString out = QString::fromUtf8(p.readAllStandardOutput());
    if (out.trimmed().isEmpty()) out = "No omen messages in dmesg. Try: sudo dmesg | grep -i omen";
    QMessageBox::information(this,"dmesg", out);
    m_log->append("=== dmesg ===\n" + out);
}

void SettingsTab::showOmenKeyInfo() {
    QMessageBox::information(this,"Omen Key Mapping",
        "<b>Omen Key → KEY_MSDOS (XF86DOS)</b><br><br>"
        "<b>GNOME:</b> Settings → Keyboard → Shortcuts → + → Press Omen key<br>"
        "<b>KDE:</b> System Settings → Shortcuts → Custom → Global → Command/URL → Trigger Omen<br>"
        "<b>i3/Sway:</b> <code>bindsym XF86DOS exec your-command</code><br>"
        "<b>Hyprland:</b> <code>bind = , XF86DOS, exec, your-command</code><br><br>"
        "To remap, edit <code>src/wmi/omen_wmi.c</code>:<br>"
        "<code>{ KE_KEY, 0x21a5, { KEY_MSDOS } }</code> → your keycode, then <code>sudo make install</code>.<br><br>"
        "Test: <code>evtest</code> or <code>wev</code> and press Omen key.");
}
