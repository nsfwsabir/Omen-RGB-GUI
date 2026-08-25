#include "MainWindow.h"
#include "ui/ColorTab.h"
#include "ui/AnimationTab.h"
#include "ui/FanTab.h"
#include "ui/SettingsTab.h"
#include "core/OmenDevice.h"
#include "core/RgbController.h"
#include "core/AnimationController.h"
#include "core/SysFs.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>
#include <QTimer>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Omen RGB — HP OMEN Keyboard & Fan Control");
    setWindowIcon(QIcon::fromTheme("input-keyboard", QIcon(":/icons/omen-rgb.svg")));
    resize(860, 680);

    // Menu
    auto *fileMenu = menuBar()->addMenu("&File");
    auto *refreshAct = fileMenu->addAction("Refresh All", this, [this](){
        m_colorTab->refresh(); m_animTab->refresh(); m_fanTab->refresh(); m_settingsTab->refresh();
        statusBar()->showMessage("Refreshed", 2000);
    });
    refreshAct->setShortcut(QKeySequence::Refresh);
    fileMenu->addSeparator();
    auto *quitAct = fileMenu->addAction("Quit", this, &QWidget::close);
    quitAct->setShortcut(QKeySequence::Quit);

    auto *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About Omen RGB", this, &MainWindow::showAbout);
    helpMenu->addAction("About Qt", qApp, &QApplication::aboutQt);

    // Central
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *lay = new QVBoxLayout(central);
    lay->setContentsMargins(0,0,0,0);
    m_tabs = new QTabWidget(central);
    m_tabs->setTabPosition(QTabWidget::North);
    m_colorTab = new ColorTab(this);
    m_animTab = new AnimationTab(this);
    m_fanTab = new FanTab(this);
    m_settingsTab = new SettingsTab(this);
    m_tabs->addTab(m_colorTab, QIcon::fromTheme("color-management"), "Colors");
    m_tabs->addTab(m_animTab, QIcon::fromTheme("media-playlist-shuffle"), "Animations");
    m_tabs->addTab(m_fanTab, QIcon::fromTheme("fan"), "Fan");
    m_tabs->addTab(m_settingsTab, QIcon::fromTheme("settings-configure"), "Settings");
    lay->addWidget(m_tabs);

    // Status bar
    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel, 1);
    bool loaded = OmenDevice::isDriverLoaded();
    m_statusLabel->setText(loaded ? "● Driver loaded" : "● Driver NOT loaded — see Settings");
    m_statusLabel->setStyleSheet(loaded ? "color: green;" : "color: red;");
    statusBar()->showMessage(loaded ? "Ready" : "Driver missing — install via Settings", 5000);

    createTray();
    loadWindowState();

    // Auto-refresh every 3s if driver loaded? Let tabs handle their own polling.

    // Tray hint
    if (m_tray && m_tray->isVisible()) {
        QTimer::singleShot(800, this, [this](){
            m_tray->showMessage("Omen RGB", "Running in system tray. Right-click tray icon for quick controls.", QSystemTrayIcon::Information, 3000);
        });
    }
}

MainWindow::~MainWindow() { saveWindowState(); }

void MainWindow::createTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray = new QSystemTrayIcon(this);
    QIcon trayIcon = QIcon::fromTheme("input-keyboard");
    if (trayIcon.isNull()) trayIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
    m_tray->setIcon(trayIcon);
    m_tray->setToolTip("Omen RGB — HP OMEN Control");

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction("Show Window", this, &MainWindow::toggleVisibility);
    m_trayMenu->addSeparator();
    // Quick presets
    auto *presets = m_trayMenu->addMenu("Quick Colors");
    struct QP { QString name; QString hex; int bri; };
    QList<QP> qps = {{"Red Gaming", "FF0000",75},{"White Subtle","FFFFFF",25},{"Ocean","0080FF",80},{"Off","000000",0}};
    for (auto &qp : qps) {
        auto *act = presets->addAction(qp.name);
        connect(act, &QAction::triggered, this, [this, qp](){
            QMap<QString, QString> writes;
            writes[OmenDevice::rgbPath("all")] = qp.hex;
            writes[OmenDevice::rgbPath("brightness")] = QString::number(qp.bri);
            QString err;
            if (!SysFs::writeBatch(writes, &err)) {
                if (m_tray) m_tray->showMessage("Omen RGB", QString("Failed %1: %2").arg(qp.name).arg(err), QSystemTrayIcon::Critical, 3000);
            } else {
                if (m_tray) m_tray->showMessage("Omen RGB", QString("Applied %1").arg(qp.name), QSystemTrayIcon::Information, 1500);
            }
            m_colorTab->refresh();
        });
    }
    Q_UNUSED(presets);
    auto *animMenu = m_trayMenu->addMenu("Quick Animations");
    for (auto &mode : QStringList{"static","breathing","rainbow","aurora","disco"}) {
        auto *act = animMenu->addAction(mode);
        connect(act, &QAction::triggered, this, [this, mode](){
            AnimationController ac;
            QString err;
            ac.setMode(mode, &err);
            m_animTab->refresh();
            if (m_tray) m_tray->showMessage("Omen RGB", "Animation: " + mode, QSystemTrayIcon::Information, 1500);
        });
    }
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Refresh All", this, [this](){
        m_colorTab->refresh(); m_animTab->refresh(); m_fanTab->refresh(); m_settingsTab->refresh();
    });
    m_trayMenu->addAction("About", this, &MainWindow::showAbout);
    m_trayMenu->addAction("Quit", qApp, &QCoreApplication::quit);

    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    m_tray->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason==QSystemTrayIcon::Trigger || reason==QSystemTrayIcon::DoubleClick) toggleVisibility();
}

void MainWindow::toggleVisibility() {
    if (isVisible()) hide();
    else { show(); raise(); activateWindow(); }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_tray && m_tray->isVisible()) {
        // Ask or just hide to tray?
        // If user checked "close to tray" behavior, hide instead. For now, respect settings: hide to tray if tray available
        QSettings s("OmenRGB","omen-rgb");
        bool closeToTray = s.value("closeToTray", true).toBool();
        if (closeToTray) {
            event->ignore();
            hide();
            m_tray->showMessage("Omen RGB", "App minimized to tray. Right-click tray to quit.", QSystemTrayIcon::Information, 2000);
            return;
        }
    }
    saveWindowState();
    event->accept();
}

void MainWindow::loadWindowState() {
    QSettings s("OmenRGB","omen-rgb");
    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("windowState").toByteArray());
    int tab = s.value("currentTab",0).toInt();
    if (tab>=0 && tab < m_tabs->count()) m_tabs->setCurrentIndex(tab);
}

void MainWindow::saveWindowState() {
    QSettings s("OmenRGB","omen-rgb");
    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", saveState());
    s.setValue("currentTab", m_tabs->currentIndex());
}

void MainWindow::showAbout() {
    QMessageBox::about(this, "About Omen RGB",
        "<h3>Omen RGB 1.5</h3>"
        "<p>Qt6 Widgets control for <b>HP OMEN RGB Keyboard</b> Linux driver<br>"
        "by <a href='https://github.com/OmenLinux/omen-rgb-keyboard'>OmenLinux/omen-rgb-keyboard</a></p>"
        "<p>Controls 4-zone RGB, brightness, 11 animations (20 FPS), gradient, fan RPM/max/thermal curve, mute LED.</p>"
        "<p><b>Arch Linux:</b> requires <code>linux-headers base-devel dkms alsa-lib</code><br>"
        "Driver sysfs: <code>/sys/devices/platform/omen-rgb-keyboard/{rgb_zones,fan}</code><br>"
        "State: <code>/var/lib/omen-rgb-keyboard/state</code></p>"
        "<p>WMI GUID 5FB7F034-2C63... | Omen key → KEY_MSDOS (XF86DOS)<br>"
        "License: GPL-3.0</p>"
        "<p>Built with Qt " QT_VERSION_STR " — <a href='https://github.com/OmenLinux/omen-rgb-keyboard'>GitHub</a> | <a href='https://discord.gg/8UwyAJ7sBH'>Discord</a></p>"
    );
}
