#include <QApplication>
#include <QIcon>
#include <QPalette>
#include <QColor>
#include "MainWindow.h"
#include "OnnxRuntimeHelper.h"
#include "Version.h"
#include <opencv2/core/utils/logger.hpp>

int main(int argc, char* argv[]) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);
    OnnxRuntimeHelper::ensureCudaDllPaths();

    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    app.setApplicationName("PixelEraser Pro");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("PixelEraser");
    app.setDesktopFileName("PixelEraser.Pro");
#ifdef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=2");
#endif
    app.setWindowIcon(QIcon(":/icons/app-icon.png"));
    app.setStyle("Fusion");

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(18, 18, 20));
    darkPalette.setColor(QPalette::WindowText, QColor(240, 242, 246));
    darkPalette.setColor(QPalette::Base, QColor(24, 24, 27));
    darkPalette.setColor(QPalette::AlternateBase, QColor(30, 31, 35));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(28, 29, 33));
    darkPalette.setColor(QPalette::ToolTipText, QColor(240, 242, 246));
    darkPalette.setColor(QPalette::Text, QColor(240, 242, 246));
    darkPalette.setColor(QPalette::Button, QColor(34, 35, 39));
    darkPalette.setColor(QPalette::ButtonText, QColor(240, 242, 246));
    darkPalette.setColor(QPalette::Highlight, QColor(90, 94, 102));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(darkPalette);

    // Clean Minimalist Global Stylesheet (Custom GlassWidgets handle all spatial surfaces)
    app.setStyleSheet(R"(
        * {
            font-family: "Segoe UI Variable Display", "Segoe UI", "SF Pro Display", sans-serif;
            font-size: 12px;
            outline: none;
        }
        QMainWindow {
            background-color: #141416;
            color: #eef0f4;
        }
        GlassModalDialog, GlassDialog {
            background-color: transparent;
            color: #eef0f4;
        }
        QLabel {
            background: transparent;
        }
        QMenuBar {
            background-color: #0c0f18;
            border-bottom: 1px solid rgba(255, 255, 255, 18);
            color: #d0daea;
            padding: 2px 6px;
        }
        QMenuBar::item {
            padding: 5px 10px;
            border-radius: 6px;
            background: transparent;
        }
        QMenuBar::item:selected {
            background-color: rgba(255, 255, 255, 22);
            color: #ffffff;
        }
        QMenu {
            background-color: #191c22;
            border: 1px solid rgba(255, 255, 255, 52);
            border-radius: 12px;
            padding: 6px;
            color: #eef3fc;
        }
        QMenu::item {
            padding: 7px 26px 7px 14px;
            border-radius: 7px;
        }
        QMenu::item:selected {
            background-color: rgba(255, 255, 255, 34);
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: rgba(255, 255, 255, 22);
            margin: 5px 10px;
        }
        QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
            background-color: rgba(24, 28, 36, 175);
            border: 1px solid rgba(255, 255, 255, 42);
            border-radius: 10px;
            padding: 5px 12px;
            color: #ffffff;
            font-weight: 600;
            min-height: 22px;
        }
        QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {
            border: 1px solid rgba(255, 255, 255, 130);
            background-color: rgba(34, 38, 48, 200);
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background-color: #1b1e25;
            border: 1px solid rgba(255, 255, 255, 48);
            border-radius: 10px;
            selection-background-color: rgba(255, 255, 255, 34);
            selection-color: #ffffff;
            padding: 4px;
            color: #eef3fc;
        }
        QGroupBox {
            color: #9baac2;
            font-weight: 600;
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 16px;
            margin-top: 14px;
            padding: 12px;
            background-color: rgba(20, 25, 38, 180);
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 6px;
        }
        QPushButton {
            background-color: rgba(255, 255, 255, 14);
            border: 1px solid rgba(255, 255, 255, 34);
            border-radius: 12px;
            padding: 7px 16px;
            color: #f0f5ff;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: rgba(255, 255, 255, 24);
            border-color: rgba(255, 255, 255, 62);
        }
        QPushButton#PrimaryAccentBtn {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0E70FF, stop:1 #3A76FF);
            border: 1px solid rgba(255, 255, 255, 110);
            border-radius: 16px;
            color: #ffffff;
            font-weight: 700;
        }
        QProgressBar {
            background-color: rgba(255, 255, 255, 22);
            border: none;
            border-radius: 4px;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #6a707c, stop:1 #aeb6c4);
            border-radius: 4px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 4px 1px;
        }
        QScrollBar::handle:vertical {
            background: rgba(255, 255, 255, 42);
            border-radius: 3px;
            min-height: 28px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QToolTip {
            background-color: #1c1e22;
            border: 1px solid rgba(255, 255, 255, 52);
            color: #eef0f4;
            padding: 6px 11px;
            border-radius: 9px;
        }
    )");

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
