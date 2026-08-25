#pragma once
#include <QWidget>

class AnimationController;
class QComboBox;
class QSlider;
class QSpinBox;
class QLabel;
class QPushButton;
class QLineEdit;

class AnimationTab : public QWidget {
    Q_OBJECT
public:
    explicit AnimationTab(QWidget *parent = nullptr);
    void refresh();

private slots:
    void applyMode();
    void applySpeed();
    void editGradient();
    void applyGradientQuick(const QString &cfg);

private:
    AnimationController *m_anim;
    QComboBox *m_modeCombo;
    QLabel *m_modeDesc;
    QSlider *m_speedSlider;
    QSpinBox *m_speedSpin;
    QLabel *m_speedLabel;
    QLineEdit *m_gradientEdit;
    QPushButton *m_gradientBtn;
    QPushButton *m_gradientApply;
    QPushButton *m_gradientEditorBtn;

    QString modeDescription(const QString &mode) const;
};
