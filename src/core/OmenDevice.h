#pragma once
#include <QString>
#include <QStringList>

namespace OmenDevice {
    // Allow override via env OMEN_RGB_ROOT for testing (e.g. /tmp/mock)
    // Otherwise defaults to real sysfs.
    QString rgbBase();
    QString fanBase();
    QString platformBase();

    inline QString rgbPath(const QString &file) { return rgbBase() + file; }
    inline QString fanPath(const QString &file) { return fanBase() + file; }

    // File names
    namespace Files {
        const QString Zone00 = "zone00";
        const QString Zone01 = "zone01";
        const QString Zone02 = "zone02";
        const QString Zone03 = "zone03";
        const QString All = "all";
        const QString Brightness = "brightness";
        const QString AnimationMode = "animation_mode";
        const QString AnimationSpeed = "animation_speed";
        const QString GradientConfig = "gradient_config";
        const QString MuteLed = "mute_led";
        const QString MuteState = "mute_state";

        const QString CpuFanRpm = "cpu_fan_rpm";
        const QString GpuFanRpm = "gpu_fan_rpm";
        const QString MaxFan = "max_fan";
        const QString ThermalProfile = "thermal_profile";
        const QString FanCurve = "fan_curve";
        const QString FanCurveEnable = "fan_curve_enable";
        const QString FanTempZone = "fan_temp_zone";
    }

    bool isDriverLoaded();
    QString driverVersion(); // from dmesg or modinfo if needed
    QString thermalZonesBase();
}
