#include <QApplication>
#include <QCommandLineParser>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Omen RGB");
    QApplication::setApplicationVersion("1.5");
    QApplication::setOrganizationName("OmenRGB");
    QApplication::setOrganizationDomain("omenrgb.local");
    QApplication::setDesktopFileName("omen-rgb");

    QCommandLineParser parser;
    parser.setApplicationDescription("HP OMEN RGB Keyboard & Fan Control — Qt6 Widgets for omen-rgb-keyboard driver");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption trayOpt({"t","tray"}, "Start minimized to tray");
    parser.addOption(trayOpt);
    parser.process(app);

    // Single instance check could be added via QLockFile, omitted for now

    MainWindow w;
    bool startTray = parser.isSet(trayOpt);
    if (startTray && QSystemTrayIcon::isSystemTrayAvailable()) {
        // Start minimized to tray: don't show window, tray icon will be visible
    } else {
        w.show();
    }

    return app.exec();
}
