#pragma once
#include <QWidget>

class DkmsInstaller;
class MuteLedController;
class QLabel;
class QPushButton;
class QCheckBox;
class QTextEdit;
class QGroupBox;

class SettingsTab : public QWidget {
    Q_OBJECT
public:
    explicit SettingsTab(QWidget *parent = nullptr);
    void refresh();

private slots:
    void checkDriver();
    void installDriver();
    void installUdev();
    void reloadUdev();
    void addToInputGroup();
    void toggleMuteLed();
    void sendMuteState();
    void refreshMuteStatus();
    void showDmesg();
    void showOmenKeyInfo();
    void checkPermissions();

private:
    DkmsInstaller *m_installer;
    MuteLedController *m_mute;

    QLabel *m_driverStatus;
    QLabel *m_dkmsStatus;
    QLabel *m_hpWmiStatus;
    QLabel *m_permStatus;
    QLabel *m_muteStatus;
    QTextEdit *m_log;
    QPushButton *m_muteLedBtn;
    bool m_muteLedState = false;
    QCheckBox *m_autostartCheck;
};
