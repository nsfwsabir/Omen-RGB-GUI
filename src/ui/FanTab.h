#pragma once
#include <QWidget>
#include <QTimer>

class FanController;
class FanCurveWidget;
class QComboBox;
class QLabel;
class QCheckBox;
class QPushButton;
class QLineEdit;
class QSpinBox;
class QTableWidget;

class FanTab : public QWidget {
    Q_OBJECT
public:
    explicit FanTab(QWidget *parent = nullptr);
    void refresh();

private slots:
    void pollRpm();
    void applyMaxFan();
    void applyThermalProfile();
    void applyTempZone();
    void applyFanCurve();
    void onCurveWidgetChanged();
    void presetSelected();
    void toggleCurveEnable();

private:
    FanController *m_fan;
    FanCurveWidget *m_curveWidget;
    QLabel *m_cpuRpm;
    QLabel *m_gpuRpm;
    QLabel *m_statusLabel;
    QCheckBox *m_maxFan;
    QComboBox *m_thermalProfile;
    QPushButton *m_thermalApply;
    QComboBox *m_tempZone;
    QPushButton *m_tempZoneApply;
    QCheckBox *m_curveEnable;
    QPushButton *m_curveApply;
    QPushButton *m_curveReset;
    QComboBox *m_presetCombo;
    QLabel *m_curveStringLabel;
    QLineEdit *m_curveEdit;
    QTimer m_pollTimer;
};
