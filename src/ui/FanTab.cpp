#include "FanTab.h"
#include "core/FanController.h"
#include "core/OmenDevice.h"
#include "core/SysFs.h"
#include "ui/FanCurveWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QApplication>

FanTab::FanTab(QWidget *parent) : QWidget(parent), m_fan(new FanController(this)) {
    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(12,12,12,12);
    main->setSpacing(10);

    // Unsupported banner (shown when driver has no fan support)
    m_unsupportedBanner = new QLabel(this);
    m_unsupportedBanner->setWordWrap(true);
    m_unsupportedBanner->setTextFormat(Qt::RichText);
    m_unsupportedBanner->setStyleSheet("background:#3d2a00; color:#ffb84d; padding:8px; border:1px solid #805500; border-radius:6px;");
    m_unsupportedBanner->setVisible(false);
    main->addWidget(m_unsupportedBanner);

    // RPM row
    auto *rpmBox = new QGroupBox("Fan RPM (read-only, polls 1s)", this);
    auto *rpmLay = new QHBoxLayout(rpmBox);
    m_cpuRpm = new QLabel("CPU: n/a", rpmBox);
    m_gpuRpm = new QLabel("GPU: n/a", rpmBox);
    m_statusLabel = new QLabel("—", rpmBox);
    m_statusLabel->setStyleSheet("color:#888;");
    auto *refreshBtn = new QPushButton("Refresh", rpmBox);
    rpmLay->addWidget(m_cpuRpm);
    rpmLay->addWidget(m_gpuRpm);
    rpmLay->addStretch();
    rpmLay->addWidget(m_statusLabel);
    rpmLay->addWidget(refreshBtn);
    main->addWidget(rpmBox);
    connect(refreshBtn, &QPushButton::clicked, this, &FanTab::pollRpm);

    // Max fan + thermal profile row
    m_ctrlBox = new QGroupBox("Fan Control", this);
    auto *cLay = new QGridLayout(m_ctrlBox);
    m_maxFan = new QCheckBox("Max Fan (disables curve, keepalive 85s)", m_ctrlBox);
    cLay->addWidget(m_maxFan, 0,0,1,2);
    cLay->addWidget(new QLabel("Thermal Profile:", m_ctrlBox), 1,0);
    m_thermalProfile = new QComboBox(m_ctrlBox);
    m_thermalProfile->addItems(FanController::availableProfiles());
    m_thermalApply = new QPushButton("Apply", m_ctrlBox);
    auto *thRow = new QWidget(m_ctrlBox);
    auto *thLay = new QHBoxLayout(thRow); thLay->setContentsMargins(0,0,0,0);
    thLay->addWidget(m_thermalProfile,1); thLay->addWidget(m_thermalApply);
    cLay->addWidget(thRow, 1,1);
    cLay->addWidget(new QLabel("Temp Zone:", m_ctrlBox), 2,0);
    m_tempZone = new QComboBox(m_ctrlBox);
    m_tempZone->setEditable(true);
    m_tempZone->addItem("(auto)");
    for (auto &z : m_fan->availableTempZones()) m_tempZone->addItem(z);
    // add common even if not present
    QStringList commons = {"x86_pkg_temp","acpitz","k10temp"};
    for (auto &c : commons) if (m_tempZone->findText(c)==-1) m_tempZone->addItem(c);
    m_tempZoneApply = new QPushButton("Apply", m_ctrlBox);
    auto *tzRow = new QWidget(m_ctrlBox);
    auto *tzLay = new QHBoxLayout(tzRow); tzLay->setContentsMargins(0,0,0,0);
    tzLay->addWidget(m_tempZone,1); tzLay->addWidget(m_tempZoneApply);
    cLay->addWidget(tzRow, 2,1);
    main->addWidget(m_ctrlBox);
    connect(m_maxFan, &QCheckBox::toggled, this, &FanTab::applyMaxFan);
    connect(m_thermalApply, &QPushButton::clicked, this, &FanTab::applyThermalProfile);
    connect(m_tempZoneApply, &QPushButton::clicked, this, &FanTab::applyTempZone);

    // Curve
    m_curveBox = new QGroupBox("Fan Curve (2-8 points, temp 0-120°C : fan 0-100%, linear interp)", this);
    auto *curveMain = new QVBoxLayout(m_curveBox);
    auto *presetRow = new QHBoxLayout();
    presetRow->addWidget(new QLabel("Presets:", m_curveBox));
    m_presetCombo = new QComboBox(m_curveBox);
    m_presetCombo->addItem("Custom", "custom");
    m_presetCombo->addItem("Silent (35:12 50:20 65:34 80:52 95:72)", "silent");
    m_presetCombo->addItem("Normal (35:22 52:36 68:55 82:78 98:95)", "normal");
    m_presetCombo->addItem("Performance (30:35 48:50 62:68 76:88 92:100)", "performance");
    m_presetCombo->addItem("Example 7pts (30:20 40:28 50:38 60:50 72:65 82:78 92:90)", "example7");
    auto *presetApply = new QPushButton("Load Preset", m_curveBox);
    presetRow->addWidget(m_presetCombo,1); presetRow->addWidget(presetApply);
    curveMain->addLayout(presetRow);
    connect(presetApply, &QPushButton::clicked, this, &FanTab::presetSelected);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::activated), this, &FanTab::presetSelected);

    m_curveWidget = new FanCurveWidget(m_curveBox);
    m_curveWidget->setMinimumHeight(260);
    curveMain->addWidget(m_curveWidget);
    connect(m_curveWidget, &FanCurveWidget::pointsChanged, this, &FanTab::onCurveWidgetChanged);

    auto *info = new QLabel("Left-click drag points, Right-click remove, Double-click add (max 8, min 2). Values mapped via BIOS fan table.", m_curveBox);
    info->setWordWrap(true); info->setStyleSheet("color:#888; font-size: 9pt;");
    curveMain->addWidget(info);

    // Curve string edit + enable
    auto *curveCtrlRow = new QHBoxLayout();
    m_curveEdit = new QLineEdit(m_curveBox);
    m_curveEdit->setPlaceholderText("30:20 40:28 50:38 60:50 ...");
    m_curveApply = new QPushButton("Apply String", m_curveBox);
    m_curveReset = new QPushButton("Read Back", m_curveBox);
    curveCtrlRow->addWidget(new QLabel("String:", m_curveBox));
    curveCtrlRow->addWidget(m_curveEdit,1);
    curveCtrlRow->addWidget(m_curveApply);
    curveCtrlRow->addWidget(m_curveReset);
    curveMain->addLayout(curveCtrlRow);
    connect(m_curveApply, &QPushButton::clicked, this, &FanTab::applyFanCurve);
    connect(m_curveReset, &QPushButton::clicked, this, &FanTab::refresh);
    connect(m_curveEdit, &QLineEdit::returnPressed, this, &FanTab::applyFanCurve);

    auto *enableRow = new QHBoxLayout();
    m_curveEnable = new QCheckBox("Enable Fan Curve (requires Victus + BIOS table + CONFIG_THERMAL)", m_curveBox);
    auto *enableBtn = new QPushButton("Toggle Enable", m_curveBox);
    m_curveStringLabel = new QLabel("(unset)", m_curveBox);
    m_curveStringLabel->setStyleSheet("color:#888; font-family: monospace;");
    enableRow->addWidget(m_curveEnable);
    enableRow->addWidget(enableBtn);
    enableRow->addStretch();
    enableRow->addWidget(m_curveStringLabel);
    curveMain->addLayout(enableRow);
    connect(enableBtn, &QPushButton::clicked, this, &FanTab::toggleCurveEnable);
    connect(m_curveEnable, &QCheckBox::toggled, this, [this](bool on){
        m_curveWidget->setReadOnly(on ? false : false); // actually readOnly tied to maxFan, not enable
    });

    main->addWidget(m_curveBox, 1);

    // Poll timer
    m_pollTimer.setInterval(1500);
    connect(&m_pollTimer, &QTimer::timeout, this, &FanTab::pollRpm);
    m_pollTimer.start();

    // Initial state
    refresh();
    pollRpm();
}

void FanTab::updateFanAvailability() {
    bool supported = FanController::isFanSupported();
    if (!supported) {
        QString msg = FanController::notSupportedMessage();
        m_unsupportedBanner->setText(QString("<b>⚠ Fan control unavailable</b><br>%1<br><br><i>RGB controls remain fully functional. Fan control only works on Victus/OMEN models with BIOS fan table and driver v1.5+ fan support.</i>").arg(msg.toHtmlEscaped()));
        m_unsupportedBanner->setVisible(true);
        m_ctrlBox->setEnabled(false);
        m_curveBox->setEnabled(false);
        m_statusLabel->setText("Fan not supported — RGB only");
        m_statusLabel->setStyleSheet("color:#ffaa00;");
        m_pollTimer.stop();
        return;
    }
    // Supported but check if RPM available (driver loaded but WMI/thermal not ready)
    bool avail = m_fan->isFanAvailable();
    if (!avail) {
        // Keep banner but allow controls? Show warning but keep enabled for retry
        // Only show banner if we haven't already shown detailed message
        if (!m_unsupportedBanner->isVisible()) {
            m_unsupportedBanner->setText(QString("<b>Fan interface not ready</b><br>%1").arg(FanController::notSupportedMessage().toHtmlEscaped()));
            m_unsupportedBanner->setVisible(true);
        }
        // Keep polling but not disable completely - user may blacklist hp_wmi and reload
        m_ctrlBox->setEnabled(true);
        m_curveBox->setEnabled(true);
        if (!m_pollTimer.isActive()) m_pollTimer.start();
    } else {
        m_unsupportedBanner->setVisible(false);
        m_ctrlBox->setEnabled(true);
        m_curveBox->setEnabled(true);
        if (!m_pollTimer.isActive()) m_pollTimer.start();
    }
}

void FanTab::refresh() {
    updateFanAvailability();
    if (!FanController::isFanSupported()) {
        // When not supported, still show n/a for curve
        m_curveStringLabel->setText("n/a (unsupported)");
        m_cpuRpm->setText("CPU: n/a");
        m_gpuRpm->setText("GPU: n/a");
        return;
    }
    bool ok;
    QString prof = m_fan->thermalProfile(&ok);
    if (ok) {
        int idx = m_thermalProfile->findText(prof);
        if (idx>=0) { m_thermalProfile->blockSignals(true); m_thermalProfile->setCurrentIndex(idx); m_thermalProfile->blockSignals(false); }
    }
    QString tz = m_fan->tempZone(&ok);
    if (ok) {
        int idx = m_tempZone->findText(tz);
        if (idx>=0) m_tempZone->setCurrentIndex(idx);
        else { m_tempZone->setEditText(tz); }
    }
    bool maxOk; bool max = m_fan->maxFan(&maxOk);
    if (maxOk) {
        m_maxFan->blockSignals(true); m_maxFan->setChecked(max); m_maxFan->blockSignals(false);
        m_curveWidget->setReadOnly(max);
        if (max) m_statusLabel->setText("Max fan active — curve disabled");
        else m_statusLabel->setText("");
    }
    bool enOk; bool en = m_fan->curveEnabled(&enOk);
    if (enOk) { m_curveEnable->blockSignals(true); m_curveEnable->setChecked(en); m_curveEnable->blockSignals(false); }

    QString cs = m_fan->fanCurveString(&ok);
    if (ok) {
        m_curveStringLabel->setText(cs);
        m_curveEdit->setText(cs=="(unset)" ? "" : cs);
        bool pOk; auto pts = FanController::stringToPoints(cs, &pOk);
        if (pOk && pts.size()>=2) m_curveWidget->setPoints(pts);
    } else {
        m_curveStringLabel->setText("n/a");
    }
}

void FanTab::pollRpm() {
    if (!FanController::isFanSupported()) {
        m_cpuRpm->setText("CPU: n/a");
        m_gpuRpm->setText("GPU: n/a");
        // banner already shown by updateFanAvailability
        return;
    }
    bool cpuOk, gpuOk;
    int cpu = m_fan->cpuRpm(&cpuOk);
    int gpu = m_fan->gpuRpm(&gpuOk);
    m_cpuRpm->setText(cpuOk ? QString("CPU: %1 RPM").arg(cpu) : "CPU: n/a");
    m_gpuRpm->setText(gpuOk ? QString("GPU: %1 RPM").arg(gpu) : "GPU: n/a");
    if (!cpuOk && !gpuOk) {
        if (m_statusLabel->text().isEmpty()) m_statusLabel->setText("Fan interface not available — check driver/hp_wmi");
        // also ensure availability banner updated
        updateFanAvailability();
    } else {
        // if RPM now available, hide banner
        if (FanController::isFanSupported() && m_fan->isFanAvailable()) {
            m_unsupportedBanner->setVisible(false);
        }
    }
}

void FanTab::applyMaxFan() {
    if (!FanController::isFanSupported()) {
        QMessageBox::information(this, "Fan Not Supported", FanController::notSupportedMessage());
        // revert checkbox
        bool cur = false; bool ok; cur = m_fan->maxFan(&ok);
        m_maxFan->blockSignals(true); m_maxFan->setChecked(ok ? cur : false); m_maxFan->blockSignals(false);
        updateFanAvailability();
        return;
    }
    bool en = m_maxFan->isChecked();
    QString err;
    if (!m_fan->setMaxFan(en, &err)) {
        // If not supported, show info not critical
        if (!FanController::isFanSupported()) QMessageBox::information(this, "Fan Not Supported", err);
        else QMessageBox::critical(this,"Max Fan Failed", err);
        m_maxFan->blockSignals(true); m_maxFan->setChecked(!en); m_maxFan->blockSignals(false);
        return;
    }
    m_curveWidget->setReadOnly(en);
    if (en) m_statusLabel->setText("Max fan enabled — curve disabled (keepalive 85s)");
    else m_statusLabel->setText("");
    // also update curve enable state
    refresh();
}

void FanTab::applyThermalProfile() {
    if (!FanController::isFanSupported()) {
        QMessageBox::information(this, "Fan Not Supported", FanController::notSupportedMessage());
        return;
    }
    QString prof = m_thermalProfile->currentText();
    QString err;
    if (!m_fan->setThermalProfile(prof, &err)) {
        if (!FanController::isFanSupported()) QMessageBox::information(this, "Fan Not Supported", err);
        else QMessageBox::critical(this,"Thermal Profile Failed", err.isEmpty()? SysFs::lastError : err);
        return;
    }
    // After success, driver auto-installs preset curve and starts worker if enabled
    QMessageBox::information(this,"Applied", QString("Thermal profile set to %1. Preset curve installed.").arg(prof));
    refresh();
}

void FanTab::applyTempZone() {
    if (!FanController::isFanSupported()) {
        QMessageBox::information(this, "Fan Not Supported", FanController::notSupportedMessage());
        return;
    }
    QString zone = m_tempZone->currentText().trimmed();
    if (zone=="(auto)") zone="(auto)"; // our setTempZone handles "(auto)" as no-op
    QString err;
    if (zone=="(auto)") {
        QMessageBox::information(this,"Temp Zone","Auto mode — driver will probe x86_pkg_temp, acpitz, etc. No write performed.");
        return;
    }
    if (!m_fan->setTempZone(zone, &err)) {
        if (!FanController::isFanSupported()) QMessageBox::information(this, "Fan Not Supported", err);
        else QMessageBox::critical(this,"Temp Zone Failed", err);
        return;
    }
    QMessageBox::information(this,"Applied", QString("Temp zone set to %1").arg(zone));
}

void FanTab::onCurveWidgetChanged() {
    auto pts = m_curveWidget->points();
    m_curveEdit->setText(FanController::pointsToString(pts));
    m_curveStringLabel->setText(FanController::pointsToString(pts));
}

void FanTab::applyFanCurve() {
    if (!FanController::isFanSupported()) {
        QMessageBox::information(this, "Fan Not Supported", FanController::notSupportedMessage());
        return;
    }
    QString str = m_curveEdit->text().trimmed();
    if (str.isEmpty()) {
        QMessageBox::warning(this,"Empty","Enter fan curve like '30:20 40:28 50:38 60:50 72:65 82:78 92:90'");
        return;
    }
    QString err;
    // Validate via parsing
    bool pOk; auto pts = FanController::stringToPoints(str, &pOk);
    if (!pOk) { QMessageBox::warning(this,"Invalid","Invalid format, expected temp:percent spaced"); return; }
    if (!FanController::validatePoints(pts, &err)) { QMessageBox::warning(this,"Invalid", err); return; }
    if (!m_fan->setFanCurve(pts, &err)) {
        if (!FanController::isFanSupported()) QMessageBox::information(this, "Fan Not Supported", err);
        else QMessageBox::critical(this,"Fan Curve Failed", err.isEmpty()? SysFs::lastError : err);
        return;
    }
    m_curveWidget->setPoints(pts);
    m_curveStringLabel->setText(str);
    QMessageBox::information(this,"Applied", "Fan curve written. Enable it via checkbox if not already.");
}

void FanTab::presetSelected() {
    if (!FanController::isFanSupported()) {
        QMessageBox::information(this, "Fan Not Supported", FanController::notSupportedMessage());
        return;
    }
    QString key = m_presetCombo->currentData().toString();
    QList<FanPoint> pts;
    if (key=="silent") pts = FanController::presetSilent();
    else if (key=="normal") pts = FanController::presetNormal();
    else if (key=="performance") pts = FanController::presetPerformance();
    else if (key=="example7") pts = {{30,20},{40,28},{50,38},{60,50},{72,65},{82,78},{92,90}};
    else return;
    m_curveWidget->setPoints(pts);
    m_curveEdit->setText(FanController::pointsToString(pts));
    // auto apply?
    QString err;
    if (!m_fan->setFanCurve(pts, &err)) {
        if (!FanController::isFanSupported()) QMessageBox::information(this, "Fan Not Supported", err);
        else QMessageBox::critical(this,"Preset Failed", err);
        return;
    }
    m_curveStringLabel->setText(FanController::pointsToString(pts));
}

void FanTab::toggleCurveEnable() {
    if (!FanController::isFanSupported()) {
        QMessageBox::information(this, "Fan Not Supported", FanController::notSupportedMessage());
        return;
    }
    bool want = m_curveEnable->isChecked();
    // toggling: we want opposite, since checkbox reflects current? Actually button toggles
    want = !want;
    // Use button logic: we toggle to opposite of current checked state
    // Simpler: enable = !currentEnabled, but our button says toggle
    bool curOk; bool cur = m_fan->curveEnabled(&curOk);
    bool target = !cur;
    QString err;
    if (!m_fan->setCurveEnable(target, &err)) {
        if (!FanController::isFanSupported()) QMessageBox::information(this, "Fan Not Supported", err);
        else QMessageBox::critical(this,"Curve Enable Failed", err.isEmpty()? SysFs::lastError : err + "\nRequires Victus + BIOS table + valid curve + max_fan off + CONFIG_THERMAL");
        return;
    }
    m_curveEnable->setChecked(target);
    QMessageBox::information(this,"Toggled", QString("Fan curve %1").arg(target?"enabled (poll 1.5s)":"disabled"));
}
