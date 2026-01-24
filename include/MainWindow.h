#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>
#include <QCloseEvent>
#include <QProgressBar>

class CanvasWidget;
class ImageProcessor;
class ToolManager;
class HistoryManager;
class UpdateChecker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void discardImage();
    void quickExport();
    void exportFile();
    void resizeImage();
    void upscaleImage();
    void undo();
    void redo();
    void zoomIn();
    void zoomOut();
    void fitToScreen();
    void zoomToActual();
    void toggleCompareOriginal();
    void toggleSidebar();
    void showShortcuts();
    void checkForUpdates();
    void onUpdateAvailable(const QString& version, const QString& downloadUrl, const QString& notes);
    void onNoUpdateAvailable();
    void onUpdateCheckFailed(const QString& error);
    void onToolChanged(int toolId);
    void onBackgroundChanged(int bgType);
    void onZoomChanged(double zoom);
    void updateStatusBar();

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupMenuBar();
    void setupToolBar();
    void setupToolPanel();
    void setupStatusBar();
    void setupShortcuts();
    void connectSignals();
    void loadImageFile(const QString& path);
    bool confirmSaveBeforeClose();
    QWidget* createSeparator();
    void showProgress(bool show, const QString& message = QString());

    CanvasWidget* m_canvas;
    ImageProcessor* m_processor;
    ToolManager* m_toolManager;
    HistoryManager* m_historyManager;
    UpdateChecker* m_updateChecker;

    QDockWidget* m_toolDock;
    QSlider* m_brushSizeSlider;
    QSlider* m_toleranceSlider;
    QSlider* m_softeningSlider;
    QSlider* m_compareOpacitySlider;
    QSpinBox* m_brushSizeSpin;
    QSpinBox* m_toleranceSpin;
    QSpinBox* m_softeningSpin;
    QLabel* m_zoomLabel;
    QLabel* m_imageSizeLabel;
    QLabel* m_positionLabel;
    QLabel* m_historyLabel;
    QProgressBar* m_progressBar;
    QButtonGroup* m_toolGroup;
    QButtonGroup* m_bgGroup;
    QPushButton* m_compareBtn;

    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_compareAction;
    QAction* m_toggleSidebarAction;
    QAction* m_toggleSidebarBtn;
    
    bool m_isComparing = false;
    QString m_currentFilePath;
};

#endif // MAINWINDOW_H
