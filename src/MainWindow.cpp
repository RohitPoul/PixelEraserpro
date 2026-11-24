#include "MainWindow.h"
#include "CanvasWidget.h"
#include "ImageProcessor.h"
#include "ToolManager.h"
#include "HistoryManager.h"
#include "ExportDialog.h"
#include "SettingsDialog.h"
#include "ResizeDialog.h"
#include "UpscaleDialog.h"
#include "Upscaler.h"
#include "BackgroundRemover.h"
#include "RemoverDialog.h"
#include "GlassWidgets.h"
#include "UpdateChecker.h"
#include "UpdateDialog.h"
#include "Version.h"

#include <QApplication>
#include <algorithm>
#include <cmath>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QShortcut>
#include <QKeySequence>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScrollArea>
#include <QTimer>
#include <QEasingCurve>
#include <QVariantAnimation>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QKeyEvent>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QAbstractButton>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

namespace {
constexpr int SIDEBAR_WIDTH = 276;

QFrame* makeGlassDivider(bool vertical = false) {
    QFrame* line = new QFrame();
    if (vertical) {
        line->setFixedSize(1, 22);
    } else {
        line->setFixedHeight(1);
    }
    line->setStyleSheet("background-color: rgba(255, 255, 255, 22); border: none;");
    return line;
}

QLabel* makeSectionTag(const QString& text) {
    QLabel* lbl = new QLabel(text);
    lbl->setStyleSheet(
        "color: rgba(210, 214, 222, 165); font-size: 10px; font-weight: 700; "
        "letter-spacing: 0.8px; background: transparent;"
    );
    return lbl;
}

bool isSupportedImageExt(const QString& ext) {
    const QString e = ext.toLower();
    return (e == "png" || e == "jpg" || e == "jpeg" || e == "bmp" ||
            e == "webp" || e == "tiff" || e == "tif");
}

QStringList collectSupportedImagePaths(const QStringList& inputs) {
    QStringList out;
    for (const QString& rawPath : inputs) {
        QFileInfo fi(rawPath);
        if (!fi.exists()) continue;
        if (fi.isDir()) {
            QDir dir(fi.absoluteFilePath());
            const QFileInfoList entries = dir.entryInfoList(
                QStringList() << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.webp" << "*.tiff" << "*.tif",
                QDir::Files | QDir::Readable,
                QDir::Name | QDir::IgnoreCase
            );
            for (const QFileInfo& entry : entries) {
                out.append(entry.absoluteFilePath());
            }
        } else if (fi.isFile() && isSupportedImageExt(fi.suffix())) {
            out.append(fi.absoluteFilePath());
        }
    }
    return out;
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_processor(new ImageProcessor())
    , m_toolManager(new ToolManager(this))
    , m_historyManager(new HistoryManager(this))
    , m_updateChecker(new UpdateChecker(this))
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);
    setWindowTitle("PixelEraser Pro");
    setWindowIcon(QIcon(":/icons/app-icon.png"));
    resize(1440, 900);
    setMinimumSize(980, 640);
    setAcceptDrops(true);

    m_historyManager->setImageProcessor(m_processor);

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() {
        m_toastText.clear();
        updateStatusBar();
    });

    setupStudioLayout();
    setupActions();
    setupShortcuts();
    connectSignals();

    // Subscribe to live dark glass theme changes
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &MainWindow::applyThemeVisuals);
    applyThemeVisuals(ThemeManager::instance().current());

    // Setup update checker
    m_updateChecker->setRepository("RohitPoul/PixelEraserpro");
    m_updateChecker->setCurrentVersion(APP_VERSION);
    connect(m_updateChecker, &UpdateChecker::updateAvailable, this, &MainWindow::onUpdateAvailable);
    connect(m_updateChecker, &UpdateChecker::noUpdateAvailable, this, &MainWindow::onNoUpdateAvailable);
    connect(m_updateChecker, &UpdateChecker::checkFailed, this, &MainWindow::onUpdateCheckFailed);

    QTimer::singleShot(3000, this, [this]() {
        m_updateChecker->checkForUpdates(true);
    });

    applyNativeWindowStyle();
    updateStatusBar();
    m_canvas->setFocus();
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);
    delete m_processor;
}

void MainWindow::applyNativeWindowStyle() {
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;
    // Enable native Windows 11 rounded corners & subtle DWM drop shadow on frameless window
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPref, sizeof(cornerPref));
    MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
#endif
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            const qreal dpr = devicePixelRatioF();
            const int gx = GET_X_LPARAM(msg->lParam);
            const int gy = GET_Y_LPARAM(msg->lParam);
            RECT wr;
            GetWindowRect(msg->hwnd, &wr);

            // 8-direction invisible resize border (only when not maximized)
            if (!isMaximized()) {
                const int b = std::max(6, static_cast<int>(6 * dpr));
                const bool left   = (gx >= wr.left && gx < wr.left + b);
                const bool right  = (gx <= wr.right && gx > wr.right - b);
                const bool top    = (gy >= wr.top && gy < wr.top + b);
                const bool bottom = (gy <= wr.bottom && gy > wr.bottom - b);

                if (top && left)     { *result = HTTOPLEFT;     return true; }
                if (top && right)    { *result = HTTOPRIGHT;    return true; }
                if (bottom && left)  { *result = HTBOTTOMLEFT;  return true; }
                if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
                if (left)            { *result = HTLEFT;        return true; }
                if (right)           { *result = HTRIGHT;       return true; }
                if (top)             { *result = HTTOP;         return true; }
                if (bottom)          { *result = HTBOTTOM;      return true; }
            }

            // Native Windows drag / double-click-maximize / Aero Snap on empty top chrome bar space!
            if (m_topChromeBar) {
                QPoint localTop = m_topChromeBar->mapFromGlobal(QPoint(static_cast<int>(gx / dpr), static_cast<int>(gy / dpr)));
                if (m_topChromeBar->rect().contains(localTop)) {
                    QWidget* child = m_topChromeBar->childAt(localTop);
                    bool interactive = false;
                    while (child && child != m_topChromeBar) {
                        if (qobject_cast<QAbstractButton*>(child) || qobject_cast<LiquidGlassTabBar*>(child)) {
                            interactive = true;
                            break;
                        }
                        child = child->parentWidget();
                    }
                    if (!interactive) {
                        *result = HTCAPTION;
                        return true;
                    }
                }
            }
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && m_winMaxBtn) {
        m_winMaxBtn->setIconName(isMaximized() ? "win_restore" : "win_max");
        m_winMaxBtn->setToolTip(isMaximized() ? "Restore Window" : "Maximize Window");
    }
}

void MainWindow::toggleMaximizeRestore() {
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        if (isActiveWindow() && !QApplication::activeModalWidget() && !QApplication::activePopupWidget()) {
            auto* ke = static_cast<QKeyEvent*>(event);
            QWidget* fw = QApplication::focusWidget();
            const bool inTextEditor = (qobject_cast<QLineEdit*>(fw) || qobject_cast<QAbstractSpinBox*>(fw));

            if (ke->key() == Qt::Key_Space && !inTextEditor) {
                if (!ke->isAutoRepeat()) {
                    m_canvas->setSpaceHeld(event->type() == QEvent::KeyPress);
                }
                return true;
            }

            if (event->type() == QEvent::KeyPress && !inTextEditor &&
                (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down)) {
                GlassSliderControl* target = nullptr;
                for (GlassSliderControl* s : {m_toleranceSlider, m_brushSizeSlider, m_softeningSlider, m_compareOpacitySlider}) {
                    if (s && s->underMouse()) {
                        target = s;
                        break;
                    }
                }
                if (!target) {
                    if (m_lastActiveSlider) {
                        target = m_lastActiveSlider;
                    } else if (m_toolManager && m_toolManager->currentTool() == ToolManager::AutoColor) {
                        target = m_toleranceSlider;
                    } else {
                        target = m_brushSizeSlider;
                    }
                }
                if (target) {
                    const int step = (ke->modifiers() & Qt::ControlModifier) ? 10 : 1;
                    target->stepBy(ke->key() == Qt::Key_Up ? step : -step);
                    return true;
                }
            }
        }
    } else if (event->type() == QEvent::WindowDeactivate && watched == this) {
        if (m_canvas) {
            m_canvas->setSpaceHeld(false);
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::hasAnyUnsavedEdits() const {
    if (m_processor && m_processor->hasImage() && m_historyManager && m_historyManager->canUndo()) {
        return true;
    }
    for (const ImageQueueItem& item : m_imageQueue) {
        if (item.hasUnsavedEdits) return true;
    }
    return false;
}

bool MainWindow::confirmSaveBeforeClose() {
    if (!hasAnyUnsavedEdits()) {
        return true;
    }

    int choice = GlassDialog::confirmThreeWay(
        this,
        "Unsaved Changes",
        "You have unsaved edits on the current image.",
        "Export your cutout before closing, or discard your changes?",
        "Export Now",
        "Discard",
        "Cancel"
    );
    if (choice == 1) {
        quickExport();
        return true;
    }
    return (choice == 2);
}

void MainWindow::setupActions() {
    // Global window-level actions (no ugly classic QMenuBar strip needed; everything lives in the sleek Menu popup!)
    auto addGlobal = [this](const QKeySequence& seq, auto slot) -> QAction* {
        QAction* act = new QAction(this);
        act->setShortcut(seq);
        connect(act, &QAction::triggered, this, slot);
        addAction(act);
        return act;
    };

    addGlobal(QKeySequence("Ctrl+N"), &MainWindow::removeAllImages);
    addGlobal(QKeySequence("Ctrl+O"), &MainWindow::openFile);
    addGlobal(QKeySequence("Ctrl+S"), &MainWindow::saveInPlace);
    addGlobal(QKeySequence("Ctrl+Shift+S"), &MainWindow::saveAllEditedImages);
    addGlobal(QKeySequence("Ctrl+D"), &MainWindow::removeCurrentImage);
    addGlobal(QKeySequence("Ctrl+E"), &MainWindow::quickExport);
    addGlobal(QKeySequence("PageUp"), &MainWindow::previousImage);
    addGlobal(QKeySequence("PageDown"), &MainWindow::nextImage);
    addGlobal(QKeySequence("Ctrl+Left"), &MainWindow::previousImage);
    addGlobal(QKeySequence("Ctrl+Right"), &MainWindow::nextImage);

    m_undoAction = addGlobal(QKeySequence("Ctrl+Z"), &MainWindow::undo);
    m_undoAction->setEnabled(false);
    m_redoAction = addGlobal(QKeySequence("Ctrl+Y"), &MainWindow::redo);
    m_redoAction->setEnabled(false);
    addGlobal(QKeySequence("Ctrl+Shift+Z"), &MainWindow::redo);

    addGlobal(QKeySequence("Ctrl+Alt+R"), &MainWindow::resizeImage);
    addGlobal(QKeySequence("Ctrl+Alt+U"), &MainWindow::upscaleImage);
    addGlobal(QKeySequence("Ctrl+Alt+B"), &MainWindow::openRemover);
    addGlobal(QKeySequence("Ctrl+Alt+M"), &MainWindow::applyMaskFromFile);
    addGlobal(QKeySequence("Ctrl+Alt+O"), &MainWindow::restoreFullOriginalImage);

    addGlobal(QKeySequence("Ctrl++"), &MainWindow::zoomIn);
    addGlobal(QKeySequence("Ctrl+-"), &MainWindow::zoomOut);
    addGlobal(QKeySequence("Ctrl+0"), &MainWindow::fitToScreen);
    addGlobal(QKeySequence("Ctrl+1"), &MainWindow::zoomToActual);

    m_compareAction = addGlobal(QKeySequence("H"), &MainWindow::toggleCompareOriginal);
    m_compareAction->setCheckable(true);

    addGlobal(QKeySequence("I"), &MainWindow::toggleInfraredMode);
    addGlobal(QKeySequence("Shift+I"), &MainWindow::cleanInfraredDust);
    m_toggleSidebarAction = addGlobal(QKeySequence("Tab"), &MainWindow::toggleSidebar);
    addGlobal(QKeySequence("F1"), &MainWindow::showShortcuts);
    addGlobal(QKeySequence("Ctrl+,"), &MainWindow::openSettings);
}

void MainWindow::showMainMenu() {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: rgba(28, 30, 34, 248); border: 1px solid rgba(255,255,255,54); "
        "border-radius: 14px; padding: 6px; color: #eceef2; }"
        "QMenu::item { padding: 7px 26px 7px 14px; border-radius: 8px; }"
        "QMenu::item:selected { background-color: rgba(255,255,255,34); color: #ffffff; }"
        "QMenu::separator { height: 1px; background: rgba(255,255,255,22); margin: 5px 10px; }"
    );
    menu.addAction("Open / Add Images...   Ctrl+O", this, &MainWindow::openFile);
    menu.addAction("Add Folder of Images...", this, &MainWindow::addImageFolder);
    QAction* actSave = menu.addAction("Save Over Original     Ctrl+S", this, &MainWindow::saveInPlace);
    actSave->setEnabled(m_processor && m_processor->hasImage() && !m_currentFilePath.isEmpty());
    QAction* actSyncAll = menu.addAction("Sync / Save All Edited Ctrl+Shift+S", this, &MainWindow::saveAllEditedImages);
    actSyncAll->setEnabled(hasAnyUnsavedEdits());
    menu.addAction("Export Copy...         Ctrl+E", this, &MainWindow::quickExport);
    menu.addSeparator();
    QAction* actRemCur = menu.addAction("Remove Current Image   Ctrl+D", this, &MainWindow::removeCurrentImage);
    actRemCur->setEnabled(m_currentQueueIndex >= 0 || (m_processor && m_processor->hasImage()));
    QAction* actRemAll = menu.addAction(
        m_imageQueue.size() > 1 ? QString("Remove All Images (%1)").arg(m_imageQueue.size()) : QString("Remove All Images"),
        this, &MainWindow::removeAllImages);
    actRemAll->setEnabled(!m_imageQueue.isEmpty() || (m_processor && m_processor->hasImage()));
    menu.addSeparator();
    menu.addAction("G4 AI Remover...       Ctrl+Alt+B", this, &MainWindow::openRemover);
    menu.addAction("Apply Mask / Cutout... Ctrl+Alt+M", this, &MainWindow::applyMaskFromFile);
    menu.addAction("Restore Full Original  Ctrl+Alt+O", this, &MainWindow::restoreFullOriginalImage);
    menu.addAction("AI Upscale...          Ctrl+Alt+U", this, &MainWindow::upscaleImage);
    menu.addAction("Resize Canvas...       Ctrl+Alt+R", this, &MainWindow::resizeImage);
    menu.addSeparator();
    menu.addAction("Infrared Dust Scan     I", this, &MainWindow::toggleInfraredMode);
    menu.addAction("Clean Infrared Dust    Shift+I", this, &MainWindow::cleanInfraredDust);
    menu.addAction("Compare Original       H", this, &MainWindow::toggleCompareOriginal);
    menu.addAction("Fit to Screen          Ctrl+0", this, &MainWindow::fitToScreen);
    menu.addAction("Actual Size (100%)     Ctrl+1", this, &MainWindow::zoomToActual);
    menu.addAction("Toggle Tools Panel     Tab", this, &MainWindow::toggleSidebar);
    menu.addSeparator();
    QMenu* themeSub = menu.addMenu("Glass Theme");
    const auto& themes = ThemeManager::instance().themes();
    for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
        QAction* ta = themeSub->addAction(themes[i].name, this, [i]() {
            ThemeManager::instance().setThemeIndex(i);
        });
        ta->setCheckable(true);
        ta->setChecked(i == ThemeManager::instance().currentIndex());
    }
    menu.addSeparator();
    menu.addAction("Settings...            Ctrl+,", this, &MainWindow::openSettings);
    menu.addAction("Keyboard Shortcuts     F1", this, &MainWindow::showShortcuts);
    menu.addAction("Check for Updates...", this, &MainWindow::checkForUpdates);
    menu.addAction("About PixelEraser Pro", this, [this]() {
        GlassDialog::info(this, "About PixelEraser Pro",
            QString("PixelEraser Pro v%1").arg(APP_VERSION),
            "Dark Smoked Liquid Glass Masking & Dataset Studio.\nPowered by Qt 6, OpenCV & ToonOut G4 ONNX Engine.");
    });

    QPoint pos = m_menuBtn->mapToGlobal(QPoint(0, m_menuBtn->height() + 6));
    menu.exec(pos);
}

void MainWindow::setupStudioLayout() {
    QWidget* root = new QWidget(this);
    root->setObjectName("StudioRoot");
    root->setStyleSheet("QWidget#StudioRoot { background-color: #16171a; }");
    setCentralWidget(root);

    QVBoxLayout* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // ========================================================================
    // 1. UNIFIED FRAMELESS LIQUID GLASS TOP CHROME BAR (Replaces ugly Windows titlebar + toolbar!)
    // ========================================================================
    m_topChromeBar = new GlassPanel(root);
    m_topChromeBar->setCornerRadius(0.0);
    m_topChromeBar->setElevated(false);
    m_topChromeBar->setFixedHeight(48);

    QHBoxLayout* topLay = new QHBoxLayout(m_topChromeBar);
    topLay->setContentsMargins(12, 6, 10, 6);
    topLay->setSpacing(6);

    // App Icon + Brand + Filename Pill (Draggable title area)
    QLabel* iconLbl = new QLabel();
    iconLbl->setPixmap(QIcon(":/icons/app-icon.png").pixmap(20, 20));
    iconLbl->setFixedSize(22, 22);
    iconLbl->setStyleSheet("background: transparent;");

    QLabel* brandLbl = new QLabel("PixelEraser Pro");
    brandLbl->setStyleSheet("color: #f2f4f8; font-size: 13px; font-weight: 700; background: transparent;");

    m_titleFileLabel = new QLabel();
    m_titleFileLabel->setVisible(false);
    m_titleFileLabel->setStyleSheet(
        "color: #cfd4de; font-size: 11px; font-weight: 600; "
        "background-color: rgba(255, 255, 255, 18); border: 1px solid rgba(255, 255, 255, 42); "
        "border-radius: 9px; padding: 2px 9px;"
    );

    m_queueNavWrap = new QWidget();
    m_queueNavWrap->setStyleSheet("background: transparent;");
    m_queueNavWrap->setVisible(false);
    QHBoxLayout* qNavLay = new QHBoxLayout(m_queueNavWrap);
    qNavLay->setContentsMargins(0, 0, 0, 0);
    qNavLay->setSpacing(3);

    m_prevImgBtn = new GlassButton("", GlassButton::IconCircle);
    m_prevImgBtn->setIconName("chevron_left");
    m_prevImgBtn->setFixedSize(26, 26);
    m_prevImgBtn->setToolTip("Previous Image (PageUp / Ctrl+Left)");
    connect(m_prevImgBtn, &QPushButton::clicked, this, &MainWindow::previousImage);

    m_queueCountLabel = new QLabel("1 / 1");
    m_queueCountLabel->setStyleSheet(
        "color: #e6eaf2; font-size: 11px; font-weight: 700; "
        "background-color: rgba(255, 255, 255, 16); border: 1px solid rgba(255, 255, 255, 36); "
        "border-radius: 8px; padding: 2px 8px;"
    );

    m_nextImgBtn = new GlassButton("", GlassButton::IconCircle);
    m_nextImgBtn->setIconName("chevron_right");
    m_nextImgBtn->setFixedSize(26, 26);
    m_nextImgBtn->setToolTip("Next Image (PageDown / Ctrl+Right)");
    connect(m_nextImgBtn, &QPushButton::clicked, this, &MainWindow::nextImage);

    qNavLay->addWidget(m_prevImgBtn);
    qNavLay->addWidget(m_queueCountLabel);
    qNavLay->addWidget(m_nextImgBtn);

    m_menuBtn = new GlassButton("", GlassButton::IconCircle);
    m_menuBtn->setIconName("menu");
    m_menuBtn->setFixedSize(32, 32);
    m_menuBtn->setToolTip("Menu (File · Image · View · Theme · Help)");
    connect(m_menuBtn, &QPushButton::clicked, this, &MainWindow::showMainMenu);

    GlassButton* openBtn = new GlassButton("Open", GlassButton::Ghost);
    openBtn->setIconName("open");
    openBtn->setToolTip("Open / Add Images — Multi-Select Supported (Ctrl+O)");
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openFile);

    GlassButton* discardBtn = new GlassButton("Discard", GlassButton::Ghost);
    discardBtn->setIconName("discard");
    discardBtn->setToolTip("Remove Current Image (Ctrl+D)");
    connect(discardBtn, &QPushButton::clicked, this, &MainWindow::removeCurrentImage);

    GlassButton* exportBtn = new GlassButton("Export", GlassButton::White);
    exportBtn->setIconName("export");
    exportBtn->setToolTip("Export Transparent PNG (Ctrl+E)");
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::quickExport);

    m_undoBtn = new GlassButton("", GlassButton::IconCircle);
    m_undoBtn->setIconName("undo");
    m_undoBtn->setFixedSize(32, 32);
    m_undoBtn->setToolTip("Undo (Ctrl+Z)");
    m_undoBtn->setEnabled(false);
    connect(m_undoBtn, &QPushButton::clicked, this, &MainWindow::undo);

    m_redoBtn = new GlassButton("", GlassButton::IconCircle);
    m_redoBtn->setIconName("redo");
    m_redoBtn->setFixedSize(32, 32);
    m_redoBtn->setToolTip("Redo (Ctrl+Y)");
    m_redoBtn->setEnabled(false);
    connect(m_redoBtn, &QPushButton::clicked, this, &MainWindow::redo);

    GlassButton* zOutBtn = new GlassButton("", GlassButton::IconCircle);
    zOutBtn->setIconName("zoom_out");
    zOutBtn->setFixedSize(28, 28);
    zOutBtn->setToolTip("Zoom Out (-)");
    connect(zOutBtn, &QPushButton::clicked, this, &MainWindow::zoomOut);

    m_zoomLabel = new QLabel("100%");
    m_zoomLabel->setFixedWidth(42);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setStyleSheet("color: #eceef2; font-weight: 700; font-size: 11px; background: transparent;");

    GlassButton* zInBtn = new GlassButton("", GlassButton::IconCircle);
    zInBtn->setIconName("zoom_in");
    zInBtn->setFixedSize(28, 28);
    zInBtn->setToolTip("Zoom In (=)");
    connect(zInBtn, &QPushButton::clicked, this, &MainWindow::zoomIn);

    GlassButton* fitBtn = new GlassButton("Fit", GlassButton::Ghost);
    fitBtn->setIconName("fit");
    fitBtn->setToolTip("Fit to Screen (Ctrl+0)");
    connect(fitBtn, &QPushButton::clicked, this, &MainWindow::fitToScreen);

    GlassButton* aiRemoverBtn = new GlassButton("G4 Remover", GlassButton::Primary);
    aiRemoverBtn->setIconName("sparkle");
    aiRemoverBtn->setToolTip("ToonOut G4 AI Background Remover (Ctrl+Alt+B)");
    connect(aiRemoverBtn, &QPushButton::clicked, this, &MainWindow::openRemover);

    GlassButton* applyMaskBtn = new GlassButton("Apply Mask", GlassButton::Ghost);
    applyMaskBtn->setIconName("mask");
    applyMaskBtn->setToolTip("Remove Background Using Mask or Transparent Image of Matching Dimensions (Ctrl+Alt+M)");
    connect(applyMaskBtn, &QPushButton::clicked, this, &MainWindow::applyMaskFromFile);

    GlassButton* upscaleBtn = new GlassButton("Upscale", GlassButton::Ghost);
    upscaleBtn->setIconName("upscale");
    upscaleBtn->setToolTip("Real-ESRGAN AI Upscaler (Ctrl+Alt+U)");
    connect(upscaleBtn, &QPushButton::clicked, this, &MainWindow::upscaleImage);

    m_panelToggleBtn = new GlassButton("Panel", GlassButton::Ghost);
    m_panelToggleBtn->setIconName("sidebar");
    m_panelToggleBtn->setCheckable(true);
    m_panelToggleBtn->setChecked(true);
    m_panelToggleBtn->setToolTip("Toggle Left Tools Panel (Tab)");
    connect(m_panelToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleSidebar);

    GlassButton* settingsBtn = new GlassButton("", GlassButton::IconCircle);
    settingsBtn->setIconName("settings");
    settingsBtn->setFixedSize(32, 32);
    settingsBtn->setToolTip("Settings — Export Naming & File Type (Ctrl+,)");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);

    // Custom Liquid Glass Window Controls (Minimize / Maximize / Close)
    GlassButton* winMinBtn = new GlassButton("", GlassButton::IconCircle);
    winMinBtn->setIconName("win_min");
    winMinBtn->setFixedSize(30, 30);
    winMinBtn->setToolTip("Minimize");
    connect(winMinBtn, &QPushButton::clicked, this, &QWidget::showMinimized);

    m_winMaxBtn = new GlassButton("", GlassButton::IconCircle);
    m_winMaxBtn->setIconName("win_max");
    m_winMaxBtn->setFixedSize(30, 30);
    m_winMaxBtn->setToolTip("Maximize Window");
    connect(m_winMaxBtn, &QPushButton::clicked, this, &MainWindow::toggleMaximizeRestore);

    GlassButton* winCloseBtn = new GlassButton("", GlassButton::Danger);
    winCloseBtn->setIconName("win_close");
    winCloseBtn->setFixedSize(30, 30);
    winCloseBtn->setCornerRadius(15.0);
    winCloseBtn->setToolTip("Close");
    connect(winCloseBtn, &QPushButton::clicked, this, &QWidget::close);

    topLay->addWidget(iconLbl);
    topLay->addWidget(brandLbl);
    topLay->addWidget(m_titleFileLabel);
    topLay->addWidget(m_queueNavWrap);
    topLay->addSpacing(6);
    topLay->addWidget(makeGlassDivider(true));
    topLay->addWidget(m_menuBtn);
    topLay->addWidget(openBtn);
    topLay->addWidget(discardBtn);
    topLay->addWidget(exportBtn);
    topLay->addWidget(makeGlassDivider(true));
    topLay->addWidget(m_undoBtn);
    topLay->addWidget(m_redoBtn);
    topLay->addWidget(makeGlassDivider(true));
    topLay->addWidget(zOutBtn);
    topLay->addWidget(m_zoomLabel);
    topLay->addWidget(zInBtn);
    topLay->addWidget(fitBtn);
    topLay->addWidget(makeGlassDivider(true));
    topLay->addWidget(aiRemoverBtn);
    topLay->addWidget(applyMaskBtn);
    topLay->addWidget(upscaleBtn);
    topLay->addWidget(m_panelToggleBtn);
    topLay->addWidget(settingsBtn);
    topLay->addStretch(1);
    topLay->addWidget(winMinBtn);
    topLay->addWidget(m_winMaxBtn);
    topLay->addWidget(winCloseBtn);

    rootLay->addWidget(m_topChromeBar);

    // ========================================================================
    // 2. WORKSPACE ROW: DOCKED LEFT LIQUID GLASS TOOLS SIDEBAR + UNOBSTRUCTED CANVAS
    //    (Matches original Image 3 structure AND gives 0.00ms HUD overdraw during pan/zoom!)
    // ========================================================================
    QHBoxLayout* workRow = new QHBoxLayout();
    workRow->setContentsMargins(0, 0, 0, 0);
    workRow->setSpacing(0);

    m_sidebarPanel = new GlassPanel(root);
    m_sidebarPanel->setCornerRadius(0.0);
    m_sidebarPanel->setElevated(false);
    m_sidebarPanel->setFixedWidth(SIDEBAR_WIDTH);

    QVBoxLayout* sideOuter = new QVBoxLayout(m_sidebarPanel);
    sideOuter->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scrollArea = new QScrollArea(m_sidebarPanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFocusPolicy(Qt::NoFocus);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    scrollArea->viewport()->setAutoFillBackground(false);
    scrollArea->viewport()->setStyleSheet("background: transparent;");

    QWidget* sideBody = new QWidget();
    sideBody->setAutoFillBackground(false);
    sideBody->setStyleSheet("background: transparent;");
    QVBoxLayout* lay = new QVBoxLayout(sideBody);
    lay->setContentsMargins(14, 14, 14, 14);
    lay->setSpacing(10);

    // 1) TOOLS — Smooth Sliding LiquidGlassTabBar (Same top slot as original!)
    lay->addWidget(makeSectionTag("TOOLS"));

    GlassPanel* toolWrap = new GlassPanel();
    toolWrap->setCornerRadius(19.0);
    toolWrap->setSubSurface(true);
    toolWrap->setElevated(false);
    toolWrap->setFixedHeight(40);
    QHBoxLayout* twLay = new QHBoxLayout(toolWrap);
    twLay->setContentsMargins(3, 2, 3, 2);

    m_toolTabBar = new LiquidGlassTabBar();
    m_toolTabBar->setPillHeight(36);
    m_toolTabBar->addTab(ToolManager::AutoColor, "Auto", "wand", "", "Auto Color Remove (A)");
    m_toolTabBar->addTab(ToolManager::ManualErase, "Eraser", "eraser", "", "Precision Eraser (E)");
    m_toolTabBar->addTab(ToolManager::Repair, "Repair", "repair", "", "Smart Repair Brush (R)");
    m_toolTabBar->setCurrentId(ToolManager::AutoColor, false);
    twLay->addWidget(m_toolTabBar);
    lay->addWidget(toolWrap);

    lay->addWidget(makeGlassDivider(false));

    // 2) TOLERANCE / BRUSH SIZE / EDGE SOFTENING (with precision ▲/▼ steppers!)
    m_toleranceSlider = new GlassSliderControl("Tolerance", 0, 255, 50);
    m_brushSizeSlider = new GlassSliderControl("Brush Size", 1, 200, 10, " px");
    m_softeningSlider = new GlassSliderControl("Edge Softening", 0, 5, 0);
    m_softeningSlider->setZeroLabel("Off");

    lay->addWidget(m_toleranceSlider);
    lay->addWidget(m_brushSizeSlider);
    lay->addWidget(m_softeningSlider);

    lay->addWidget(makeGlassDivider(false));

    // 3) PREVIEW BACKGROUND / CANVAS SURFACE (Whole-Canvas: Dark | White | Black | Infrared Dust Scan)
    lay->addWidget(makeSectionTag("PREVIEW BACKGROUND"));

    m_bgGroup = new QButtonGroup(this);
    m_bgGroup->setExclusive(true);

    GlassSwatchButton* swDark     = new GlassSwatchButton(CanvasWidget::Dark, "Dark", "Classic Neutral Gray Checkerboard (Whole Canvas)");
    GlassSwatchButton* swWhite    = new GlassSwatchButton(CanvasWidget::White, "White", "Complete Pure White (Whole Canvas)");
    GlassSwatchButton* swBlack    = new GlassSwatchButton(CanvasWidget::Black, "Black", "Complete Pitch Black (Whole Canvas)");
    GlassSwatchButton* swInfrared = new GlassSwatchButton(CanvasWidget::Infrared, "Infrared", "Infrared Alpha Dust & Stray Particle Inspector (I)");
    swDark->setChecked(true);

    m_bgGroup->addButton(swDark, CanvasWidget::Dark);
    m_bgGroup->addButton(swWhite, CanvasWidget::White);
    m_bgGroup->addButton(swBlack, CanvasWidget::Black);
    m_bgGroup->addButton(swInfrared, CanvasWidget::Infrared);

    QHBoxLayout* swRow = new QHBoxLayout();
    swRow->setSpacing(6);
    swRow->addWidget(swDark);
    swRow->addWidget(swWhite);
    swRow->addWidget(swBlack);
    swRow->addWidget(swInfrared);
    lay->addLayout(swRow);

    m_cleanIrDustBtn = new GlassButton("Clean Infrared Dust", GlassButton::Danger);
    m_cleanIrDustBtn->setIconName("infrared");
    m_cleanIrDustBtn->setFixedHeight(34);
    m_cleanIrDustBtn->setToolTip("Remove ONLY the exact infrared-highlighted dust pixels (Shift+I · Undoable with Ctrl+Z)");
    m_cleanIrDustBtn->setVisible(false);
    connect(m_cleanIrDustBtn, &QPushButton::clicked, this, &MainWindow::cleanInfraredDust);
    lay->addWidget(m_cleanIrDustBtn);

    lay->addWidget(makeGlassDivider(false));

    // 4) COMPARE ORIGINAL (Toggle H + Opacity slider)
    QHBoxLayout* cmpRow = new QHBoxLayout();
    QLabel* cmpLbl = new QLabel("Compare Original  [H]");
    cmpLbl->setStyleSheet("color: #e4e7ee; font-size: 12px; font-weight: 600; background: transparent;");
    m_compareSwitch = new GlassToggleSwitch();
    m_compareSwitch->setToolTip("Toggle original image overlay (Shortcut: H)");
    cmpRow->addWidget(cmpLbl);
    cmpRow->addStretch();
    cmpRow->addWidget(m_compareSwitch);
    lay->addLayout(cmpRow);

    m_compareOpacitySlider = new GlassSliderControl("Compare Opacity", 0, 100, 100, "%");
    m_compareOpacitySlider->setUseThemeToggleColor(true);
    lay->addWidget(m_compareOpacitySlider);

    lay->addWidget(makeGlassDivider(false));

    // 5) DARK GLASS THEME PICKER
    QHBoxLayout* themeHdr = new QHBoxLayout();
    themeHdr->addWidget(makeSectionTag("GLASS THEME"));
    themeHdr->addStretch();
    m_themeNameLabel = new QLabel(ThemeManager::instance().current().name);
    m_themeNameLabel->setStyleSheet("color: rgba(230, 234, 240, 155); font-size: 10px; font-weight: 600; background: transparent;");
    themeHdr->addWidget(m_themeNameLabel);
    lay->addLayout(themeHdr);

    GlassThemePicker* themePicker = new GlassThemePicker();
    lay->addWidget(themePicker);

    lay->addWidget(makeGlassDivider(false));

    // 6) AI STUDIO
    lay->addWidget(makeSectionTag("AI STUDIO"));

    GlassActionRow* rowRemover = new GlassActionRow("sparkle", "G4 AI Remover", "1024 / 1536 / 2048px SmartEdge", true);
    connect(rowRemover, &QPushButton::clicked, this, &MainWindow::openRemover);

    GlassActionRow* rowMask = new GlassActionRow("mask", "Apply Mask / Cutout", "From Mask or Transparent PNG", true);
    connect(rowMask, &QPushButton::clicked, this, &MainWindow::applyMaskFromFile);

    GlassActionRow* rowUpscale = new GlassActionRow("upscale", "AI Upscaler", "Real-ESRGAN 2× & 4×", true);
    connect(rowUpscale, &QPushButton::clicked, this, &MainWindow::upscaleImage);

    GlassActionRow* rowResize = new GlassActionRow("resize", "Resize Canvas", "Lanczos4 Resampling", true);
    connect(rowResize, &QPushButton::clicked, this, &MainWindow::resizeImage);

    lay->addWidget(rowRemover);
    lay->addWidget(rowMask);
    lay->addWidget(rowUpscale);
    lay->addWidget(rowResize);

    // 7) PATH-ONLY IMAGE QUEUE (Zero RAM thumbnails — only active image decoded in memory!)
    m_queueSection = new QWidget();
    m_queueSection->setStyleSheet("background: transparent;");
    m_queueSection->setVisible(false);
    QVBoxLayout* qSecLay = new QVBoxLayout(m_queueSection);
    qSecLay->setContentsMargins(0, 4, 0, 0);
    qSecLay->setSpacing(6);
    qSecLay->addWidget(makeGlassDivider(false));

    QHBoxLayout* qHdr = new QHBoxLayout();
    qHdr->setContentsMargins(0, 2, 0, 0);
    qHdr->setSpacing(5);
    m_queueHeaderLabel = makeSectionTag("IMAGE QUEUE (0)");

    GlassButton* qSyncBtn = new GlassButton("Sync", GlassButton::Primary);
    qSyncBtn->setIconName("sync");
    qSyncBtn->setFixedHeight(22);
    qSyncBtn->setToolTip("Sync / Save All Edited Images in Queue (Ctrl+Shift+S)");
    connect(qSyncBtn, &QPushButton::clicked, this, &MainWindow::saveAllEditedImages);

    GlassButton* qClearBtn = new GlassButton("Clear", GlassButton::Ghost);
    qClearBtn->setFixedHeight(22);
    qClearBtn->setToolTip("Remove All Images from Queue");
    connect(qClearBtn, &QPushButton::clicked, this, &MainWindow::removeAllImages);

    qHdr->addWidget(m_queueHeaderLabel);
    qHdr->addStretch();
    qHdr->addWidget(qSyncBtn);
    qHdr->addWidget(qClearBtn);
    qSecLay->addLayout(qHdr);

    m_queueList = new QListWidget();
    m_queueList->setFixedHeight(168);
    m_queueList->setFocusPolicy(Qt::NoFocus);
    m_queueList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_queueList->setTextElideMode(Qt::ElideMiddle);
    m_queueList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_queueList->setStyleSheet(
        "QListWidget { "
        "  background-color: rgba(14, 16, 20, 175); border: 1px solid rgba(255, 255, 255, 28); "
        "  border-radius: 12px; padding: 4px; color: #cfd5e0; font-size: 11px; font-weight: 600; "
        "}"
        "QListWidget::item { padding: 5px 8px; border-radius: 7px; }"
        "QListWidget::item:hover { background-color: rgba(255, 255, 255, 18); }"
        "QListWidget::item:selected { background-color: rgba(255, 255, 255, 36); color: #ffffff; }"
    );
    connect(m_queueList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        int row = m_queueList->row(item);
        if (row >= 0 && row < m_imageQueue.size()) {
            switchToQueueIndex(row);
        }
    });
    connect(m_queueList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = m_queueList->itemAt(pos);
        if (!item) return;
        int row = m_queueList->row(item);
        if (row < 0 || row >= m_imageQueue.size()) return;
        QMenu ctx(this);
        ctx.setStyleSheet(
            "QMenu { background-color: rgba(28, 30, 34, 248); border: 1px solid rgba(255,255,255,54); "
            "border-radius: 10px; padding: 4px; color: #eceef2; }"
            "QMenu::item { padding: 6px 18px 6px 12px; border-radius: 6px; }"
            "QMenu::item:selected { background-color: rgba(255,255,255,34); color: #ffffff; }"
        );
        ctx.addAction("Open This Image", this, [this, row]() { switchToQueueIndex(row); });
        ctx.addAction("Remove from Queue", this, [this, row]() { removeQueueIndex(row, false); });
        ctx.addSeparator();
        ctx.addAction("Remove All Images", this, &MainWindow::removeAllImages);
        ctx.exec(m_queueList->viewport()->mapToGlobal(pos));
    });
    qSecLay->addWidget(m_queueList);

    lay->addWidget(m_queueSection);
    lay->addStretch(1);

    scrollArea->setWidget(sideBody);
    sideOuter->addWidget(scrollArea);

    // Smooth sidebar collapse/expand animation
    m_sidebarAnim = new QVariantAnimation(this);
    m_sidebarAnim->setDuration(220);
    m_sidebarAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_sidebarAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        int w = val.toInt();
        m_sidebarPanel->setFixedWidth(w);
        m_sidebarPanel->setVisible(w > 2);
    });

    // Unobstructed Full-Viewport CanvasWidget
    m_canvas = new CanvasWidget(root);
    m_canvas->setImageProcessor(m_processor);
    m_canvas->setToolManager(m_toolManager);
    m_canvas->setHistoryManager(m_historyManager);

    workRow->addWidget(m_sidebarPanel);
    workRow->addWidget(m_canvas, 1);
    rootLay->addLayout(workRow, 1);

    // ========================================================================
    // 3. BOTTOM LIQUID GLASS STATUS STRIP (Matches original Image 3, zero canvas overlap!)
    // ========================================================================
    m_statusBarPanel = new GlassPanel(root);
    m_statusBarPanel->setCornerRadius(0.0);
    m_statusBarPanel->setElevated(false);
    m_statusBarPanel->setFixedHeight(28);

    QHBoxLayout* stLay = new QHBoxLayout(m_statusBarPanel);
    stLay->setContentsMargins(14, 0, 14, 0);
    stLay->setSpacing(12);

    m_statusLeftLabel = new QLabel("Zoom: 100%  ·  No Image");
    m_statusLeftLabel->setStyleSheet("color: #c8ccd4; font-size: 11px; font-weight: 600; background: transparent;");

    m_statusToastLabel = new QLabel();
    m_statusToastLabel->setStyleSheet("color: #f0f2f6; font-size: 11px; font-weight: 600; background: transparent;");

    m_statusCoordLabel = new QLabel("X: —   Y: —");
    m_statusCoordLabel->setStyleSheet("color: #aeb4be; font-size: 11px; font-weight: 600; background: transparent;");

    stLay->addWidget(m_statusLeftLabel);
    stLay->addWidget(m_statusToastLabel, 1);
    stLay->addWidget(m_statusCoordLabel);
    rootLay->addWidget(m_statusBarPanel);

    // ========================================================================
    // 4. TEMPORARY FLOATING LIQUID GLASS AI PROGRESS OVERLAY (Pause / Resume / Cancel)
    // ========================================================================
    m_progressOverlay = new GlassPanel(m_canvas);
    m_progressOverlay->setCornerRadius(22.0);
    m_progressOverlay->setFixedSize(468, 194);
    m_progressOverlay->setVisible(false);

    QVBoxLayout* progLay = new QVBoxLayout(m_progressOverlay);
    progLay->setContentsMargins(22, 18, 22, 16);
    progLay->setSpacing(9);

    QHBoxLayout* progTop = new QHBoxLayout();
    progTop->setSpacing(8);
    m_progTitleLabel = new QLabel("G4 AI Background Remover");
    m_progTitleLabel->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: 700; background: transparent;");

    m_progDeviceBadge = new QLabel("Init");
    m_progDeviceBadge->setStyleSheet(
        "color: #9be7c4; font-size: 10px; font-weight: 700; "
        "background-color: rgba(46, 204, 113, 28); border: 1px solid rgba(82, 230, 152, 90); "
        "border-radius: 8px; padding: 2px 8px;"
    );

    m_progPctLabel = new QLabel("0%");
    m_progPctLabel->setStyleSheet(
        "color: #eceef2; font-size: 12px; font-weight: 700; "
        "background-color: rgba(255, 255, 255, 28); border: 1px solid rgba(255, 255, 255, 76); "
        "border-radius: 10px; padding: 2px 10px;"
    );
    progTop->addWidget(m_progTitleLabel);
    progTop->addWidget(m_progDeviceBadge);
    progTop->addStretch();
    progTop->addWidget(m_progPctLabel);
    progLay->addLayout(progTop);

    m_progSubLabel = new QLabel("Initializing...");
    m_progSubLabel->setStyleSheet("color: #d6dae3; font-size: 12px; background: transparent;");
    progLay->addWidget(m_progSubLabel);

    m_progressBar = new QProgressBar();
    m_progressBar->setFixedHeight(8);
    m_progressBar->setTextVisible(false);
    progLay->addWidget(m_progressBar);

    m_progMetaLabel = new QLabel("Elapsed 0.0s · Live GPU stream");
    m_progMetaLabel->setStyleSheet("color: #8f96a4; font-size: 11px; background: transparent;");
    progLay->addWidget(m_progMetaLabel);

    m_progBtnContainer = new QWidget();
    m_progBtnContainer->setStyleSheet("background: transparent;");
    QHBoxLayout* progBtnLay = new QHBoxLayout(m_progBtnContainer);
    progBtnLay->setContentsMargins(0, 2, 0, 0);
    progBtnLay->setSpacing(8);
    progBtnLay->addStretch();

    m_progPauseBtn = new GlassButton("Pause", GlassButton::Ghost);
    m_progPauseBtn->setIconName("pause");
    m_progPauseBtn->setMinimumSize(112, 34);
    connect(m_progPauseBtn, &QPushButton::clicked, this, &MainWindow::togglePauseActiveJob);

    m_progCancelBtn = new GlassButton("Cancel", GlassButton::Danger);
    m_progCancelBtn->setIconName("stop");
    m_progCancelBtn->setMinimumSize(96, 34);
    connect(m_progCancelBtn, &QPushButton::clicked, this, &MainWindow::cancelActiveJob);

    progBtnLay->addWidget(m_progPauseBtn);
    progBtnLay->addWidget(m_progCancelBtn);
    progLay->addWidget(m_progBtnContainer);

    m_progTicker = new QTimer(this);
    m_progTicker->setInterval(50); // 20Hz live bar & elapsed timer refresh
    connect(m_progTicker, &QTimer::timeout, this, &MainWindow::updateProgressTelemetry);
}

void MainWindow::updateProgressPosition() {
    if (m_progressOverlay && m_canvas) {
        m_progressOverlay->move((m_canvas->width() - m_progressOverlay->width()) / 2,
                                (m_canvas->height() - m_progressOverlay->height()) / 2);
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateProgressPosition();
}

quint64 MainWindow::beginActiveJob(ActiveJob job, const QString& title, const QString& subtitle) {
    m_activeJob = job;
    m_workerRunning = true;
    m_jobPaused = false;
    const quint64 token = ++m_activeJobToken;
    m_progPauseBtn->setText("Pause");
    m_progPauseBtn->setIconName("pause");
    m_progPauseBtn->setVariant(GlassButton::Ghost);
    m_progPauseBtn->setEnabled(true);
    m_progCancelBtn->setEnabled(true);
    showGlassProgress(true, title, subtitle, 0, true);
    QApplication::processEvents();
    return token;
}

void MainWindow::showGlassProgress(bool show, const QString& title, const QString& subtitle, int percent, bool showControls) {
    if (!m_progressOverlay) return;
    if (!show) {
        if (m_progTicker) m_progTicker->stop();
        m_progressOverlay->setVisible(false);
        return;
    }
    if (!title.isEmpty()) m_progTitleLabel->setText(title);
    if (!subtitle.isEmpty()) m_progSubLabel->setText(subtitle);
    if (percent < 0) {
        m_progressBar->setRange(0, 0);
        m_progAccumulatedMs = 0;
        m_progActiveSegmentTimer.restart();
        m_progPctLabel->setText(m_jobPaused ? "Paused" : "Working");
    } else {
        m_progressBar->setRange(0, 1000);
        if (percent == 0 || m_activeJob != JobNone) {
            // Reset or update target directly when caller drives showGlassProgress(true, ..., 0)
            if (percent == 0) {
                m_progBasePct = 0;
                m_progNextPct = 5;
                m_progDisplayPct = 0.0;
                m_progAccumulatedMs = 0;
                m_progBackendText = "Probing GPU";
                setProgressBackend(m_progBackendText);
                m_progActiveSegmentTimer.restart();
                m_progStageTimer.restart();
            } else if (percent > m_progBasePct) {
                m_progBasePct = percent;
                m_progDisplayPct = std::max(m_progDisplayPct, static_cast<double>(percent));
            }
        }
        m_progressBar->setValue(std::clamp(static_cast<int>(std::lround(m_progDisplayPct * 10.0)), 0, 1000));
        int shownPct = std::clamp(static_cast<int>(std::floor(m_progDisplayPct + 0.2)), 0, 100);
        m_progPctLabel->setText(m_jobPaused ? QString("Paused · %1%").arg(shownPct) : QString("%1%").arg(shownPct));
    }
    const bool showIoMeta = (m_activeJob == JobLoad || m_activeJob == JobExport);
    if (m_progDeviceBadge) {
        m_progDeviceBadge->setVisible(showControls);
    }
    if (m_progMetaLabel) {
        m_progMetaLabel->setVisible(showControls || showIoMeta);
        if (showIoMeta) {
            m_progMetaLabel->setText("Elapsed 0.0s · Off-thread I/O (UI unlocked)");
        }
    }
    if (m_progBtnContainer) {
        m_progBtnContainer->setVisible(showControls);
        m_progressOverlay->setFixedSize(468, showControls ? 194 : (showIoMeta ? 152 : 128));
    }
    if (m_progTicker && (showControls || showIoMeta) && !m_progTicker->isActive()) {
        m_progTicker->start();
    }
    updateProgressPosition();
    m_progressOverlay->raise();
    m_progressOverlay->setVisible(true);
    m_progressOverlay->update();
}

void MainWindow::setProgressStage(int basePercent, int nextPercent, const QString& subtitle) {
    if (!m_progressOverlay || !m_progressOverlay->isVisible()) return;
    if (!subtitle.isEmpty()) {
        m_progSubLabel->setText(subtitle);
    }
    m_progBasePct = std::clamp(basePercent, 0, 100);
    m_progNextPct = std::clamp(std::max(nextPercent, m_progBasePct), 0, 100);
    if (m_progBasePct > m_progDisplayPct) {
        m_progDisplayPct = static_cast<double>(m_progBasePct);
    }
    m_progStageTimer.restart();
    updateProgressTelemetry();
}

void MainWindow::setProgressBackend(const QString& backendLabel) {
    if (!m_progDeviceBadge) return;
    m_progBackendText = backendLabel;
    m_progDeviceBadge->setText(backendLabel);
    const bool isGpu = backendLabel.contains("CUDA", Qt::CaseInsensitive)
        || backendLabel.contains("GPU", Qt::CaseInsensitive)
        || backendLabel.contains("RTX", Qt::CaseInsensitive);
    if (isGpu) {
        m_progDeviceBadge->setStyleSheet(
            "color: #9be7c4; font-size: 10px; font-weight: 700; "
            "background-color: rgba(46, 204, 113, 28); border: 1px solid rgba(82, 230, 152, 95); "
            "border-radius: 8px; padding: 2px 8px;"
        );
    } else {
        m_progDeviceBadge->setStyleSheet(
            "color: #f3c87d; font-size: 10px; font-weight: 700; "
            "background-color: rgba(240, 173, 78, 26); border: 1px solid rgba(240, 173, 78, 90); "
            "border-radius: 8px; padding: 2px 8px;"
        );
    }
}

void MainWindow::updateProgressTelemetry() {
    if (!m_progressOverlay || !m_progressOverlay->isVisible() || m_activeJob == JobNone) return;

    qint64 totalElapsedMs = m_progAccumulatedMs;
    if (!m_jobPaused && m_progActiveSegmentTimer.isValid()) {
        totalElapsedMs += m_progActiveSegmentTimer.elapsed();
    }

    if (!m_jobPaused && m_progNextPct > m_progBasePct) {
        // Asymptotic intra-pass progress creep inside [basePct, nextPct - 0.4] so progress visibly streams live
        double stageSec = m_progStageTimer.isValid() ? (m_progStageTimer.elapsed() / 1000.0) : 0.0;
        double tau = m_progBackendText.contains("CUDA", Qt::CaseInsensitive) ? 1.6 : 8.5;
        double frac = 1.0 - std::exp(-stageSec / tau);
        double span = static_cast<double>(m_progNextPct - m_progBasePct);
        double targetCap = std::max(static_cast<double>(m_progBasePct), m_progNextPct - 0.4);
        double smoothTarget = std::min(targetCap, m_progBasePct + span * 0.92 * frac);
        if (smoothTarget > m_progDisplayPct) {
            m_progDisplayPct = smoothTarget;
        }
    }

    if (m_progBasePct >= 100) {
        m_progDisplayPct = 100.0;
    }

    double elapsedSec = totalElapsedMs / 1000.0;
    if (m_activeJob == JobLoad || m_activeJob == JobExport) {
        m_progPctLabel->setText("Working");
        m_progMetaLabel->setText(QString("Elapsed %1s · Off-thread I/O (UI unlocked)").arg(elapsedSec, 0, 'f', 1));
        return;
    }

    if (m_progressBar->maximum() == 1000) {
        m_progressBar->setValue(std::clamp(static_cast<int>(std::lround(m_progDisplayPct * 10.0)), 0, 1000));
    }
    int shownPct = std::clamp(static_cast<int>(std::floor(m_progDisplayPct + 0.15)), 0, 100);
    m_progPctLabel->setText(m_jobPaused ? QString("Paused · %1%").arg(shownPct) : QString("%1%").arg(shownPct));

    QString metaText = QString("Elapsed %1s").arg(elapsedSec, 0, 'f', 1);
    if (m_jobPaused) {
        metaText += " · Paused";
    } else if (m_progDisplayPct >= 6.0 && m_progDisplayPct < 99.0 && elapsedSec >= 0.6) {
        double etaSec = elapsedSec * (100.0 - m_progDisplayPct) / m_progDisplayPct;
        if (etaSec < 60.0) {
            metaText += QString(" · ~%1s left").arg(std::max(1, static_cast<int>(std::lround(etaSec))));
        } else {
            int mins = static_cast<int>(etaSec) / 60;
            int secs = static_cast<int>(etaSec) % 60;
            metaText += QString(" · ~%1m %2s left").arg(mins).arg(secs);
        }
    }
    if (!m_progBackendText.isEmpty()) {
        metaText += QString(" · %1").arg(m_progBackendText);
    }
    m_progMetaLabel->setText(metaText);
}

void MainWindow::togglePauseActiveJob() {
    if (m_activeJob == JobNone) return;
    m_jobPaused = !m_jobPaused;

    if (m_jobPaused) {
        if (m_progActiveSegmentTimer.isValid()) {
            m_progAccumulatedMs += m_progActiveSegmentTimer.elapsed();
        }
    } else {
        m_progActiveSegmentTimer.restart();
        m_progStageTimer.restart();
    }

    if (m_activeJob == JobRemover && m_remover) {
        m_remover->setPaused(m_jobPaused);
    } else if (m_activeJob == JobUpscaler && m_upscaler) {
        m_upscaler->setPaused(m_jobPaused);
    }

    if (m_jobPaused) {
        m_progPauseBtn->setText("Resume");
        m_progPauseBtn->setIconName("play");
        m_progPauseBtn->setVariant(GlassButton::White);
        m_progSubLabel->setText("Pausing after active step — Click Resume to continue");
    } else {
        m_progPauseBtn->setText("Pause");
        m_progPauseBtn->setIconName("pause");
        m_progPauseBtn->setVariant(GlassButton::Ghost);
        m_progSubLabel->setText("Resuming neural processing...");
    }
    updateProgressTelemetry();
    m_progressOverlay->update();
}

void MainWindow::cancelActiveJob() {
    if (m_activeJob == JobNone) return;
    ActiveJob jobType = m_activeJob;
    m_activeJob = JobNone;
    m_jobPaused = false;
    ++m_activeJobToken; // Ignore any late completion signal while ORT unwinds

    if (jobType == JobRemover && m_remover) {
        m_remover->requestCancel(); // Also invokes Ort::RunOptions::SetTerminate() immediately
        disconnect(m_remover, nullptr, this, nullptr);
    } else if (jobType == JobUpscaler && m_upscaler) {
        m_upscaler->requestCancel();
        disconnect(m_upscaler, nullptr, this, nullptr);
    }

    showGlassProgress(false);
    showToast(jobType == JobRemover ? "G4 AI removal cancelled" : "AI Upscale cancelled", 2400);
}

void MainWindow::toggleInfraredMode() {
    if (!m_canvas || !m_bgGroup) return;
    int nextBg;
    if (m_canvas->backgroundType() == CanvasWidget::Infrared) {
        nextBg = m_lastNonInfraredBg;
    } else {
        m_lastNonInfraredBg = static_cast<int>(m_canvas->backgroundType());
        nextBg = CanvasWidget::Infrared;
    }
    if (QAbstractButton* btn = m_bgGroup->button(nextBg)) {
        btn->setChecked(true);
    }
    onBackgroundChanged(nextBg);
}

void MainWindow::cleanInfraredDust() {
    if (!m_processor || !m_processor->hasImage() || !m_canvas) {
        showToast("Open an image first", 1800);
        return;
    }
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }

    const int removed = m_canvas->cleanInfraredDustPixels();
    if (removed > 0) {
        updateQueueUI();
        updateStatusBar();
        showToast(QString("Cleaned %1 infrared dust pixel%2 (Ctrl+Z to undo)")
            .arg(removed).arg(removed == 1 ? "" : "s"), 3000);
    } else {
        showToast("No infrared dust pixels found — mask is spotless!", 2400);
    }
}

void MainWindow::showToast(const QString& message, int durationMs) {
    m_toastText = message;
    updateStatusBar();
    m_toastTimer->start(durationMs);
}

void MainWindow::applyThemeVisuals(const GlassTheme& theme) {
    if (m_themeNameLabel) {
        m_themeNameLabel->setText(theme.name);
    }
    if (m_progPctLabel) {
        m_progPctLabel->setStyleSheet(QString(
            "color: %1; font-size: 12px; font-weight: 700; "
            "background-color: rgba(%2,%3,%4,%5); border: 1px solid rgba(%6,%7,%8,%9); "
            "border-radius: 10px; padding: 2px 10px;"
        ).arg(theme.badgeText.name())
         .arg(theme.badgeBg.red()).arg(theme.badgeBg.green()).arg(theme.badgeBg.blue()).arg(theme.badgeBg.alpha())
         .arg(theme.badgeBorder.red()).arg(theme.badgeBorder.green()).arg(theme.badgeBorder.blue()).arg(theme.badgeBorder.alpha()));
    }
    if (m_progressBar) {
        m_progressBar->setStyleSheet(QString(
            "QProgressBar { background-color: rgba(255,255,255,26); border: none; border-radius: 4px; }"
            "QProgressBar::chunk { background-color: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 %1, stop:1 %2); border-radius: 4px; }"
        ).arg(theme.accentStart.name(), theme.accentEnd.name()));
    }
    showToast(QString("Theme: %1").arg(theme.name), 1600);
    update();
}

void MainWindow::setupShortcuts() {
    QShortcut* sA = new QShortcut(QKeySequence("A"), this);
    connect(sA, &QShortcut::activated, this, [this]() {
        m_toolManager->setCurrentTool(ToolManager::AutoColor);
        m_toolTabBar->setCurrentId(ToolManager::AutoColor, true);
    });

    QShortcut* sE = new QShortcut(QKeySequence("E"), this);
    connect(sE, &QShortcut::activated, this, [this]() {
        m_toolManager->setCurrentTool(ToolManager::ManualErase);
        m_toolTabBar->setCurrentId(ToolManager::ManualErase, true);
    });

    QShortcut* sR = new QShortcut(QKeySequence("R"), this);
    connect(sR, &QShortcut::activated, this, [this]() {
        m_toolManager->setCurrentTool(ToolManager::Repair);
        m_toolTabBar->setCurrentId(ToolManager::Repair, true);
    });

    QShortcut* sBrushDown = new QShortcut(QKeySequence("["), this);
    connect(sBrushDown, &QShortcut::activated, this, [this]() {
        int newSize = qMax(1, m_toolManager->brushSize() - 5);
        m_toolManager->setBrushSize(newSize);
        m_brushSizeSlider->setValue(newSize);
    });

    QShortcut* sBrushUp = new QShortcut(QKeySequence("]"), this);
    connect(sBrushUp, &QShortcut::activated, this, [this]() {
        int newSize = qMin(200, m_toolManager->brushSize() + 5);
        m_toolManager->setBrushSize(newSize);
        m_brushSizeSlider->setValue(newSize);
    });

    QShortcut* sZoomIn = new QShortcut(QKeySequence("="), this);
    connect(sZoomIn, &QShortcut::activated, this, &MainWindow::zoomIn);

    QShortcut* sZoomOut = new QShortcut(QKeySequence("-"), this);
    connect(sZoomOut, &QShortcut::activated, this, &MainWindow::zoomOut);
}

void MainWindow::connectSignals() {
    connect(m_canvas, &CanvasWidget::openRequested, this, &MainWindow::openFile);
    connect(m_toolTabBar, &LiquidGlassTabBar::tabSelected, this, &MainWindow::onToolChanged);
    connect(m_bgGroup, &QButtonGroup::idClicked, this, &MainWindow::onBackgroundChanged);

    connect(m_toleranceSlider, &GlassSliderControl::valueChanged, this, [this](int v) {
        m_lastActiveSlider = m_toleranceSlider;
        m_toolManager->setTolerance(v);
    });

    connect(m_brushSizeSlider, &GlassSliderControl::valueChanged, this, [this](int v) {
        m_lastActiveSlider = m_brushSizeSlider;
        m_toolManager->setBrushSize(v);
    });

    connect(m_softeningSlider, &GlassSliderControl::valueChanged, this, [this](int v) {
        m_lastActiveSlider = m_softeningSlider;
        m_canvas->setEdgeSoftening(v);
        showToast(v > 0 ? QString("Edge softening: %1").arg(v) : "Edge softening off", 1400);
    });

    connect(m_compareSwitch, &QAbstractButton::toggled, this, [this](bool checked) {
        if (m_isComparing != checked) {
            m_isComparing = checked;
            m_canvas->setShowOriginal(checked);
            m_compareAction->setChecked(checked);
        }
    });

    connect(m_compareOpacitySlider, &GlassSliderControl::valueChanged, this, [this](int v) {
        m_lastActiveSlider = m_compareOpacitySlider;
        m_canvas->setCompareOpacity(v / 100.0);
    });

    connect(m_canvas, &CanvasWidget::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(m_canvas, &CanvasWidget::cursorPositionChanged, this, [this](int x, int y) {
        if (m_coordThrottle.isValid() && m_coordThrottle.elapsed() < 40) return;
        m_coordThrottle.start();
        m_statusCoordLabel->setText(QString("X: %1   Y: %2").arg(x).arg(y));
    });
    connect(m_canvas, &CanvasWidget::imageModified, this, &MainWindow::updateStatusBar);
    connect(m_canvas, &CanvasWidget::infraredStatsChanged, this, [this](int, int) { updateStatusBar(); });
    connect(m_historyManager, &HistoryManager::historyChanged, this, &MainWindow::updateStatusBar);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_activeJob == JobExport) {
        showToast("Finishing PNG export to disk, please wait a moment...", 2200);
        event->ignore();
        return;
    }
    if (confirmSaveBeforeClose()) {
        if (m_remover) m_remover->requestCancel();
        if (m_upscaler) m_upscaler->requestCancel();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QStringList rawPaths;
        for (const QUrl& url : mimeData->urls()) {
            const QString local = url.toLocalFile();
            if (!local.isEmpty()) rawPaths.append(local);
        }
        const QStringList images = collectSupportedImagePaths(rawPaths);
        if (!images.isEmpty()) {
            enqueueImagePaths(images, true);
        }
    }
}

void MainWindow::clearCanvasOnly() {
    m_processor->clear();
    m_historyManager->clear();
    m_canvas->updateDisplay();
    m_currentFilePath.clear();
    m_titleFileLabel->setVisible(false);
    setWindowTitle("PixelEraser Pro");
    m_statusCoordLabel->setText("X: —   Y: —");
    updateStatusBar();
}

void MainWindow::updateQueueUI() {
    const int count = m_imageQueue.size();
    const bool showMulti = (count > 1);
    if (m_queueNavWrap) {
        m_queueNavWrap->setVisible(showMulti);
    }
    if (m_queueCountLabel && showMulti) {
        m_queueCountLabel->setText(QString("%1 / %2").arg(m_currentQueueIndex + 1).arg(count));
    }
    if (m_prevImgBtn) m_prevImgBtn->setEnabled(m_currentQueueIndex > 0);
    if (m_nextImgBtn) m_nextImgBtn->setEnabled(m_currentQueueIndex >= 0 && m_currentQueueIndex + 1 < count);

    if (m_queueSection) {
        m_queueSection->setVisible(showMulti);
    }
    if (m_queueHeaderLabel) {
        m_queueHeaderLabel->setText(QString("IMAGE QUEUE (%1)").arg(count));
    }
    if (m_queueList && showMulti) {
        m_queueList->blockSignals(true);
        m_queueList->clear();
        for (int i = 0; i < count; ++i) {
            const ImageQueueItem& item = m_imageQueue[i];
            const bool isActive = (i == m_currentQueueIndex);
            const bool dirty = item.hasUnsavedEdits || (isActive && m_historyManager && m_historyManager->canUndo());
            const QString label = QString("%1. %2%3")
                .arg(i + 1)
                .arg(QFileInfo(item.sourcePath).fileName())
                .arg(dirty ? "  ●" : "");
            auto* lwItem = new QListWidgetItem();
            lwItem->setSizeHint(QSize(0, 28));
            lwItem->setToolTip(item.sourcePath);
            m_queueList->addItem(lwItem);

            QWidget* rowWidget = new QWidget();
            rowWidget->setStyleSheet("background: transparent;");
            QHBoxLayout* rowLay = new QHBoxLayout(rowWidget);
            rowLay->setContentsMargins(6, 1, 4, 1);
            rowLay->setSpacing(4);

            QLabel* nameLbl = new QLabel(label);
            nameLbl->setStyleSheet(isActive
                ? "color: #ffffff; font-size: 11px; font-weight: 700; background: transparent;"
                : "color: #cfd5e0; font-size: 11px; font-weight: 600; background: transparent;");
            nameLbl->setToolTip(item.sourcePath);
            nameLbl->setMinimumWidth(0);

            QPushButton* delBtn = new QPushButton("×");
            delBtn->setFixedSize(20, 20);
            delBtn->setFocusPolicy(Qt::NoFocus);
            delBtn->setCursor(Qt::PointingHandCursor);
            delBtn->setToolTip("Remove from Queue");
            delBtn->setStyleSheet(
                "QPushButton { background: transparent; border: none; border-radius: 6px; "
                "  color: rgba(210, 216, 228, 145); font-size: 13px; font-weight: 700; padding: 0px; }"
                "QPushButton:hover { background-color: rgba(235, 72, 88, 175); color: #ffffff; }"
            );
            const quint64 itemId = item.id;
            connect(delBtn, &QPushButton::clicked, this, [this, itemId]() {
                for (int idx = 0; idx < m_imageQueue.size(); ++idx) {
                    if (m_imageQueue[idx].id == itemId) {
                        removeQueueIndex(idx, false);
                        break;
                    }
                }
            });

            rowLay->addWidget(nameLbl, 1);
            rowLay->addWidget(delBtn, 0);
            m_queueList->setItemWidget(lwItem, rowWidget);
        }
        if (m_currentQueueIndex >= 0 && m_currentQueueIndex < count) {
            m_queueList->setCurrentRow(m_currentQueueIndex);
            m_queueList->scrollToItem(m_queueList->item(m_currentQueueIndex));
        }
        m_queueList->blockSignals(false);
    }
}

void MainWindow::enqueueImagePaths(const QStringList& paths, bool switchToFirstNew) {
    if (paths.isEmpty()) return;

    QSet<QString> existing;
    for (const ImageQueueItem& it : m_imageQueue) {
        existing.insert(QFileInfo(it.sourcePath).absoluteFilePath().toLower());
    }

    int firstNewIndex = -1;
    int addedCount = 0;
    for (const QString& p : paths) {
        const QString absPath = QFileInfo(p).absoluteFilePath();
        const QString key = absPath.toLower();
        if (existing.contains(key)) continue;
        existing.insert(key);

        ImageQueueItem item;
        item.id = m_nextQueueId++;
        item.sourcePath = absPath;
        if (firstNewIndex < 0) firstNewIndex = m_imageQueue.size();
        m_imageQueue.append(item);
        ++addedCount;
    }

    if (addedCount == 0) {
        // User selected an already-queued file — switch to it if it's a single file
        if (paths.size() == 1) {
            const QString key = QFileInfo(paths.first()).absoluteFilePath().toLower();
            for (int i = 0; i < m_imageQueue.size(); ++i) {
                if (QFileInfo(m_imageQueue[i].sourcePath).absoluteFilePath().toLower() == key) {
                    switchToQueueIndex(i);
                    break;
                }
            }
        } else {
            showToast("Selected images are already in the queue", 2200);
        }
        return;
    }

    updateQueueUI();
    if (addedCount > 1) {
        showToast(QString("Added %1 images to queue (path-only, low RAM)").arg(addedCount), 2800);
    }

    if (switchToFirstNew && firstNewIndex >= 0) {
        if (m_currentQueueIndex < 0) {
            loadQueueItemNow(firstNewIndex);
        } else {
            switchToQueueIndex(firstNewIndex);
        }
    }
}

void MainWindow::switchToQueueIndex(int targetIndex) {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }
    if (targetIndex < 0 || targetIndex >= m_imageQueue.size() || targetIndex == m_currentQueueIndex) {
        return;
    }

    // If current image has unsaved edits, offload it to a temp PNG on disk before freeing RAM
    if (m_currentQueueIndex >= 0 && m_currentQueueIndex < m_imageQueue.size() &&
        m_processor->hasImage() && m_historyManager->canUndo()) {
        ImageQueueItem& curItem = m_imageQueue[m_currentQueueIndex];
        if (!m_sessionTempDir.isValid()) {
            loadQueueItemNow(targetIndex);
            return;
        }
        const QString tempPath = m_sessionTempDir.path() + QString("/q_%1.png").arg(curItem.id);
        cv::Mat curSnap = m_processor->getCurrentImage().clone();
        cv::Mat origSnap = m_processor->getOriginalImage().clone();

        m_activeJob = JobExport;
        m_workerRunning = true;
        showGlassProgress(true, "Caching Edits to Disk", QFileInfo(curItem.sourcePath).fileName(), -1, false);
        QApplication::processEvents();

        const int prevIdx = m_currentQueueIndex;
        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, prevIdx, targetIndex, tempPath]() {
            bool ok = watcher->result();
            watcher->deleteLater();
            m_workerRunning = false;
            m_activeJob = JobNone;
            showGlassProgress(false);

            if (ok && prevIdx >= 0 && prevIdx < m_imageQueue.size()) {
                m_imageQueue[prevIdx].tempEditPath = tempPath;
                m_imageQueue[prevIdx].hasUnsavedEdits = true;
            }
            loadQueueItemNow(targetIndex);
        });

        watcher->setFuture(QtConcurrent::run([curSnap, origSnap, tempPath]() {
            return ImageProcessor::exportImageData(curSnap, origSnap, tempPath, 0);
        }));
        return;
    }

    loadQueueItemNow(targetIndex);
}

void MainWindow::loadQueueItemNow(int index) {
    if (index < 0 || index >= m_imageQueue.size()) return;
    m_currentQueueIndex = index;
    const ImageQueueItem& item = m_imageQueue[index];
    const QString diskPath = (!item.tempEditPath.isEmpty() && QFile::exists(item.tempEditPath))
        ? item.tempEditPath
        : item.sourcePath;
    loadImageFile(diskPath);
}

void MainWindow::loadImageFile(const QString& path) {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }

    const QString displaySourcePath = (m_currentQueueIndex >= 0 && m_currentQueueIndex < m_imageQueue.size())
        ? m_imageQueue[m_currentQueueIndex].sourcePath
        : path;

    m_activeJob = JobLoad;
    m_workerRunning = true;
    showGlassProgress(true, "Loading Image", QFileInfo(displaySourcePath).fileName(), -1, false);
    QApplication::processEvents();

    QFutureWatcher<ImageProcessor::LoadedImageData>* watcher =
        new QFutureWatcher<ImageProcessor::LoadedImageData>(this);

    connect(watcher, &QFutureWatcher<ImageProcessor::LoadedImageData>::finished, this, [this, watcher, path, displaySourcePath]() {
        ImageProcessor::LoadedImageData data = watcher->result();
        watcher->deleteLater();
        m_workerRunning = false;
        m_activeJob = JobNone;
        showGlassProgress(false);

        if (data.ok) {
            cv::Mat initAlpha = std::move(data.initialAlpha);
            QImage disp = std::move(data.displayPremul);
            QImage mip4 = std::move(data.displayMip4);
            m_processor->applyLoadedData(std::move(data));

            m_currentFilePath = displaySourcePath;
            m_historyManager->saveInitialState(initAlpha);
            m_canvas->setLoadedCaches(disp, mip4);
            m_canvas->fitToScreen();
            updateQueueUI();
            updateStatusBar();

            QFileInfo fi(displaySourcePath);
            m_titleFileLabel->setText(fi.fileName());
            m_titleFileLabel->setVisible(true);
            setWindowTitle(QString("PixelEraser Pro — %1").arg(fi.fileName()));

            if (m_processor->hasEmbeddedOriginal()) {
                showToast(QString("Loaded %1 × %2  ·  Self-Memory Active (Repair [R], Compare [H] & Restore Ready)")
                    .arg(m_processor->getWidth()).arg(m_processor->getHeight()), 3400);
            } else {
                showToast(QString("Loaded %1 × %2").arg(m_processor->getWidth()).arg(m_processor->getHeight()), 2200);
            }
            m_canvas->setFocus();
        } else {
            GlassDialog::error(this, "Failed to Load Image", "Could not open the selected image file.", path);
        }
    });

    watcher->setFuture(QtConcurrent::run([path]() {
        return ImageProcessor::decodeImageFile(path, true);
    }));
}

void MainWindow::openFile() {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }

    const QStringList filenames = QFileDialog::getOpenFileNames(
        this, "Open / Add Images (Multi-Select Supported)", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp *.webp *.tiff *.tif);;All Files (*.*)"
    );

    if (!filenames.isEmpty()) {
        const QStringList valid = collectSupportedImagePaths(filenames);
        if (!valid.isEmpty()) {
            enqueueImagePaths(valid, true);
        }
    }
}

void MainWindow::addImageFolder() {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }

    const QString dir = QFileDialog::getExistingDirectory(this, "Select Folder of Images");
    if (dir.isEmpty()) return;

    const QStringList images = collectSupportedImagePaths(QStringList{dir});
    if (images.isEmpty()) {
        GlassDialog::warning(this, "No Images Found", "The selected folder does not contain supported image files.", dir);
        return;
    }
    enqueueImagePaths(images, true);
}

void MainWindow::previousImage() {
    if (m_currentQueueIndex > 0) {
        switchToQueueIndex(m_currentQueueIndex - 1);
    }
}

void MainWindow::nextImage() {
    if (m_currentQueueIndex >= 0 && m_currentQueueIndex + 1 < m_imageQueue.size()) {
        switchToQueueIndex(m_currentQueueIndex + 1);
    }
}

void MainWindow::removeQueueIndex(int index, bool askConfirm) {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }
    if (index < 0 || index >= m_imageQueue.size()) {
        if (m_processor->hasImage()) {
            clearCanvasOnly();
            showToast("Canvas cleared", 1800);
        }
        return;
    }

    const bool isActive = (index == m_currentQueueIndex);
    const bool hasDirty = m_imageQueue[index].hasUnsavedEdits || (isActive && m_historyManager->canUndo());
    if (askConfirm && hasDirty) {
        bool confirmed = GlassDialog::confirm(
            this,
            "Remove Image?",
            QString("Remove \"%1\" from the workspace?").arg(QFileInfo(m_imageQueue[index].sourcePath).fileName()),
            "Unsaved edits on this image will be discarded (the original file on disk is untouched).",
            "Remove",
            "Cancel",
            true
        );
        if (!confirmed) return;
    }

    if (!m_imageQueue[index].tempEditPath.isEmpty()) {
        QFile::remove(m_imageQueue[index].tempEditPath);
    }
    m_imageQueue.removeAt(index);

    if (m_imageQueue.isEmpty()) {
        m_currentQueueIndex = -1;
        clearCanvasOnly();
        updateQueueUI();
        showToast("Image removed", 1800);
        return;
    }

    if (isActive) {
        const int nextIdx = std::min(index, static_cast<int>(m_imageQueue.size()) - 1);
        m_currentQueueIndex = -1;
        loadQueueItemNow(nextIdx);
        showToast("Removed image · Loaded next", 1800);
    } else {
        if (index < m_currentQueueIndex) {
            --m_currentQueueIndex;
        }
        updateQueueUI();
        showToast("Removed image from queue", 1800);
    }
}

void MainWindow::removeCurrentImage() {
    removeQueueIndex(m_currentQueueIndex, true);
}

void MainWindow::removeAllImages() {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }
    if (m_imageQueue.isEmpty() && !m_processor->hasImage()) return;

    const int total = std::max(static_cast<int>(m_imageQueue.size()), m_processor->hasImage() ? 1 : 0);
    bool confirmed = GlassDialog::confirm(
        this,
        "Remove All Images?",
        QString("Remove all %1 image%2 from PixelEraser Pro?").arg(total).arg(total > 1 ? "s" : ""),
        "All unsaved edits and temp caches will be cleared (your original files on disk remain untouched).",
        "Remove All",
        "Cancel",
        true
    );
    if (!confirmed) return;

    for (const ImageQueueItem& item : m_imageQueue) {
        if (!item.tempEditPath.isEmpty()) {
            QFile::remove(item.tempEditPath);
        }
    }
    m_imageQueue.clear();
    m_currentQueueIndex = -1;
    clearCanvasOnly();
    updateQueueUI();
    showToast("All images removed", 2000);
}

void MainWindow::discardImage() {
    removeCurrentImage();
}

void MainWindow::saveInPlace() {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }
    if (!m_processor->hasImage()) return;

    if (m_currentFilePath.isEmpty()) {
        quickExport();
        return;
    }

    const QFileInfo fi(m_currentFilePath);
    bool confirmed = GlassDialog::confirm(
        this,
        "Overwrite Original File?",
        QString("Save changes directly into \"%1\"?").arg(fi.fileName()),
        QString("This will overwrite the existing file on disk:\n%1\n\nUse Export (Ctrl+E) if you want to save a separate copy instead.").arg(m_currentFilePath),
        "Save & Overwrite",
        "Cancel",
        true
    );
    if (!confirmed) return;

    const QString targetPath = m_currentFilePath;
    const int softening = m_softeningSlider->value();
    cv::Mat curSnap = m_processor->getCurrentImage().clone();
    cv::Mat origSnap = m_processor->getOriginalImage().clone();

    m_activeJob = JobExport;
    m_workerRunning = true;
    showGlassProgress(true, "Saving Original File", QString("Overwriting %1...").arg(fi.fileName()), -1, false);
    QApplication::processEvents();

    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, targetPath]() {
        bool ok = watcher->result();
        watcher->deleteLater();
        m_workerRunning = false;
        m_activeJob = JobNone;
        showGlassProgress(false);

        if (ok) {
            if (m_currentQueueIndex >= 0 && m_currentQueueIndex < m_imageQueue.size()) {
                auto& item = m_imageQueue[m_currentQueueIndex];
                item.hasUnsavedEdits = false;
                if (!item.tempEditPath.isEmpty()) {
                    QFile::remove(item.tempEditPath);
                    item.tempEditPath.clear();
                }
                updateQueueUI();
            }
            showToast(QString("Saved directly to %1").arg(QFileInfo(targetPath).fileName()), 3000);
        } else {
            GlassDialog::error(this, "Save Failed", "Could not overwrite the file on disk.", targetPath);
        }
    });

    watcher->setFuture(QtConcurrent::run([curSnap, origSnap, targetPath, softening]() {
        return ImageProcessor::exportImageData(curSnap, origSnap, targetPath, softening);
    }));
}

void MainWindow::saveAllEditedImages() {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }

    struct SyncTask {
        quint64 itemId = 0;
        QString sourcePath;
        QString tempEditPath;
        bool isActive = false;
    };

    QVector<SyncTask> tasks;
    for (int i = 0; i < m_imageQueue.size(); ++i) {
        const ImageQueueItem& item = m_imageQueue[i];
        const bool isActive = (i == m_currentQueueIndex);
        if (isActive) {
            if (m_processor->hasImage() && (m_historyManager->canUndo() || item.hasUnsavedEdits)) {
                tasks.append({item.id, item.sourcePath, item.tempEditPath, true});
            }
        } else if (item.hasUnsavedEdits && !item.tempEditPath.isEmpty() && QFile::exists(item.tempEditPath)) {
            tasks.append({item.id, item.sourcePath, item.tempEditPath, false});
        }
    }

    if (tasks.isEmpty()) {
        showToast("All queued images are already synced!", 2200);
        return;
    }

    const int n = static_cast<int>(tasks.size());
    const int choice = GlassDialog::confirmThreeWay(
        this,
        "Sync / Save All Edited Images",
        QString("Save changes for %1 edited image%2?").arg(n).arg(n == 1 ? "" : "s"),
        "• Export Copies — saves each edited image using your Export Settings (prefix/suffix/format).\n"
        "• Overwrite Originals — writes changes directly into each original file on disk.",
        "Export Copies",
        "Overwrite Originals",
        "Cancel"
    );
    if (choice == 0) return;

    const bool overwriteOriginals = (choice == 2);
    const ExportSettings exportCfg = SettingsDialog::loadSettings();
    const int softening = m_softeningSlider->value();

    cv::Mat activeCurSnap;
    cv::Mat activeOrigSnap;
    if (m_processor->hasImage()) {
        activeCurSnap = m_processor->getCurrentImage().clone();
        activeOrigSnap = m_processor->getOriginalImage().clone();
    }

    m_activeJob = JobExport;
    m_workerRunning = true;
    showGlassProgress(true,
        overwriteOriginals ? "Syncing Over Originals" : "Syncing Export Copies",
        QString("Saving 0 / %1...").arg(n), 0, false);
    QApplication::processEvents();

    struct SyncResult {
        int saved = 0;
        int failed = 0;
        QVector<quint64> succeededIds;
    };

    QFutureWatcher<SyncResult>* watcher = new QFutureWatcher<SyncResult>(this);
    connect(watcher, &QFutureWatcher<SyncResult>::finished, this, [this, watcher, overwriteOriginals]() {
        const SyncResult res = watcher->result();
        watcher->deleteLater();
        m_workerRunning = false;
        m_activeJob = JobNone;
        showGlassProgress(false);

        QSet<quint64> doneIds(res.succeededIds.begin(), res.succeededIds.end());
        for (ImageQueueItem& item : m_imageQueue) {
            if (doneIds.contains(item.id)) {
                item.hasUnsavedEdits = false;
                if (overwriteOriginals && !item.tempEditPath.isEmpty()) {
                    QFile::remove(item.tempEditPath);
                    item.tempEditPath.clear();
                }
            }
        }
        updateQueueUI();
        updateStatusBar();

        if (res.failed == 0) {
            showToast(QString("Synced %1 image%2 successfully").arg(res.saved).arg(res.saved == 1 ? "" : "s"), 3200);
        } else {
            GlassDialog::warning(this, "Sync Completed with Warnings",
                QString("Saved %1 image(s), failed %2.").arg(res.saved).arg(res.failed),
                "Some destination files may be read-only or locked by another app.");
        }
    });

    watcher->setFuture(QtConcurrent::run([this, tasks, activeCurSnap, activeOrigSnap, overwriteOriginals, exportCfg, softening]() -> SyncResult {
        SyncResult res;
        const int total = static_cast<int>(tasks.size());
        for (int i = 0; i < total; ++i) {
            const SyncTask& t = tasks[i];
            const QString destPath = overwriteOriginals
                ? t.sourcePath
                : SettingsDialog::buildExportPath(t.sourcePath, exportCfg);
            const QString destName = QFileInfo(destPath).fileName();
            const int pct = (i * 100) / std::max(1, total);
            const int nextPct = ((i + 1) * 100) / std::max(1, total);

            QMetaObject::invokeMethod(this, [this, pct, nextPct, i, total, destName]() {
                setProgressStage(pct, nextPct, QString("Syncing %1 / %2 — %3").arg(i + 1).arg(total).arg(destName));
            }, Qt::QueuedConnection);

            bool ok = false;
            if (t.isActive) {
                ok = ImageProcessor::exportImageData(activeCurSnap, activeOrigSnap, destPath, softening);
            } else {
                const QString ext = QFileInfo(destPath).suffix().toLower();
                if (ext == "png" && softening == 0) {
                    // Ultra-fast direct file copy: tempEditPath is already a lossless Self-Remembering RGBA PNG!
                    QFile::remove(destPath);
                    ok = QFile::copy(t.tempEditPath, destPath);
                } else {
                    ImageProcessor::LoadedImageData decoded = ImageProcessor::decodeImageFile(t.tempEditPath, false);
                    if (decoded.ok) {
                        ok = ImageProcessor::exportImageData(decoded.currentImage, decoded.originalImage, destPath, softening);
                    }
                }
            }

            if (ok) {
                ++res.saved;
                res.succeededIds.append(t.itemId);
            } else {
                ++res.failed;
            }
        }
        return res;
    }));
}

void MainWindow::quickExport() {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Busy with active task, please wait a moment", 2000);
        return;
    }
    if (!m_processor->hasImage()) return;

    const ExportSettings exportCfg = SettingsDialog::loadSettings();
    const QString targetExt = SettingsDialog::resolveExtension(m_currentFilePath, exportCfg.formatMode);
    const QString defaultPath = SettingsDialog::buildExportPath(m_currentFilePath, exportCfg);
    const QString filter = SettingsDialog::buildFileDialogFilter(targetExt);

    QString path = QFileDialog::getSaveFileName(this, "Export Image", defaultPath, filter);
    if (!path.isEmpty()) {
        const QString chosenExt = QFileInfo(path).suffix().toLower();
        const bool hasSupportedExt = (chosenExt == "png" || chosenExt == "webp" ||
                                      chosenExt == "jpg" || chosenExt == "jpeg" ||
                                      chosenExt == "bmp" || chosenExt == "tiff" || chosenExt == "tif");
        if (!hasSupportedExt) {
            path += "." + targetExt;
        }

        int softening = m_softeningSlider->value();
        cv::Mat curSnap = m_processor->getCurrentImage().clone();
        cv::Mat origSnap = m_processor->getOriginalImage().clone();

        m_activeJob = JobExport;
        m_workerRunning = true;
        showGlassProgress(true, "Exporting Cutout", QString("Saving %1...").arg(QFileInfo(path).fileName()), -1, false);
        QApplication::processEvents();

        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, path]() {
            bool ok = watcher->result();
            watcher->deleteLater();
            m_workerRunning = false;
            m_activeJob = JobNone;
            showGlassProgress(false);

            if (ok) {
                if (m_currentQueueIndex >= 0 && m_currentQueueIndex < m_imageQueue.size()) {
                    m_imageQueue[m_currentQueueIndex].hasUnsavedEdits = false;
                    updateQueueUI();
                }
                showToast(QString("Exported %1").arg(QFileInfo(path).fileName()), 3000);
                GlassDialog::success(this, "Export Successful", QString("Saved %1 successfully.").arg(QFileInfo(path).fileName()), path);
            } else {
                GlassDialog::error(this, "Export Failed", "Could not write the image file to disk.", path);
            }
        });

        watcher->setFuture(QtConcurrent::run([curSnap, origSnap, path, softening]() {
            return ImageProcessor::exportImageData(curSnap, origSnap, path, softening);
        }));
    }
}

void MainWindow::exportFile() {
    quickExport();
}

void MainWindow::resizeImage() {
    if (!m_processor->hasImage()) {
        GlassDialog::warning(this, "No Image Loaded", "Open or drop an image onto the canvas before resizing.");
        return;
    }

    ResizeDialog dialog(m_processor->getWidth(), m_processor->getHeight(), this);
    if (dialog.exec() == QDialog::Accepted) {
        int newW = dialog.getNewWidth();
        int newH = dialog.getNewHeight();

        if (newW != m_processor->getWidth() || newH != m_processor->getHeight()) {
            m_historyManager->saveStateBeforeChange();
            m_processor->resize(newW, newH);
            m_historyManager->saveState();
            m_canvas->updateDisplay();
            m_canvas->fitToScreen();
            updateStatusBar();
            showToast(QString("Resized to %1 × %2").arg(newW).arg(newH), 2600);
        }
    }
}

void MainWindow::upscaleImage() {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Previous AI pass is still finishing cleanup, please wait a moment", 2200);
        return;
    }
    if (!m_processor->hasImage()) {
        GlassDialog::warning(this, "No Image Loaded", "Open or drop an image onto the canvas before upscaling.");
        return;
    }

    if (m_historyManager->canUndo()) {
        GlassDialog::warning(this, "Fresh Image Required",
            "AI Upscaling works best on unmodified source images.",
            "Open a fresh image (or undo all edits) to run Real-ESRGAN upscaling.");
        return;
    }

    UpscaleDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        Upscaler::Model model = dialog.getSelectedModel();
        int scale = dialog.getScale();
        cv::Mat inputImage = m_processor->getCurrentImage().clone();

        if (!m_upscaler) {
            m_upscaler = new Upscaler(this);
        }
        const quint64 jobToken = beginActiveJob(
            JobUpscaler, "Real-ESRGAN AI Upscaler", "Preparing neural upscaling tiles..."
        );

        Upscaler* upscaler = m_upscaler;
        disconnect(upscaler, nullptr, this, nullptr);
        connect(upscaler, &Upscaler::backendChanged, this, [this, jobToken](const QString& label) {
            if (jobToken == m_activeJobToken && m_activeJob == JobUpscaler) {
                setProgressBackend(label);
            }
        });
        connect(upscaler, &Upscaler::progressStageChanged, this, [this, jobToken](int basePct, int nextPct, const QString& statusText) {
            if (jobToken == m_activeJobToken && m_activeJob == JobUpscaler) {
                setProgressStage(basePct, nextPct, statusText);
            }
        });
        connect(upscaler, &Upscaler::statusChanged, this, [this, jobToken](const QString& statusText) {
            if (jobToken == m_activeJobToken && m_activeJob == JobUpscaler && m_progSubLabel) {
                m_progSubLabel->setText(statusText);
            }
        });

        QFutureWatcher<cv::Mat>* watcher = new QFutureWatcher<cv::Mat>(this);
        connect(watcher, &QFutureWatcher<cv::Mat>::finished, this, [this, watcher, upscaler, scale, jobToken]() {
            cv::Mat result = watcher->result();
            bool wasCancelled = upscaler->isCancelled();
            m_workerRunning = false;
            watcher->deleteLater();

            if (jobToken != m_activeJobToken) {
                return;
            }

            disconnect(upscaler, nullptr, this, nullptr);
            m_activeJob = JobNone;
            m_jobPaused = false;
            showGlassProgress(false);

            if (wasCancelled) {
                showToast("AI Upscale cancelled", 2400);
            } else if (!result.empty()) {
                m_historyManager->saveStateBeforeChange();
                m_processor->getCurrentImage() = result;
                m_processor->updateOriginalImage();
                m_canvas->updateDisplay();
                m_historyManager->saveState();
                m_canvas->fitToScreen();
                updateStatusBar();
                showToast(QString("Upscaled %1× to %2 × %3").arg(scale).arg(result.cols).arg(result.rows), 3200);
            } else {
                GlassDialog::error(this, "Upscale Failed", "Real-ESRGAN could not upscale this image.");
            }
        });

        QFuture<cv::Mat> future = QtConcurrent::run([upscaler, inputImage, model, scale]() {
            return upscaler->upscale(inputImage, model, scale);
        });
        watcher->setFuture(future);
    }
}

void MainWindow::openRemover() {
    if (m_activeJob != JobNone || m_workerRunning) {
        showToast("Previous AI pass is still finishing cleanup, please wait a moment", 2200);
        return;
    }
    if (!m_processor->hasImage()) {
        GlassDialog::warning(this, "No Image Loaded", "Open or drop an image onto the canvas before running G4 Remover.");
        return;
    }

    if (!m_remover) {
        m_remover = new BackgroundRemover(this);
    }

    RemoverDialog dialog(m_processor->getWidth(), m_processor->getHeight(), m_remover, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    BackgroundRemover::Settings settings = dialog.getSettings();
    cv::Mat inputImage = m_processor->getCurrentImage().clone();

    const quint64 jobToken = beginActiveJob(
        JobRemover, "ToonOut G4 AI Remover", "Initializing G4 neural matting engine..."
    );

    BackgroundRemover* remover = m_remover;
    disconnect(remover, nullptr, this, nullptr);
    connect(remover, &BackgroundRemover::backendChanged, this, [this, jobToken](const QString& label) {
        if (jobToken == m_activeJobToken && m_activeJob == JobRemover) {
            setProgressBackend(label);
        }
    });
    connect(remover, &BackgroundRemover::progressStageChanged, this, [this, jobToken](int basePct, int nextPct, const QString& statusText) {
        if (jobToken == m_activeJobToken && m_activeJob == JobRemover) {
            setProgressStage(basePct, nextPct, statusText);
        }
    });

    QFutureWatcher<cv::Mat>* watcher = new QFutureWatcher<cv::Mat>(this);
    connect(watcher, &QFutureWatcher<cv::Mat>::finished, this, [this, watcher, remover, jobToken]() {
        cv::Mat result = watcher->result();
        bool wasCancelled = remover->isCancelled();
        m_workerRunning = false;
        watcher->deleteLater();

        if (jobToken != m_activeJobToken) {
            return; // Already cancelled and dismissed immediately
        }

        disconnect(remover, nullptr, this, nullptr);
        m_activeJob = JobNone;
        m_jobPaused = false;
        showGlassProgress(false);

        if (wasCancelled) {
            showToast("G4 AI removal cancelled", 2400);
        } else if (!result.empty()) {
            m_historyManager->saveStateBeforeChange();
            m_processor->getCurrentImage() = result;
            m_historyManager->saveState();
            m_canvas->updateDisplay();
            updateStatusBar();
            showToast("G4 AI background removal complete", 3200);
        } else {
            GlassDialog::error(this, "G4 Remover Failed", "Could not run ToonOut G4 background removal on this image.");
        }
    });

    QFuture<cv::Mat> future = QtConcurrent::run([remover, inputImage, settings]() {
        return remover->removeBackground(inputImage, settings);
    });
    watcher->setFuture(future);
}

void MainWindow::applyMaskFromFile() {
    if (!m_processor->hasImage()) {
        GlassDialog::warning(this, "No Image Loaded",
            "Open or drop a base image onto the canvas first.",
            "Then choose a grayscale/B&W mask or a transparent cutout image of matching dimensions.");
        return;
    }

    QString maskPath = QFileDialog::getOpenFileName(
        this,
        QString("Select Mask or Transparent Image (%1 × %2)").arg(m_processor->getWidth()).arg(m_processor->getHeight()),
        QString(),
        "Mask / Transparent Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff);;All Files (*.*)"
    );
    if (maskPath.isEmpty()) return;

    m_historyManager->saveStateBeforeChange();
    QSize maskSz;
    auto status = m_processor->applyExternalMask(maskPath, &maskSz);

    if (status == ImageProcessor::MaskErrorDimensionMismatch) {
        GlassDialog::warning(
            this,
            "Dimension Mismatch",
            QString("The selected image (%1 × %2) does not match the canvas dimensions (%3 × %4).")
                .arg(maskSz.width()).arg(maskSz.height())
                .arg(m_processor->getWidth()).arg(m_processor->getHeight()),
            "Both the base image and the mask / transparent reference image must have identical width and height."
        );
        return;
    }

    if (status == ImageProcessor::MaskErrorLoadFailed) {
        GlassDialog::error(this, "Failed to Load Mask", "Could not read the selected mask or image file.", maskPath);
        return;
    }

    m_historyManager->saveState();
    m_canvas->updateDisplay();
    updateStatusBar();
    m_canvas->setFocus();

    if (status == ImageProcessor::MaskSuccessAlpha) {
        showToast(QString("Applied transparent cutout alpha mask (%1 × %2) · Original RGB preserved")
            .arg(maskSz.width()).arg(maskSz.height()), 3400);
    } else if (status == ImageProcessor::MaskSuccessEmbeddedRgb) {
        showToast(QString("Embedded original RGB memory (%1 × %2) into cutout · Repair [R] & Compare [H] ready")
            .arg(maskSz.width()).arg(maskSz.height()), 3600);
    } else {
        showToast(QString("Applied luminance boundary mask (%1 × %2) · Original RGB preserved")
            .arg(maskSz.width()).arg(maskSz.height()), 3400);
    }
}

void MainWindow::restoreFullOriginalImage() {
    if (!m_processor->hasImage()) {
        GlassDialog::warning(this, "No Image Loaded", "Open an image or self-remembering transparent PNG first.");
        return;
    }

    m_historyManager->saveStateBeforeChange();
    m_processor->restoreFullOriginal();
    m_historyManager->saveState();
    m_canvas->updateDisplay();
    updateStatusBar();
    m_canvas->setFocus();
    showToast("Restored full un-removed original image from embedded RGB memory", 3200);
}

void MainWindow::toggleSidebar() {
    m_sidebarVisible = !m_sidebarVisible;
    m_panelToggleBtn->setChecked(m_sidebarVisible);
    if (m_sidebarVisible) {
        m_sidebarPanel->setVisible(true);
    }
    m_sidebarAnim->stop();
    m_sidebarAnim->setStartValue(m_sidebarPanel->width());
    m_sidebarAnim->setEndValue(m_sidebarVisible ? SIDEBAR_WIDTH : 0);
    m_sidebarAnim->start();
}

void MainWindow::openSettings() {
    SettingsDialog dlg(m_currentFilePath, this);
    if (dlg.exec() == QDialog::Accepted) {
        showToast("Export settings saved", 2000);
    }
}

void MainWindow::showShortcuts() {
    GlassDialog::showShortcuts(this, "Keyboard Shortcuts",
        "FILE & QUEUE\n"
        "  Ctrl+O            Open / Add Images (Multi-Select)\n"
        "  Ctrl+S            Save Over Original File (In-Place)\n"
        "  Ctrl+Shift+S      Sync / Save All Edited Queue Images\n"
        "  Ctrl+E            Export Copy\n"
        "  Ctrl+D            Remove Current Image\n"
        "  Ctrl+N            Remove All Images\n"
        "  PageUp / Ctrl+←   Previous Image in Queue\n"
        "  PageDown / Ctrl+→ Next Image in Queue\n"
        "  Ctrl+,            Settings (Export Naming & Format)\n\n"
        "AI STUDIO & IMAGE\n"
        "  Ctrl+Alt+B        G4 AI Background Remover\n"
        "  Ctrl+Alt+M        Apply Mask / Transparent Cutout\n"
        "  Ctrl+Alt+O        Restore Full Original (Self-Memory)\n"
        "  Ctrl+Alt+U        Real-ESRGAN AI Upscaler\n"
        "  Ctrl+Alt+R        Resize Canvas\n\n"
        "EDIT\n"
        "  Ctrl+Z            Undo\n"
        "  Ctrl+Y            Redo\n\n"
        "VIEW & CANVAS\n"
        "  Ctrl++ / =        Zoom In\n"
        "  Ctrl+- / -        Zoom Out\n"
        "  Ctrl+0            Fit to Screen\n"
        "  Ctrl+1            Actual Size (100%)\n"
        "  Tab               Toggle Tools Panel\n"
        "  H                 Compare Original\n"
        "  I                 Infrared Alpha Dust Scan\n"
        "  Shift+I           Clean Infrared Dust Pixels\n"
        "  Space + Drag      Pan Canvas\n\n"
        "TOOLS\n"
        "  A                 Auto Color Remove\n"
        "  E                 Eraser\n"
        "  R                 Repair\n"
        "  [ / ]             Smaller / Larger Brush"
    );
}

void MainWindow::undo() {
    if (m_historyManager->canUndo()) {
        m_historyManager->undo();
        m_canvas->updateDisplay();
        showToast(QString("Undo (%1 left)").arg(m_historyManager->undoSteps()), 1500);
    }
}

void MainWindow::redo() {
    if (m_historyManager->canRedo()) {
        m_historyManager->redo();
        m_canvas->updateDisplay();
        showToast(QString("Redo (%1 left)").arg(m_historyManager->redoSteps()), 1500);
    }
}

void MainWindow::zoomIn() { m_canvas->zoomIn(); }
void MainWindow::zoomOut() { m_canvas->zoomOut(); }
void MainWindow::fitToScreen() { m_canvas->fitToScreen(); }
void MainWindow::zoomToActual() { m_canvas->setZoom(1.0); }

void MainWindow::toggleCompareOriginal() {
    m_isComparing = !m_isComparing;
    m_canvas->setShowOriginal(m_isComparing);
    m_compareAction->setChecked(m_isComparing);
    m_compareSwitch->setChecked(m_isComparing);
}

void MainWindow::onToolChanged(int toolId) {
    auto tool = static_cast<ToolManager::Tool>(toolId);
    m_toolManager->setCurrentTool(tool);
    m_lastActiveSlider = (tool == ToolManager::AutoColor) ? m_toleranceSlider : m_brushSizeSlider;
    m_canvas->setFocus();
}

void MainWindow::onBackgroundChanged(int bgType) {
    auto type = static_cast<CanvasWidget::BackgroundType>(bgType);
    if (type != CanvasWidget::Infrared) {
        m_lastNonInfraredBg = bgType;
    }
    m_canvas->setBackgroundType(type);
    if (m_cleanIrDustBtn) {
        m_cleanIrDustBtn->setVisible(type == CanvasWidget::Infrared);
    }
    m_canvas->setFocus();
    if (type == CanvasWidget::Infrared) {
        if (m_processor->hasImage()) {
            showToast(QString("Infrared Dust Scan · %1 dust chunks · %2 faint ghost px")
                .arg(m_canvas->infraredDustChunks())
                .arg(m_canvas->infraredGhostPixels()), 3000);
        } else {
            showToast("Infrared Dust Scan active (exposes translucent dust & specks)", 2400);
        }
    }
    updateStatusBar();
}

void MainWindow::onZoomChanged(double zoom) {
    m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(zoom * 100)));
    updateStatusBar();
}

void MainWindow::updateStatusBar() {
    const int zoomPct = static_cast<int>(m_canvas->getZoom() * 100);
    const int undoCount = m_historyManager->undoSteps();
    const int redoCount = m_historyManager->redoSteps();

    if (m_processor->hasImage()) {
        m_statusLeftLabel->setText(QString("Zoom: %1%   ·   %2 × %3   ·   Undo %4 · Redo %5")
            .arg(zoomPct)
            .arg(m_processor->getWidth())
            .arg(m_processor->getHeight())
            .arg(undoCount)
            .arg(redoCount));
    } else {
        m_statusLeftLabel->setText(QString("Zoom: %1%   ·   No Image").arg(zoomPct));
    }

    if (!m_toastText.isEmpty()) {
        m_statusToastLabel->setText(QString("  |   %1").arg(m_toastText));
    } else if (m_processor->hasImage() && m_canvas->backgroundType() == CanvasWidget::Infrared) {
        m_statusToastLabel->setText(QString("  |   IR Dust Scan: %1 stray chunks · %2 ghost px")
            .arg(m_canvas->infraredDustChunks())
            .arg(m_canvas->infraredGhostPixels()));
    } else {
        m_statusToastLabel->setText(QString());
    }

    m_undoAction->setEnabled(m_historyManager->canUndo());
    m_redoAction->setEnabled(m_historyManager->canRedo());
    if (m_undoBtn) m_undoBtn->setEnabled(m_historyManager->canUndo());
    if (m_redoBtn) m_redoBtn->setEnabled(m_historyManager->canRedo());
}

void MainWindow::checkForUpdates() {
    showToast("Checking for updates...", 2000);
    m_updateChecker->checkForUpdates(false);
}

void MainWindow::onUpdateAvailable(const UpdateInfo& info) {
    showToast(QString("Update v%1 available!").arg(info.version), 3000);
    UpdateDialog dlg(info, APP_VERSION, this);
    dlg.exec();
}

void MainWindow::onNoUpdateAvailable() {
    showToast("You're up to date", 2500);
    GlassDialog::success(this, "Up to Date",
        QString("You are running the latest version of PixelEraser Pro (v%1).").arg(APP_VERSION));
}

void MainWindow::onUpdateCheckFailed(const QString& error) {
    showToast("Update check failed", 2500);
    GlassDialog::warning(this, "Update Check Failed",
        "Could not reach GitHub to check for updates.",
        error);
}
