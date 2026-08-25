# Omen RGB GUI

[![Arch Linux](https://img.shields.io/badge/Arch-Linux-1793D1?logo=arch-linux&logoColor=white)](https://archlinux.org)
[![Qt6](https://img.shields.io/badge/Qt-6.11-41CD52?logo=qt&logoColor=white)](https://www.qt.io)
[![CMake](https://img.shields.io/badge/CMake-3.16+-064F8C?logo=cmake&logoColor=white)](https://cmake.org)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)
[![Driver](https://img.shields.io/badge/Driver-omen--rgb--keyboard-ff6600)](https://github.com/OmenLinux/omen-rgb-keyboard)

**Qt6 Widgets desktop GUI for [OmenLinux/omen-rgb-keyboard](https://github.com/OmenLinux/omen-rgb-keyboard)** — controls HP OMEN 4-zone RGB keyboard, 11 animations, brightness, fan curves, and mute LED on Arch Linux.

Built with **Qt6 Widgets** + **CMake** + **C++17** — bundled DKMS build, `pkexec tee` fallback for sysfs, local `cmake --install`, system tray + autostart.

> Tested on Arch (qt6-base 6.11.2, linux-headers, dkms, alsa-lib) — no `qt6-charts` dependency (custom QPainter fan curve).

---

## ✨ Screenshots

> _Add screenshots/screencast here — placeholders for now_

| Colors | Animations | Fan |
|--------|------------|-----|
| 4-zone pickers + brightness + presets | 11 modes + gradient editor | RPM + thermal profile + draggable curve |
| ![colors](docs/screenshot-colors.png) | ![anim](docs/screenshot-animations.png) | ![fan](docs/screenshot-fan.png) |

System tray: right-click → Quick Colors/Animations, Show/Hide (double-click), Refresh, Quit. Close to tray via `QSettings`.

---

## 📋 Features (1:1 with driver docs)

**Colors**
- 4 individual zones `zone00-03` (hex `RRGGBB`, `#` stripped on read, bare hex on write → stops animation → `static`)
- All-zones `all`, brightness `0-100%` (`brightness` + `omen::kbd_backlight` LED classdev) — slider + spinbox
- Presets: Gaming Red, Subtle White, Rainbow Zones, Ocean, Neon Purple, Off

**Animations — 11 modes, 20 FPS timer**
- `static, breathing, rainbow, wave, pulse, chase, sparkle, candle, aurora, disco, gradient`
- Speed `1-10` (`animation_speed`, driver default `1`, UI default `5`)
- Gradient editor: `gradient_config` format `zones:colors;...` e.g. `0,1,2:FF0000,00FF00,0000FF;3:800080,FFA500` (max 4 groups, 16 colors, 512 chars) — validated, serialized `;`/`:`/`,`

**Fan (`/fan/`, needs `hp_wmi` blacklisted + `CONFIG_THERMAL`)**
- RPM `cpu_fan_rpm`/`gpu_fan_rpm` (read-only `n/a` handling, 1.5s poll)
- `max_fan` 0/1 (85s keepalive, disables curve)
- `thermal_profile` `silent`/`normal`/`performance` (EC `0x95`/`0x59`, installs 5-point preset curves)
- `fan_curve` `temp:pct` 2-8 points `0-120:0-100` sorted, linear interp, BIOS table mapped — custom `FanCurveWidget` (QPainter draggable, add via double-click, remove via right-click)
- `fan_curve_enable` 0/1 (requires Victus `0x2D` + BIOS table `0x2F` + thermal zone, `EBUSY` if `max_fan`)
- `fan_temp_zone` combobox from `/sys/class/thermal/thermal_zone*/type` (`x86_pkg_temp` etc) + `(auto)`

**Mute LED**
- `mute_led` manual `0/1` (disables auto-sync until reload)
- `mute_state` `0/1` for PipeWire via `omen-mute-monitor` (`wpctl get-volume @DEFAULT_AUDIO_SINK@` poll 100ms)
- Service `systemctl --user {is-active,is-enabled} omen-mute-monitor.service`, HDA 200ms Realtek `0x10ec` verbs `0x500`/`0x400`

**System**
- Driver detection (`/sys/devices/platform/omen-rgb-keyboard` + `lsmod` + `modinfo -F version`)
- `hp_wmi` conflict warning, udev `99-omen-rgb-keyboard.rules` (`GROUP input MODE 0664` + `chgrp`/`chmod g+w`)
- Permissions badge (`groups`/`input`, `QFileInfo::isWritable` → `pkexec tee` fallback)
- Omen key `KEY_MSDOS`/`XF86DOS` (GNOME/KDE/i3 `bindsym XF86DOS` snippets, remap via `src/wmi/omen_wmi.c`)
- State `/var/lib/omen-rgb-keyboard/state` respected, `QSettings` for window/tray

Driver sysfs:
```
RGB: /sys/devices/platform/omen-rgb-keyboard/rgb_zones/{zone00-03,all,brightness,animation_mode,animation_speed,gradient_config,mute_led,mute_state}
FAN: /sys/devices/platform/omen-rgb-keyboard/fan/{cpu_fan_rpm,gpu_fan_rpm,max_fan,thermal_profile,fan_curve,fan_curve_enable,fan_temp_zone}
LED: /sys/class/leds/omen::kbd_backlight/{brightness,max_brightness}
```

---

## 📦 Requirements (Arch)

```bash
sudo pacman -S qt6-base linux-headers base-devel dkms alsa-lib git
# optional: wireplumber (wpctl), pipewire, pipewire-pulse
```

Qt6 6.11.2+, CMake 3.16+, GCC 16.2.1. No `qt6-charts` needed.

---

## 🚀 Build & Install (local cmake)

```bash
# driver source for bundled DKMS (optional if you already have driver)
git clone https://github.com/OmenLinux/omen-rgb-keyboard /tmp/omen-rgb-keyboard

git clone https://github.com/nsfwsabir/Omen-RGB-GUI.git
cd Omen-RGB-GUI
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# test without install (mock or real hardware)
OMEN_RGB_ROOT=/tmp/mock_omen QT_QPA_PLATFORM=offscreen ./build/omen-rgb  # mock
./build/omen-rgb  # needs driver loaded

# install locally
sudo cmake --install build
# → /usr/local/bin/omen-rgb
# → /usr/share/applications/omen-rgb.desktop
# → /usr/share/icons/hicolor/scalable/apps/omen-rgb.svg
# → /usr/share/polkit-1/actions/io.omenrgb.policy

# run
omen-rgb
omen-rgb --tray  # minimized to tray
```

Uninstall:
```bash
sudo rm /usr/local/bin/omen-rgb /usr/share/applications/omen-rgb.desktop \
        /usr/share/icons/hicolor/scalable/apps/omen-rgb.svg \
        /usr/share/polkit-1/actions/io.omenrgb.policy
```

---

## 🔧 Driver Install (bundled DKMS via GUI)

**Settings → Install Driver (DKMS)** — runs `pkexec bash -c ...` (polkit dialog):

1. `dkms` check (`pacman -S dkms` if missing)
2. `modprobe -r hp_wmi` + `blacklist hp_wmi > /etc/modprobe.d/blacklist-hp.conf`
3. `make install` (auto-finds `dkms.conf`/`Makefile` nearby, or clones to `/tmp/omen-rgb-keyboard`)
4. `cp omen_rgb_keyboard.conf /etc/modprobe.d/`, `echo omen_rgb_keyboard > /etc/modules-load.d/omen_rgb_keyboard.conf`, `mkdir -p /var/lib/omen-rgb-keyboard`
5. `scripts/omen-mute-monitor.sh → /usr/local/bin/`, systemd user service `/etc/systemd/user/omen-mute-monitor.service`, tmpfiles `0666`, `loginctl enable-linger`
6. Udev `99-omen-rgb-keyboard.rules` + `udevadm trigger` + `usermod -aG input $SUDO_USER`
7. `modprobe omen_rgb_keyboard` (checks `dmesg`; Secure Boot “Key was rejected by service” → sign MOK via `mokutil` + DKMS signing — see Arch Wiki DKMS Secure Boot)

Manual:
```bash
git clone https://github.com/OmenLinux/omen-rgb-keyboard
cd omen-rgb-keyboard
sudo make install
sudo cp omen_rgb_keyboard.conf /etc/modprobe.d/
echo "omen_rgb_keyboard" | sudo tee /etc/modules-load.d/omen_rgb_keyboard.conf
sudo mkdir -p /var/lib/omen-rgb-keyboard
sudo modprobe omen_rgb_keyboard
```

**Non-root:** Settings → Install Udev Rules + Add to `input` → logout/login or `newgrp input`. App auto-falls back to `pkexec tee <path>` if not writable (immediate use, auth per write; udev preferred for password-less).

---

## 🎮 Usage

- **Colors:** Pick per zone → Apply (hex, preview). All Zones sets 4 at once. Brightness 0-100 → Apply. Presets immediate.
- **Animations:** Combo exact strings, speed 1-10. Quick buttons: Breathing Red, Rainbow, Aurora, Disco, Candle. Gradient Editor builds `;`/`:`/`,` string, enforces 4×16×512.
- **Fan:** RPM auto-poll. Max Fan disables curve. Thermal profile writes WMI `0x1A` and installs 5-point preset. Curve: drag, double-click add, right-click remove (2-8). Apply String parses `temp:percent`, sorts. Enable toggle checks `max_fan`, `fan_tbl_valid`, `CONFIG_THERMAL`.
- **Settings:** Refresh Status, Show `dmesg | grep -i omen`, permissions, mute controls, Omen key help (`XF86DOS`), autostart toggle (`~/.config/autostart/omen-rgb.desktop --tray`), log view.

**Tray:** Right-click → Quick Colors/Animations, Refresh, Show Window (double-click toggle). Close to tray via `QSettings OmenRGB/omen-rgb closeToTray` (default true) — hide not quit; quit via tray menu.

---

## 🔐 Permissions & pkexec

`SysFs::write` tries `QFile::WriteOnly` first; if fails (not in `input`, no udev), runs `pkexec tee <path>` with stdin payload (polkit dialog). Reads via `QFile`. Test:

```bash
mkdir -p /tmp/mock_omen/{rgb_zones,fan}
echo "FF0000" > /tmp/mock_omen/rgb_zones/zone00; chmod 666 /tmp/mock_omen/rgb_zones/zone00
OMEN_RGB_ROOT=/tmp/mock_omen QT_QPA_PLATFORM=offscreen ./build/omen-rgb
# For pkexec path: chmod 444 /tmp/mock_omen/rgb_zones/zone00 → will prompt
```

---

## 🔄 Autostart

Checkbox creates/removes `~/.config/autostart/omen-rgb.desktop`:
```ini
[Desktop Entry]
Type=Application
Name=Omen RGB
Exec=/usr/local/bin/omen-rgb --tray
Icon=omen-rgb
X-GNOME-Autostart-enabled=true
```
Enable linger: `loginctl enable-linger $USER`.

---

## 🗂️ Project Structure

```
CMakeLists.txt
resources.qrc
resources/omen-rgb.desktop
resources/io.omenrgb.policy
resources/icons/omen-rgb.svg
src/main.cpp
src/core/{OmenDevice,SysFs,RgbController,AnimationController,FanController,MuteLedController,DkmsInstaller}.{h,cpp}
src/ui/{MainWindow,ColorTab,AnimationTab,FanTab,SettingsTab,GradientEditorDialog,FanCurveWidget}.{h,cpp}
```

---

## ✅ Verification

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
timeout 2 QT_QPA_PLATFORM=offscreen ./build/omen-rgb; echo $?  # 124 = alive 2s (good)
timeout 2 OMEN_RGB_ROOT=/tmp/mock_omen QT_QPA_PLATFORM=offscreen ./build/omen-rgb; echo $?  # 124
```

Mock sysfs at `/tmp/mock_omen` for CI without hardware: zones, brightness, 11 modes, gradient, fan RPM/curve. Controllers validated via `echo` + `QFile` round-trip (writable → direct write).

---

## ⚠️ Notes

- No secrets stored. `pkexec` only for sysfs + driver install (explicit consent); udev preferred.
- Secure Boot: sign module via `mokutil` + DKMS signing (see Gist sbueringer + Arch Wiki).
- `hp_wmi` must be blacklisted; app warns if loaded.
- `fan_curve` needs `CONFIG_THERMAL` + Victus table; UI disables if unavailable.
- Driver saves to `/var/lib/omen-rgb-keyboard/state` on each write; app respects it + own `QSettings`.

---

## 📄 License

GPL-3.0 (driver). App GPL-3.0 compatible. See [LICENSE](LICENSE).

## 🤝 Contributing

PRs welcome — please test with mock `OMEN_RGB_ROOT` and real hardware if possible, and run `cmake --build build`.

## 🔗 Links

- Driver: https://github.com/OmenLinux/omen-rgb-keyboard
- Discord: https://discord.gg/8UwyAJ7sBH
- Arch Wiki DKMS Secure Boot: https://wiki.archlinux.org/title/Dynamic_Kernel_Module_Support#Secure_Boot
