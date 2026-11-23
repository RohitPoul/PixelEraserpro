#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QLabel>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QProgressBar>
#include <QTimer>
#include <QElapsedTimer>
#include <QListWidget>
#include <QTemporaryDir>
#include <QVector>

class CanvasWidget;
class ImageProcessor;
class ToolManager;
class HistoryManager;
class UpdateChecker;
class Upscaler;
class BackgroundRemover;
class GlassPanel;
class GlassButton;
class GlassToggleSwitch;
class GlassSliderControl;
class LiquidGlassTabBar;
class QVariantAnimation;
struct GlassTheme;
struct UpdateInfo;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void addImageFolder();
    void discardImage();
    void removeCurrentImage();
    void removeAllImages();
    void previousImage();
    void nextImage();
    void quickExport();
    void exportFile();
    void resizeImage();
    void upscaleImage();
    void openRemover();
    void applyMaskFromFile();
    void restoreFullOriginalImage();
    void undo();
    void redo();
    void zoomIn();
    void zoomOut();
    void fitToScreen();
    void zoomToActual();
    void toggleCompareOriginal();
    void toggleInfraredMode();
    void toggleSidebar();
    void toggleMaximizeRestore();
    void togglePauseActiveJob();
    void cancelActiveJob();
    void showShortcuts();
    void openSettings();
    void showMainMenu();
    void checkForUpdates();
    void onUpdateAvailable(const UpdateInfo& info);
    void onNoUpdateAvailable();
    void onUpdateCheckFailed(const QString& error);
    void onToolChanged(int toolId);
    void onBackgroundChanged(int bgType);
    void onZoomChanged(double zoom);
    void updateStatusBar();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    enum ActiveJob { JobNone, JobRemover, JobUpscaler, JobLoad, JobExport };

    struct ImageQueueItem {
        quint64 id = 0;
        QString sourcePath;
        QString tempEditPath; // Only written to disk when switching away from a modified image
        bool hasUnsavedEdits = false;
    };

    void applyNativeWindowStyle();
    void setupActions();
    void setupStudioLayout();
    void setupShortcuts();
    void connectSignals();
    void updateProgressPosition();
    void loadImageFile(const QString& path);
    void enqueueImagePaths(const QStringList& paths, bool switchToFirstNew = true);
    void switchToQueueIndex(int targetIndex);
    void loadQueueItemNow(int index);
    void removeQueueIndex(int index, bool askConfirm);
    void clearCanvasOnly();
    void updateQueueUI();
    bool hasAnyUnsavedEdits() const;
    bool confirmSaveBeforeClose();
    void showGlassProgress(bool show, const QString& title = QString(),
                           const QString& subtitle = QString(), int percent = -1,
                           bool showControls = false);
    quint64 beginActiveJob(ActiveJob job, const QString& title, const QString& subtitle);
    void setProgressStage(int basePercent, int nextPercent, const QString& subtitle);
    void setProgressBackend(const QString& backendLabel);
    void updateProgressTelemetry();
    void showToast(const QString& message, int durationMs = 2600);
    void applyThemeVisuals(const GlassTheme& theme);

    CanvasWidget* m_canvas;
    ImageProcessor* m_processor;
    ToolManager* m_toolManager;
    HistoryManager* m_historyManager;
    UpdateChecker* m_updateChecker;
    Upscaler* m_upscaler = nullptr;
    BackgroundRemover* m_remover = nullptr;

    // Docked Liquid Glass Studio Chrome (Siblings of CanvasWidget = ZERO canvas overdraw!)
    GlassPanel* m_topChromeBar = nullptr;
    GlassPanel* m_sidebarPanel = nullptr;
    GlassPanel* m_statusBarPanel = nullptr;
    GlassPanel* m_progressOverlay = nullptr; // Temporary overlay ONLY during AI jobs

    // Top Chrome Bar controls (Unified Titlebar + Toolbar + Window Controls)
    QLabel* m_titleFileLabel = nullptr;
    QWidget* m_queueNavWrap = nullptr;
    GlassButton* m_prevImgBtn = nullptr;
    GlassButton* m_nextImgBtn = nullptr;
    QLabel* m_queueCountLabel = nullptr;
    GlassButton* m_menuBtn = nullptr;
    GlassButton* m_undoBtn = nullptr;
    GlassButton* m_redoBtn = nullptr;
    GlassButton* m_panelToggleBtn = nullptr;
    GlassButton* m_winMaxBtn = nullptr;
    QLabel* m_zoomLabel = nullptr;

    // Left Docked Tools Sidebar (Exact original top-to-bottom workflow!)
    LiquidGlassTabBar* m_toolTabBar = nullptr;
    GlassSliderControl* m_toleranceSlider = nullptr;
    GlassSliderControl* m_brushSizeSlider = nullptr;
    GlassSliderControl* m_softeningSlider = nullptr;
    GlassSliderControl* m_compareOpacitySlider = nullptr;
    GlassSliderControl* m_lastActiveSlider = nullptr;
    GlassToggleSwitch* m_compareSwitch = nullptr;
    QButtonGroup* m_bgGroup = nullptr;
    QLabel* m_themeNameLabel = nullptr;
    QWidget* m_queueSection = nullptr;
    QLabel* m_queueHeaderLabel = nullptr;
    QListWidget* m_queueList = nullptr;
    QVariantAnimation* m_sidebarAnim = nullptr;

    // Bottom Status Strip controls
    QLabel* m_statusLeftLabel = nullptr;
    QLabel* m_statusToastLabel = nullptr;
    QLabel* m_statusCoordLabel = nullptr;
    QString m_toastText;
    QTimer* m_toastTimer = nullptr;
    QElapsedTimer m_coordThrottle;

    // Floating glass progress overlay widgets (with Pause/Resume, Cancel, and real-time telemetry)
    QLabel* m_progTitleLabel = nullptr;
    QLabel* m_progDeviceBadge = nullptr;
    QLabel* m_progSubLabel = nullptr;
    QLabel* m_progMetaLabel = nullptr;
    QLabel* m_progPctLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QWidget* m_progBtnContainer = nullptr;
    GlassButton* m_progPauseBtn = nullptr;
    GlassButton* m_progCancelBtn = nullptr;
    QTimer* m_progTicker = nullptr;
    QElapsedTimer m_progStageTimer;
    QElapsedTimer m_progActiveSegmentTimer;
    qint64 m_progAccumulatedMs = 0;
    int m_progBasePct = 0;
    int m_progNextPct = 0;
    double m_progDisplayPct = 0.0;
    QString m_progBackendText;
    ActiveJob m_activeJob = JobNone;
    bool m_jobPaused = false;
    bool m_workerRunning = false;
    quint64 m_activeJobToken = 0;

    // Global shortcut actions
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_compareAction = nullptr;
    QAction* m_toggleSidebarAction = nullptr;

    bool m_isComparing = false;
    bool m_sidebarVisible = true;
    int m_lastNonInfraredBg = 0;
    QString m_currentFilePath;
    QVector<ImageQueueItem> m_imageQueue;
    int m_currentQueueIndex = -1;
    quint64 m_nextQueueId = 1;
    QTemporaryDir m_sessionTempDir;
};

#endif // MAINWINDOW_H
