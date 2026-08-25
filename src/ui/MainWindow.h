#pragma once
#include <QMainWindow>
#include <QSystemTrayIcon>

class QTabWidget;
class ColorTab;
class AnimationTab;
class FanTab;
class SettingsTab;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleVisibility();
    void showAbout();

private:
    void createTray();
    void loadWindowState();
    void saveWindowState();

    QTabWidget *m_tabs;
    ColorTab *m_colorTab;
    AnimationTab *m_animTab;
    FanTab *m_fanTab;
    SettingsTab *m_settingsTab;
    QLabel *m_statusLabel;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
};
