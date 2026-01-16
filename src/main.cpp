#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    app.setApplicationName("PixelEraser Pro");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("PixelEraser");

    app.setStyle("Fusion");
    
    // Professional dark theme - DaVinci Resolve inspired
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(36, 36, 38));
    darkPalette.setColor(QPalette::WindowText, QColor(208, 208, 208));
    darkPalette.setColor(QPalette::Base, QColor(28, 28, 30));
    darkPalette.setColor(QPalette::AlternateBase, QColor(36, 36, 38));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(48, 48, 52));
    darkPalette.setColor(QPalette::ToolTipText, QColor(208, 208, 208));
    darkPalette.setColor(QPalette::Text, QColor(208, 208, 208));
    darkPalette.setColor(QPalette::Button, QColor(48, 48, 52));
    darkPalette.setColor(QPalette::ButtonText, QColor(208, 208, 208));
    darkPalette.setColor(QPalette::BrightText, QColor(255, 100, 100));
    darkPalette.setColor(QPalette::Link, QColor(100, 149, 237));
    darkPalette.setColor(QPalette::Highlight, QColor(70, 100, 150));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 100, 100));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
    app.setPalette(darkPalette);

    // Professional stylesheet
    app.setStyleSheet(R"(
        * {
            font-family: "Segoe UI", "SF Pro Display", sans-serif;
        }
        QMainWindow {
            background-color: #242426;
        }
        QMenuBar {
            background-color: #2d2d30;
            border-bottom: 1px solid #1e1e1e;
            padding: 2px 0;
        }
        QMenuBar::item {
            padding: 6px 12px;
            background: transparent;
        }
        QMenuBar::item:selected {
            background-color: #3d3d42;
            border-radius: 4px;
        }
        QMenu {
            background-color: #2d2d30;
            border: 1px solid #1e1e1e;
            padding: 4px;
        }
        QMenu::item {
            padding: 8px 32px 8px 24px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #3d5c8a;
        }
        QMenu::separator {
            height: 1px;
            background: #3d3d42;
            margin: 4px 8px;
        }
        QToolBar {
            background-color: #2d2d30;
            border: none;
            border-bottom: 1px solid #1e1e1e;
            spacing: 4px;
            padding: 4px 8px;
        }
        QToolBar QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 6px 10px;
            color: #d0d0d0;
        }
        QToolBar QToolButton:hover {
            background-color: #3d3d42;
            border-color: #4d4d52;
        }
        QToolBar QToolButton:pressed {
            background-color: #2a2a2e;
        }
        QToolBar::separator {
            width: 1px;
            background: #3d3d42;
            margin: 6px 8px;
        }
        QPushButton {
            background-color: #3d3d42;
            border: 1px solid #4d4d52;
            border-radius: 4px;
            padding: 8px 16px;
            color: #d0d0d0;
            min-height: 18px;
        }
        QPushButton:hover {
            background-color: #4d4d52;
            border-color: #5d5d62;
        }
        QPushButton:pressed {
            background-color: #2d2d32;
        }
        QPushButton:checked {
            background-color: #3d5c8a;
            border-color: #4d6c9a;
        }
        QPushButton:disabled {
            background-color: #2d2d32;
            color: #666;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: #1e1e1e;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #6a9ed4;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #7aaeE4;
        }
        QSlider::sub-page:horizontal {
            background: #3d5c8a;
            border-radius: 2px;
        }
        QSpinBox, QDoubleSpinBox {
            background-color: #1e1e1e;
            border: 1px solid #3d3d42;
            border-radius: 4px;
            padding: 4px 8px;
            color: #d0d0d0;
            min-height: 24px;
        }
        QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #6a9ed4;
        }
        QSpinBox::up-button, QSpinBox::down-button,
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            width: 20px;
            background: #3d3d42;
            border: none;
            border-left: 1px solid #2d2d30;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover,
        QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {
            background: #4d4d52;
        }
        QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
            background: #2d2d32;
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-bottom: 6px solid #d0d0d0;
            width: 0;
            height: 0;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #d0d0d0;
            width: 0;
            height: 0;
        }
        QSpinBox::up-arrow:hover, QDoubleSpinBox::up-arrow:hover,
        QSpinBox::down-arrow:hover, QDoubleSpinBox::down-arrow:hover {
            border-bottom-color: #ffffff;
            border-top-color: #ffffff;
        }
        QGroupBox {
            font-weight: 600;
            font-size: 11px;
            color: #888;
            border: none;
            margin-top: 16px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 0px;
            padding: 0;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        QRadioButton, QCheckBox {
            spacing: 8px;
            color: #d0d0d0;
        }
        QRadioButton::indicator, QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 1px solid #4d4d52;
            background: #1e1e1e;
        }
        QRadioButton::indicator {
            border-radius: 8px;
        }
        QRadioButton::indicator:checked, QCheckBox::indicator:checked {
            background-color: #6a9ed4;
            border-color: #6a9ed4;
        }
        QStatusBar {
            background-color: #2d2d30;
            border-top: 1px solid #1e1e1e;
            color: #888;
            font-size: 11px;
        }
        QDockWidget {
            font-size: 11px;
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        QDockWidget::title {
            background-color: #2d2d30;
            padding: 8px;
            border-bottom: 1px solid #1e1e1e;
        }
        QScrollArea {
            border: none;
            background: transparent;
        }
        QScrollBar:vertical {
            background: #242426;
            width: 10px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background: #4d4d52;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #5d5d62;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QLineEdit {
            background-color: #1e1e1e;
            border: 1px solid #3d3d42;
            border-radius: 4px;
            padding: 6px 10px;
            color: #d0d0d0;
        }
        QLineEdit:focus {
            border-color: #6a9ed4;
        }
        QToolTip {
            background-color: #3d3d42;
            border: 1px solid #4d4d52;
            color: #d0d0d0;
            padding: 6px 10px;
            border-radius: 4px;
        }
        QLabel#sectionHeader {
            font-weight: 600;
            font-size: 11px;
            color: #888;
            text-transform: uppercase;
            letter-spacing: 1px;
            padding: 8px 0 4px 0;
        }
    )");

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
