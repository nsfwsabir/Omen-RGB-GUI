#pragma once
#include <QWidget>

class RgbController;
class QSlider;
class QSpinBox;
class QPushButton;
class QLineEdit;
class QComboBox;
class QLabel;

class ColorTab : public QWidget {
    Q_OBJECT
public:
    explicit ColorTab(QWidget *parent = nullptr);
    void refresh();

private slots:
    void pickZoneColor(int zone);
    void applyZoneHex(int zone);
    void applyAllZones();
    void applyBrightness();
    void applyPreset();

private:
    RgbController *m_rgb;
    // per zone
    QPushButton *m_zoneBtn[4];
    QLineEdit *m_zoneEdit[4];
    QLabel *m_zonePreview[4];
    // all
    QPushButton *m_allBtn;
    QLineEdit *m_allEdit;
    // brightness
    QSlider *m_brightnessSlider;
    QSpinBox *m_brightnessSpin;
    QLabel *m_brightnessLabel;
    // presets
    QComboBox *m_presetCombo;
    QPushButton *m_presetApply;

    void updateZoneButtonColor(int zone, const QString &hex);
    QString currentZoneHex(int zone) const;
};
