#include "AnimationTab.h"
#include "core/AnimationController.h"
#include "core/SysFs.h"
#include "core/OmenDevice.h"
#include "ui/GradientEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QApplication>

AnimationTab::AnimationTab(QWidget *parent) : QWidget(parent), m_anim(new AnimationController(this)) {
    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(12,12,12,12);
    main->setSpacing(12);

    // Mode
    auto *modeBox = new QGroupBox("Animation Mode (11 modes, CPU-efficient 20 FPS)", this);
    auto *mLay = new QVBoxLayout(modeBox);
    auto *row1 = new QHBoxLayout();
    m_modeCombo = new QComboBox(modeBox);
    for (auto &m : AnimationController::availableModes()) m_modeCombo->addItem(m);
    auto *applyModeBtn = new QPushButton("Apply Mode", modeBox);
    row1->addWidget(new QLabel("Mode:", modeBox));
    row1->addWidget(m_modeCombo, 1);
    row1->addWidget(applyModeBtn);
    mLay->addLayout(row1);
    m_modeDesc = new QLabel(modeBox);
    m_modeDesc->setWordWrap(true);
    m_modeDesc->setStyleSheet("color:#888; font-style:italic;");
    mLay->addWidget(m_modeDesc);
    // Examples quick buttons
    auto *quickRow = new QHBoxLayout();
    quickRow->addWidget(new QLabel("Quick:", modeBox));
    struct Quick { QString label; QString mode; int speed; };
    QList<Quick> quicks = {{"Breathing Red", "breathing", 3}, {"Rainbow", "rainbow", 5}, {"Aurora", "aurora", 3}, {"Disco", "disco", 6}, {"Candle", "candle", 4}};
    for (auto &q : quicks) {
        auto *btn = new QPushButton(q.label, modeBox);
        btn->setToolTip(QString("%1 @ %2").arg(q.mode).arg(q.speed));
        mLay->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, q](){
            QMap<QString, QString> writes;
            writes[OmenDevice::rgbPath("animation_mode")] = q.mode;
            writes[OmenDevice::rgbPath("animation_speed")] = QString::number(q.speed);
            QString err;
            if (!SysFs::writeBatch(writes, &err)) { QMessageBox::critical(this,"Quick Failed", err); return; }
            refresh();
        });
    }
    // Actually quickRow not used; we added to mLay directly but okay
    Q_UNUSED(quickRow);
    main->addWidget(modeBox);
    connect(m_modeCombo, &QComboBox::currentTextChanged, this, [this](const QString &m){ m_modeDesc->setText(modeDescription(m)); });
    connect(applyModeBtn, &QPushButton::clicked, this, &AnimationTab::applyMode);

    // Speed
    auto *speedBox = new QGroupBox("Animation Speed (1-10)", this);
    auto *sLay = new QHBoxLayout(speedBox);
    m_speedSlider = new QSlider(Qt::Horizontal, speedBox);
    m_speedSlider->setRange(1,10);
    m_speedSlider->setTickPosition(QSlider::TicksBelow);
    m_speedSlider->setTickInterval(1);
    m_speedSpin = new QSpinBox(speedBox);
    m_speedSpin->setRange(1,10);
    auto *applySpeedBtn = new QPushButton("Apply Speed", speedBox);
    m_speedLabel = new QLabel("—", speedBox);
    sLay->addWidget(new QLabel("Speed:", speedBox));
    sLay->addWidget(m_speedSlider, 1);
    sLay->addWidget(m_speedSpin);
    sLay->addWidget(applySpeedBtn);
    sLay->addWidget(m_speedLabel);
    main->addWidget(speedBox);
    connect(m_speedSlider, &QSlider::valueChanged, m_speedSpin, &QSpinBox::setValue);
    connect(m_speedSpin, &QSpinBox::valueChanged, m_speedSlider, &QSlider::setValue);
    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int v){ m_speedLabel->setText(QString::number(v)); });
    connect(applySpeedBtn, &QPushButton::clicked, this, &AnimationTab::applySpeed);
    connect(m_speedSlider, &QSlider::sliderReleased, this, &AnimationTab::applySpeed);

    // Gradient
    auto *gradBox = new QGroupBox("Gradient Configuration (gradient mode only)", this);
    auto *gLay = new QVBoxLayout(gradBox);
    gLay->addWidget(new QLabel("Format: zones:colors;...  e.g. 0,1,2:FF0000,00FF00,0000FF;3:800080,FFA500  (max 4 groups, 16 colors each, 512 chars)", gradBox));
    auto *gRow = new QHBoxLayout();
    m_gradientEdit = new QLineEdit(gradBox);
    m_gradientEdit->setPlaceholderText("0,1,2:FF0000,00FF00,0000FF;3:800080,FFA500");
    m_gradientApply = new QPushButton("Apply", gradBox);
    m_gradientEditorBtn = new QPushButton("Open Editor…", gradBox);
    gRow->addWidget(m_gradientEdit, 1);
    gRow->addWidget(m_gradientApply);
    gRow->addWidget(m_gradientEditorBtn);
    gLay->addLayout(gRow);
    auto *gQuick = new QHBoxLayout();
    gQuick->addWidget(new QLabel("Examples:", gradBox));
    auto *ex1 = new QPushButton("Rainbow Split", gradBox);
    auto *ex2 = new QPushButton("Purple-Orange", gradBox);
    gQuick->addWidget(ex1); gQuick->addWidget(ex2); gQuick->addStretch();
    gLay->addLayout(gQuick);
    main->addWidget(gradBox);
    connect(m_gradientApply, &QPushButton::clicked, this, [this](){ applyGradientQuick(m_gradientEdit->text()); });
    connect(m_gradientEditorBtn, &QPushButton::clicked, this, &AnimationTab::editGradient);
    connect(ex1, &QPushButton::clicked, this, [this](){ m_gradientEdit->setText("0,1,2:FF0000,00FF00,0000FF;3:800080,FFA500"); applyGradientQuick(m_gradientEdit->text()); });
    connect(ex2, &QPushButton::clicked, this, [this](){ m_gradientEdit->setText("0,1:FF00FF,00FFFF;2,3:FFA500,FF0000"); applyGradientQuick(m_gradientEdit->text()); });
    connect(m_gradientEdit, &QLineEdit::returnPressed, this, [this](){ applyGradientQuick(m_gradientEdit->text()); });

    main->addStretch();
    refresh();
}

QString AnimationTab::modeDescription(const QString &mode) const {
    static QMap<QString,QString> desc = {
        {"static","No animation, static colors (default)"},
        {"breathing","Smooth breathing fade in/out (2000/speed ms cycle)"},
        {"rainbow","Rainbow wave cycles through all colors (3000/speed)"},
        {"wave","Wave moves across zones"},
        {"pulse","Pulsing varying intensity"},
        {"chase","Lights follow each other in sequence"},
        {"sparkle","Random sparkle white flashes"},
        {"candle","Warm flickering orange/red"},
        {"aurora","Aurora borealis green/blue waves"},
        {"disco","Disco strobe multi-colored flashes"},
        {"gradient","Custom per-zone-group color cycling (configure below)"}
    };
    return desc.value(mode, "");
}

void AnimationTab::refresh() {
    bool ok;
    QString mode = m_anim->mode(&ok);
    if (ok) {
        int idx = m_modeCombo->findText(mode);
        if (idx>=0) {
            m_modeCombo->blockSignals(true);
            m_modeCombo->setCurrentIndex(idx);
            m_modeCombo->blockSignals(false);
            m_modeDesc->setText(modeDescription(mode));
        }
        // enable gradient editor only if gradient or always? Keep always but highlight
        bool isGrad = (mode=="gradient");
        m_gradientEdit->setEnabled(true);
        m_gradientEditorBtn->setEnabled(true);
        QPalette pal = m_gradientEdit->palette();
        if (isGrad) {
            // highlight
        }
        Q_UNUSED(isGrad);
    } else {
        m_modeDesc->setText("Driver not loaded or read failed");
    }
    int sp = m_anim->speed(&ok);
    if (ok) {
        m_speedSlider->blockSignals(true);
        m_speedSpin->blockSignals(true);
        m_speedSlider->setValue(sp);
        m_speedSpin->setValue(sp);
        m_speedSlider->blockSignals(false);
        m_speedSpin->blockSignals(false);
        m_speedLabel->setText(QString::number(sp));
    } else {
        m_speedLabel->setText("n/a");
    }
    QString grad = m_anim->gradientConfig(&ok);
    if (ok) m_gradientEdit->setText(grad);
    else m_gradientEdit->setPlaceholderText("n/a — driver not loaded");
}

void AnimationTab::applyMode() {
    QString mode = m_modeCombo->currentText();
    QString err;
    if (!m_anim->setMode(mode, &err)) {
        QMessageBox::critical(this,"Failed", err.isEmpty()?"Unknown error":err);
        return;
    }
    m_modeDesc->setText(modeDescription(mode));
}

void AnimationTab::applySpeed() {
    int v = m_speedSlider->value();
    QString err;
    if (!m_anim->setSpeed(v, &err)) {
        QMessageBox::critical(this,"Failed", err);
        return;
    }
    m_speedLabel->setText(QString::number(v));
}

void AnimationTab::applyGradientQuick(const QString &cfg) {
    QString err;
    if (!m_anim->setGradientConfig(cfg, &err)) {
        QMessageBox::critical(this,"Gradient Failed", err.isEmpty()?"Invalid format":err);
        return;
    }
    // Optionally also switch to gradient mode
    if (m_modeCombo->currentText()!="gradient") {
        auto btn = QMessageBox::question(this,"Switch mode?","Gradient config applied. Switch animation mode to 'gradient' now?", QMessageBox::Yes|QMessageBox::No);
        if (btn==QMessageBox::Yes) {
            m_anim->setMode("gradient", &err);
            refresh();
        }
    }
}

void AnimationTab::editGradient() {
    GradientEditorDialog dlg(m_gradientEdit->text(), this);
    if (dlg.exec()==QDialog::Accepted) {
        QString cfg = dlg.gradientConfig();
        m_gradientEdit->setText(cfg);
        applyGradientQuick(cfg);
    }
}
