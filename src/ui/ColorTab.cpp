#include "ColorTab.h"
#include "core/RgbController.h"
#include "core/SysFs.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QColorDialog>
#include <QMessageBox>
#include <QRegularExpressionValidator>

ColorTab::ColorTab(QWidget *parent) : QWidget(parent), m_rgb(new RgbController(this)) {
    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(12,12,12,12);
    main->setSpacing(12);

    // Zones group
    auto *zoneBox = new QGroupBox("Keyboard Zones (4-zone individual control)", this);
    auto *zoneGrid = new QGridLayout(zoneBox);
    zoneGrid->addWidget(new QLabel("Zone", zoneBox), 0,0);
    zoneGrid->addWidget(new QLabel("Color", zoneBox), 0,1);
    zoneGrid->addWidget(new QLabel("Hex (RRGGBB)", zoneBox), 0,2);
    zoneGrid->addWidget(new QLabel("Action", zoneBox), 0,3);

    QRegularExpression re("^[0-9a-fA-F]{6}$");
    auto *hexValidator = new QRegularExpressionValidator(re, this);

    for (int i=0;i<4;++i) {
        QString lbl = QString("Zone %1").arg(i);
        zoneGrid->addWidget(new QLabel(lbl, zoneBox), i+1, 0);
        m_zoneBtn[i] = new QPushButton("Pick…", zoneBox);
        m_zoneBtn[i]->setMinimumWidth(90);
        m_zonePreview[i] = new QLabel(zoneBox);
        m_zonePreview[i]->setFixedSize(36,24);
        m_zonePreview[i]->setStyleSheet("border:1px solid #888; border-radius:4px;");
        m_zonePreview[i]->setAutoFillBackground(true);
        auto *btnRow = new QWidget(zoneBox);
        auto *bLay = new QHBoxLayout(btnRow); bLay->setContentsMargins(0,0,0,0);
        bLay->addWidget(m_zonePreview[i]); bLay->addWidget(m_zoneBtn[i]);
        zoneGrid->addWidget(btnRow, i+1, 1);

        m_zoneEdit[i] = new QLineEdit(zoneBox);
        m_zoneEdit[i]->setPlaceholderText("FF0000");
        m_zoneEdit[i]->setMaxLength(6);
        m_zoneEdit[i]->setValidator(hexValidator);
        m_zoneEdit[i]->setMinimumWidth(100);
        zoneGrid->addWidget(m_zoneEdit[i], i+1, 2);

        auto *applyBtn = new QPushButton("Apply", zoneBox);
        applyBtn->setProperty("zone", i);
        zoneGrid->addWidget(applyBtn, i+1, 3);
        connect(m_zoneBtn[i], &QPushButton::clicked, this, [this,i]{ pickZoneColor(i); });
        connect(applyBtn, &QPushButton::clicked, this, [this,i]{ applyZoneHex(i); });
        connect(m_zoneEdit[i], &QLineEdit::returnPressed, this, [this,i]{ applyZoneHex(i); });
        // live preview on edit
        connect(m_zoneEdit[i], &QLineEdit::textChanged, this, [this,i](const QString &t){
            if (t.size()==6 && SysFs::isValidHexColor(t)) {
                QColor c("#"+t);
                m_zonePreview[i]->setStyleSheet(QString("background:%1; border:1px solid #888; border-radius:4px;").arg(c.name()));
            }
        });
    }
    main->addWidget(zoneBox);

    // All zones
    auto *allBox = new QGroupBox("All Zones (set all 4 at once)", this);
    auto *allLay = new QHBoxLayout(allBox);
    m_allEdit = new QLineEdit(allBox);
    m_allEdit->setPlaceholderText("FFFFFF");
    m_allEdit->setMaxLength(6);
    m_allEdit->setValidator(hexValidator);
    m_allBtn = new QPushButton("Apply to All Zones", allBox);
    auto *pickAllBtn = new QPushButton("Pick…", allBox);
    allLay->addWidget(new QLabel("Hex:", allBox));
    allLay->addWidget(m_allEdit);
    allLay->addWidget(pickAllBtn);
    allLay->addWidget(m_allBtn);
    allLay->addStretch();
    main->addWidget(allBox);
    connect(m_allBtn, &QPushButton::clicked, this, &ColorTab::applyAllZones);
    connect(pickAllBtn, &QPushButton::clicked, this, [this](){
        QColor c = QColorDialog::getColor(Qt::white, this, "Pick All Zones Color");
        if (!c.isValid()) return;
        QString hex = RgbController::colorToHex(c);
        m_allEdit->setText(hex);
        applyAllZones();
    });
    connect(m_allEdit, &QLineEdit::returnPressed, this, &ColorTab::applyAllZones);

    // Brightness
    auto *brightBox = new QGroupBox("Brightness (0-100%)", this);
    auto *bLay = new QHBoxLayout(brightBox);
    m_brightnessSlider = new QSlider(Qt::Horizontal, brightBox);
    m_brightnessSlider->setRange(0,100);
    m_brightnessSlider->setTickPosition(QSlider::TicksBelow);
    m_brightnessSlider->setTickInterval(10);
    m_brightnessSpin = new QSpinBox(brightBox);
    m_brightnessSpin->setRange(0,100);
    m_brightnessSpin->setSuffix("%");
    auto *brightApply = new QPushButton("Apply", brightBox);
    m_brightnessLabel = new QLabel("—", brightBox);
    bLay->addWidget(new QLabel("Level:", brightBox));
    bLay->addWidget(m_brightnessSlider, 1);
    bLay->addWidget(m_brightnessSpin);
    bLay->addWidget(brightApply);
    bLay->addWidget(m_brightnessLabel);
    main->addWidget(brightBox);
    connect(m_brightnessSlider, &QSlider::valueChanged, m_brightnessSpin, &QSpinBox::setValue);
    connect(m_brightnessSpin, &QSpinBox::valueChanged, m_brightnessSlider, &QSlider::setValue);
    connect(brightApply, &QPushButton::clicked, this, &ColorTab::applyBrightness);
    // auto-apply on slider release
    connect(m_brightnessSlider, &QSlider::sliderReleased, this, &ColorTab::applyBrightness);

    // Presets
    auto *presetBox = new QGroupBox("Presets", this);
    auto *pLay = new QHBoxLayout(presetBox);
    m_presetCombo = new QComboBox(presetBox);
    m_presetCombo->addItem("Gaming Red (FF0000 @ 75%)", "preset_gaming");
    m_presetCombo->addItem("Subtle White (FFFFFF @ 25%)", "preset_white");
    m_presetCombo->addItem("Rainbow Zones (R,O,Y,G)", "preset_rainbow");
    m_presetCombo->addItem("Ocean Blue (0080FF @ 80%)", "preset_ocean");
    m_presetCombo->addItem("Neon Purple (FF00FF @ 90%)", "preset_purple");
    m_presetCombo->addItem("Off (000000 @ 0%)", "preset_off");
    m_presetApply = new QPushButton("Apply Preset", presetBox);
    pLay->addWidget(m_presetCombo, 1);
    pLay->addWidget(m_presetApply);
    main->addWidget(presetBox);
    connect(m_presetApply, &QPushButton::clicked, this, &ColorTab::applyPreset);

    main->addStretch();

    refresh();
}

void ColorTab::refresh() {
    // Read zones
    for (int i=0;i<4;++i) {
        bool ok;
        QString hex = m_rgb->zoneHex(i, &ok);
        if (ok) {
            m_zoneEdit[i]->setText(hex);
            updateZoneButtonColor(i, hex);
        } else {
            m_zoneEdit[i]->setPlaceholderText("—");
            m_zonePreview[i]->setStyleSheet("background:#444; border:1px solid #888; border-radius:4px;");
        }
    }
    bool bOk;
    int b = m_rgb->brightness(&bOk);
    if (bOk) {
        m_brightnessSlider->blockSignals(true);
        m_brightnessSpin->blockSignals(true);
        m_brightnessSlider->setValue(b);
        m_brightnessSpin->setValue(b);
        m_brightnessSlider->blockSignals(false);
        m_brightnessSpin->blockSignals(false);
        m_brightnessLabel->setText(QString("%1%").arg(b));
    } else {
        m_brightnessLabel->setText("n/a");
    }
}

void ColorTab::updateZoneButtonColor(int zone, const QString &hex) {
    QColor c("#"+hex);
    m_zonePreview[zone]->setStyleSheet(QString("background:%1; border:1px solid #888; border-radius:4px;").arg(c.name()));
    m_zoneBtn[zone]->setStyleSheet(QString("background:%1; color:%2; border-radius:4px; padding:4px;").arg(c.name()).arg(c.lightness()>128?"black":"white"));
}

QString ColorTab::currentZoneHex(int zone) const {
    return m_zoneEdit[zone]->text().trimmed();
}

void ColorTab::pickZoneColor(int zone) {
    QString cur = currentZoneHex(zone);
    QColor init = cur.size()==6 ? QColor("#"+cur) : Qt::white;
    QColor c = QColorDialog::getColor(init, this, QString("Pick Zone %1 Color").arg(zone));
    if (!c.isValid()) return;
    QString hex = RgbController::colorToHex(c);
    m_zoneEdit[zone]->setText(hex);
    updateZoneButtonColor(zone, hex);
    // auto apply
    applyZoneHex(zone);
}

void ColorTab::applyZoneHex(int zone) {
    QString hex = currentZoneHex(zone);
    if (!SysFs::isValidHexColor(hex)) {
        QMessageBox::warning(this,"Invalid Hex","Enter 6 hex chars (e.g. FF0000)");
        return;
    }
    QString err;
    if (!m_rgb->setZoneHex(zone, hex, &err)) {
        QMessageBox::critical(this,"Write Failed", err.isEmpty()? SysFs::lastError : err);
        return;
    }
    updateZoneButtonColor(zone, SysFs::normalizeHex(hex));
}

void ColorTab::applyAllZones() {
    QString hex = m_allEdit->text().trimmed();
    if (!SysFs::isValidHexColor(hex)) {
        QMessageBox::warning(this,"Invalid Hex","Enter 6 hex chars");
        return;
    }
    QString err;
    if (!m_rgb->setAllHex(hex, &err)) {
        QMessageBox::critical(this,"Write Failed", err);
        return;
    }
    // update all previews
    hex = SysFs::normalizeHex(hex);
    for (int i=0;i<4;++i) {
        m_zoneEdit[i]->setText(hex);
        updateZoneButtonColor(i, hex);
    }
}

void ColorTab::applyBrightness() {
    int v = m_brightnessSlider->value();
    QString err;
    if (!m_rgb->setBrightness(v, &err)) {
        QMessageBox::critical(this,"Brightness Failed", err);
        return;
    }
    m_brightnessLabel->setText(QString("%1%").arg(v));
}

void ColorTab::applyPreset() {
    QString key = m_presetCombo->currentData().toString();
    QString err;
    if (key=="preset_gaming") {
        m_rgb->setAllHex("FF0000",&err);
        m_rgb->setBrightness(75,&err);
    } else if (key=="preset_white") {
        m_rgb->setAllHex("FFFFFF",&err);
        m_rgb->setBrightness(25,&err);
    } else if (key=="preset_rainbow") {
        m_rgb->setZoneHex(0,"FF0000",&err);
        m_rgb->setZoneHex(1,"FF8000",&err);
        m_rgb->setZoneHex(2,"FFFF00",&err);
        m_rgb->setZoneHex(3,"00FF00",&err);
        m_rgb->setBrightness(80,&err);
    } else if (key=="preset_ocean") {
        m_rgb->setAllHex("0080FF",&err);
        m_rgb->setBrightness(80,&err);
    } else if (key=="preset_purple") {
        m_rgb->setAllHex("FF00FF",&err);
        m_rgb->setBrightness(90,&err);
    } else if (key=="preset_off") {
        m_rgb->setAllHex("000000",&err);
        m_rgb->setBrightness(0,&err);
    }
    if (!err.isEmpty()) {
        // check via SysFs lastError
    }
    refresh();
}
