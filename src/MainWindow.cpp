#include "MainWindow.h"
#include "CanvasWidget.h"
#include "ImageProcessor.h"
#include "ToolManager.h"
#include "HistoryManager.h"
#include "ExportDialog.h"
#include "ResizeDialog.h"
#include "UpscaleDialog.h"
#include "Upscaler.h"
#include "UpdateChecker.h"
#include "Version.h"

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QShortcut>
#include <QKeySequence>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScrollArea>
#include <QToolButton>
#include <QTimer>
#include <QProgressDialog>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_processor(new ImageProcessor())
    , m_toolManager(new ToolManager(this))
    , m_historyManager(new HistoryManager(this))
    , m_updateChecker(new UpdateChecker(this))
{
    setWindowTitle("PixelEraser Pro");
    setWindowIcon(QIcon(":/icons/app-icon.png"));  // Set window icon explicitly
    resize(1400, 900);
    setMinimumSize(900, 600);
    setAcceptDrops(true);

    m_canvas = new CanvasWidget(this);
    m_canvas->setImageProcessor(m_processor);
    m_canvas->setToolManager(m_toolManager);
    m_canvas->setHistoryManager(m_historyManager);
    setCentralWidget(m_canvas);

    m_historyManager->setImageProcessor(m_processor);

    setupMenuBar();
    setupToolBar();
    setupToolPanel();
    setupStatusBar();
    setupShortcuts();
    connectSignals();
    
    // Setup update checker
    m_updateChecker->setRepository("RohitPoul/PixelEraserPro");
    m_updateChecker->setCurrentVersion(APP_VERSION);
    connect(m_updateChecker, &UpdateChecker::updateAvailable, this, &MainWindow::onUpdateAvailable);
    connect(m_updateChecker, &UpdateChecker::noUpdateAvailable, this, &MainWindow::onNoUpdateAvailable);
    connect(m_updateChecker, &UpdateChecker::checkFailed, this, &MainWindow::onUpdateCheckFailed);
    
    // Check for updates silently on startup
    QTimer::singleShot(3000, this, [this]() {
        m_updateChecker->checkForUpdates(true);
    });
}

MainWindow::~MainWindow() {
    delete m_processor;
}

bool MainWindow::confirmSaveBeforeClose() {
    if (!m_processor->hasImage() || !m_historyManager->canUndo()) {
        return true;
    }
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Unsaved Changes");
    msgBox.setText("You have unsaved changes. Do you want to export before continuing?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes) {
        quickExport();
        return true;
    } else if (ret == QMessageBox::No) {
        return true;
    }
    return false;
}

void MainWindow::setupMenuBar() {
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("File");
    
    fileMenu->addAction("New", this, [this]() {
        if (confirmSaveBeforeClose()) {
            m_processor->clear();
            m_historyManager->clear();
            m_canvas->updateDisplay();
            m_currentFilePath.clear();
            setWindowTitle("PixelEraser Pro");
            updateStatusBar();
        }
    }, QKeySequence("Ctrl+N"));
    
    fileMenu->addAction("Open...", this, &MainWindow::openFile, QKeySequence("Ctrl+O"));
    fileMenu->addAction("Discard Image", this, &MainWindow::discardImage, QKeySequence("Ctrl+D"));
    fileMenu->addSeparator();
    fileMenu->addAction("Export...", this, &MainWindow::quickExport, QKeySequence("Ctrl+E"));
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QMainWindow::close, QKeySequence("Alt+F4"));

    // Edit menu
    QMenu* editMenu = menuBar()->addMenu("Edit");
    
    m_undoAction = editMenu->addAction("Undo", this, &MainWindow::undo, QKeySequence("Ctrl+Z"));
    m_undoAction->setEnabled(false);
    
    m_redoAction = editMenu->addAction("Redo", this, &MainWindow::redo, QKeySequence("Ctrl+Y"));
    m_redoAction->setEnabled(false);
    
    // Alt redo shortcut
    QAction* redoAlt = new QAction(this);
    redoAlt->setShortcut(QKeySequence("Ctrl+Shift+Z"));
    connect(redoAlt, &QAction::triggered, this, &MainWindow::redo);
    addAction(redoAlt);

    // Image menu
    QMenu* imageMenu = menuBar()->addMenu("Image");
    imageMenu->addAction("Resize...", this, &MainWindow::resizeImage, QKeySequence("Ctrl+Alt+R"));
    imageMenu->addAction("Upscale...", this, &MainWindow::upscaleImage, QKeySequence("Ctrl+Alt+U"));

    // View menu
    QMenu* viewMenu = menuBar()->addMenu("View");
    
    viewMenu->addAction("Zoom In", this, &MainWindow::zoomIn, QKeySequence("Ctrl++"));
    viewMenu->addAction("Zoom Out", this, &MainWindow::zoomOut, QKeySequence("Ctrl+-"));
    viewMenu->addAction("Fit to Screen", this, &MainWindow::fitToScreen, QKeySequence("Ctrl+0"));
    viewMenu->addAction("Actual Size (100%)", this, &MainWindow::zoomToActual, QKeySequence("Ctrl+1"));
    viewMenu->addSeparator();
    
    m_compareAction = viewMenu->addAction("Compare Original", this, &MainWindow::toggleCompareOriginal, QKeySequence("H"));
    m_compareAction->setCheckable(true);
    
    viewMenu->addSeparator();
    m_toggleSidebarAction = viewMenu->addAction("Toggle Sidebar", this, &MainWindow::toggleSidebar, QKeySequence("Tab"));
    viewMenu->addAction("Toggle Menu Bar", this, [this]() {
        menuBar()->setVisible(!menuBar()->isVisible());
    }, QKeySequence("Ctrl+M"));

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("Keyboard Shortcuts", this, &MainWindow::showShortcuts, QKeySequence("F1"));
    helpMenu->addAction("Check for Updates...", this, &MainWindow::checkForUpdates);
    helpMenu->addSeparator();
    helpMenu->addAction("About", this, [this]() {
        QMessageBox::about(this, "About PixelEraser Pro",
            QString("PixelEraser Pro v%1\n\n"
            "Professional background removal tool.\n"
            "Built with Qt 6 and OpenCV.").arg(APP_VERSION));
    });
}

void MainWindow::setupToolBar() {
    QToolBar* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    toolbar->addAction("Open", this, &MainWindow::openFile)->setToolTip("Open (Ctrl+O)");
    toolbar->addAction("Discard", this, &MainWindow::discardImage)->setToolTip("Discard Image (Ctrl+D)");
    toolbar->addAction("Export", this, &MainWindow::quickExport)->setToolTip("Export (Ctrl+E)");
    
    toolbar->addSeparator();
    
    QAction* undoBtn = toolbar->addAction("Undo", this, &MainWindow::undo);
    undoBtn->setToolTip("Undo (Ctrl+Z)");
    QAction* redoBtn = toolbar->addAction("Redo", this, &MainWindow::redo);
    redoBtn->setToolTip("Redo (Ctrl+Y)");
    
    toolbar->addSeparator();
    
    toolbar->addAction("Zoom +", this, &MainWindow::zoomIn);
    toolbar->addAction("Zoom -", this, &MainWindow::zoomOut);
    toolbar->addAction("Fit", this, &MainWindow::fitToScreen);
    
    toolbar->addSeparator();
    
    m_toggleSidebarBtn = toolbar->addAction("Panel", this, &MainWindow::toggleSidebar);
    m_toggleSidebarBtn->setToolTip("Toggle Sidebar (Tab)");
    m_toggleSidebarBtn->setCheckable(true);
    m_toggleSidebarBtn->setChecked(true);
}

void MainWindow::setupToolPanel() {
    m_toolDock = new QDockWidget("Tools", this);
    m_toolDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_toolDock->setFixedWidth(240);

    // Scroll area for the panel
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* toolWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(toolWidget);
    layout->setSpacing(16);
    layout->setContentsMargins(12, 12, 12, 12);

    // === TOOLS SECTION ===
    QLabel* toolsHeader = new QLabel("TOOLS");
    toolsHeader->setObjectName("sectionHeader");
    layout->addWidget(toolsHeader);

    m_toolGroup = new QButtonGroup(this);
    
    QPushButton* autoBtn = new QPushButton("Auto Color Remove");
    QPushButton* eraseBtn = new QPushButton("Eraser");
    QPushButton* repairBtn = new QPushButton("Repair");

    autoBtn->setCheckable(true);
    eraseBtn->setCheckable(true);
    repairBtn->setCheckable(true);
    autoBtn->setChecked(true);
    
    autoBtn->setToolTip("Click to remove similar colors (A)");
    eraseBtn->setToolTip("Paint to erase pixels (E)");
    repairBtn->setToolTip("Paint to restore original pixels (R)");

    m_toolGroup->addButton(autoBtn, ToolManager::AutoColor);
    m_toolGroup->addButton(eraseBtn, ToolManager::ManualErase);
    m_toolGroup->addButton(repairBtn, ToolManager::Repair);

    layout->addWidget(autoBtn);
    layout->addWidget(eraseBtn);
    layout->addWidget(repairBtn);

    // === TOLERANCE SECTION ===
    QLabel* toleranceHeader = new QLabel("TOLERANCE");
    toleranceHeader->setObjectName("sectionHeader");
    layout->addWidget(toleranceHeader);
    
    QHBoxLayout* toleranceLayout = new QHBoxLayout();
    m_toleranceSlider = new QSlider(Qt::Horizontal);
    m_toleranceSlider->setRange(0, 255);
    m_toleranceSlider->setValue(50);
    
    m_toleranceSpin = new QSpinBox();
    m_toleranceSpin->setRange(0, 255);
    m_toleranceSpin->setValue(50);
    m_toleranceSpin->setFixedWidth(60);
    
    toleranceLayout->addWidget(m_toleranceSlider);
    toleranceLayout->addWidget(m_toleranceSpin);
    layout->addLayout(toleranceLayout);

    // === BRUSH SIZE SECTION ===
    QLabel* brushHeader = new QLabel("BRUSH SIZE");
    brushHeader->setObjectName("sectionHeader");
    layout->addWidget(brushHeader);
    
    QHBoxLayout* brushLayout = new QHBoxLayout();
    m_brushSizeSlider = new QSlider(Qt::Horizontal);
    m_brushSizeSlider->setRange(1, 200);
    m_brushSizeSlider->setValue(10);
    
    m_brushSizeSpin = new QSpinBox();
    m_brushSizeSpin->setRange(1, 200);
    m_brushSizeSpin->setValue(10);
    m_brushSizeSpin->setSuffix(" px");
    m_brushSizeSpin->setFixedWidth(70);
    
    brushLayout->addWidget(m_brushSizeSlider);
    brushLayout->addWidget(m_brushSizeSpin);
    layout->addLayout(brushLayout);

    // === EDGE SOFTENING SECTION ===
    QLabel* softenHeader = new QLabel("EDGE SOFTENING");
    softenHeader->setObjectName("sectionHeader");
    layout->addWidget(softenHeader);
    
    QHBoxLayout* softenLayout = new QHBoxLayout();
    m_softeningSlider = new QSlider(Qt::Horizontal);
    m_softeningSlider->setRange(0, 5);
    m_softeningSlider->setValue(0);
    
    m_softeningSpin = new QSpinBox();
    m_softeningSpin->setRange(0, 5);
    m_softeningSpin->setValue(0);
    m_softeningSpin->setFixedWidth(50);
    
    softenLayout->addWidget(m_softeningSlider);
    softenLayout->addWidget(m_softeningSpin);
    layout->addLayout(softenLayout);
    
    QLabel* softenHint = new QLabel("Real-time preview");
    softenHint->setStyleSheet("color: #666; font-size: 10px;");
    layout->addWidget(softenHint);

    // === BACKGROUND SECTION ===
    QLabel* bgHeader = new QLabel("PREVIEW BACKGROUND");
    bgHeader->setObjectName("sectionHeader");
    layout->addWidget(bgHeader);

    m_bgGroup = new QButtonGroup(this);
    QRadioButton* darkBg = new QRadioButton("Dark");
    QRadioButton* lightBg = new QRadioButton("Light");
    QRadioButton* amoledBg = new QRadioButton("Black");
    QRadioButton* whiteBg = new QRadioButton("White");

    darkBg->setChecked(true);
    m_bgGroup->addButton(darkBg, CanvasWidget::Dark);
    m_bgGroup->addButton(lightBg, CanvasWidget::Light);
    m_bgGroup->addButton(amoledBg, CanvasWidget::AMOLED);
    m_bgGroup->addButton(whiteBg, CanvasWidget::White);

    QHBoxLayout* bgRow1 = new QHBoxLayout();
    bgRow1->addWidget(darkBg);
    bgRow1->addWidget(lightBg);
    layout->addLayout(bgRow1);
    
    QHBoxLayout* bgRow2 = new QHBoxLayout();
    bgRow2->addWidget(amoledBg);
    bgRow2->addWidget(whiteBg);
    layout->addLayout(bgRow2);

    // === COMPARE SECTION ===
    layout->addSpacing(8);
    QLabel* compareHeader = new QLabel("COMPARE ORIGINAL");
    compareHeader->setObjectName("sectionHeader");
    layout->addWidget(compareHeader);
    
    m_compareBtn = new QPushButton("Press H to Compare");
    m_compareBtn->setToolTip("Press H to view original image");
    layout->addWidget(m_compareBtn);
    
    QHBoxLayout* opacityLayout = new QHBoxLayout();
    QLabel* opacityLabel = new QLabel("Opacity:");
    m_compareOpacitySlider = new QSlider(Qt::Horizontal);
    m_compareOpacitySlider->setRange(0, 100);
    m_compareOpacitySlider->setValue(100);
    QLabel* opacityValueLabel = new QLabel("100%");
    opacityValueLabel->setFixedWidth(40);
    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(m_compareOpacitySlider);
    opacityLayout->addWidget(opacityValueLabel);
    layout->addLayout(opacityLayout);
    
    connect(m_compareOpacitySlider, &QSlider::valueChanged, this, [this, opacityValueLabel](int v) {
        opacityValueLabel->setText(QString("%1%").arg(v));
        m_canvas->setCompareOpacity(v / 100.0);
    });

    layout->addStretch();

    scrollArea->setWidget(toolWidget);
    m_toolDock->setWidget(scrollArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_toolDock);
    
    // Connect sidebar visibility to toolbar button
    connect(m_toolDock, &QDockWidget::visibilityChanged, m_toggleSidebarBtn, &QAction::setChecked);
}

void MainWindow::setupStatusBar() {
    m_zoomLabel = new QLabel("100%");
    m_imageSizeLabel = new QLabel("No image");
    m_positionLabel = new QLabel("");
    m_historyLabel = new QLabel("");
    
    m_progressBar = new QProgressBar();
    m_progressBar->setFixedWidth(200);
    m_progressBar->setFixedHeight(18);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("Processing...");
    m_progressBar->setVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar { background: #1e1e1e; border: 1px solid #3d3d42; border-radius: 3px; color: #ccc; font-size: 11px; }"
        "QProgressBar::chunk { background: #6a9ed4; border-radius: 2px; }"
    );

    statusBar()->addWidget(new QLabel("Zoom:"));
    statusBar()->addWidget(m_zoomLabel);
    statusBar()->addWidget(createSeparator());
    statusBar()->addWidget(m_imageSizeLabel);
    statusBar()->addWidget(createSeparator());
    statusBar()->addWidget(m_historyLabel);
    statusBar()->addWidget(createSeparator());
    statusBar()->addWidget(m_progressBar);
    statusBar()->addPermanentWidget(m_positionLabel);
}

QWidget* MainWindow::createSeparator() {
    QFrame* sep = new QFrame();
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: #3d3d42;");
    return sep;
}

void MainWindow::setupShortcuts() {
    // Tool shortcuts
    QShortcut* sA = new QShortcut(QKeySequence("A"), this);
    connect(sA, &QShortcut::activated, this, [this]() {
        m_toolManager->setCurrentTool(ToolManager::AutoColor);
        m_toolGroup->button(ToolManager::AutoColor)->setChecked(true);
    });
    
    QShortcut* sE = new QShortcut(QKeySequence("E"), this);
    connect(sE, &QShortcut::activated, this, [this]() {
        m_toolManager->setCurrentTool(ToolManager::ManualErase);
        m_toolGroup->button(ToolManager::ManualErase)->setChecked(true);
    });
    
    QShortcut* sR = new QShortcut(QKeySequence("R"), this);
    connect(sR, &QShortcut::activated, this, [this]() {
        m_toolManager->setCurrentTool(ToolManager::Repair);
        m_toolGroup->button(ToolManager::Repair)->setChecked(true);
    });

    // Brush size
    QShortcut* sBrushDown = new QShortcut(QKeySequence("["), this);
    connect(sBrushDown, &QShortcut::activated, this, [this]() {
        int newSize = qMax(1, m_toolManager->brushSize() - 5);
        m_toolManager->setBrushSize(newSize);
        m_brushSizeSlider->setValue(newSize);
        m_brushSizeSpin->setValue(newSize);
    });
    
    QShortcut* sBrushUp = new QShortcut(QKeySequence("]"), this);
    connect(sBrushUp, &QShortcut::activated, this, [this]() {
        int newSize = qMin(200, m_toolManager->brushSize() + 5);
        m_toolManager->setBrushSize(newSize);
        m_brushSizeSlider->setValue(newSize);
        m_brushSizeSpin->setValue(newSize);
    });
    
    // Zoom
    QShortcut* sZoomIn = new QShortcut(QKeySequence("="), this);
    connect(sZoomIn, &QShortcut::activated, this, &MainWindow::zoomIn);
    
    QShortcut* sZoomOut = new QShortcut(QKeySequence("-"), this);
    connect(sZoomOut, &QShortcut::activated, this, &MainWindow::zoomOut);
}

void MainWindow::connectSignals() {
    connect(m_toolGroup, &QButtonGroup::idClicked, this, &MainWindow::onToolChanged);
    
    // Tolerance - sync slider and spinbox
    connect(m_toleranceSlider, &QSlider::valueChanged, this, [this](int v) {
        m_toleranceSpin->setValue(v);
        m_toolManager->setTolerance(v);
    });
    connect(m_toleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_toleranceSlider->setValue(v);
        m_toolManager->setTolerance(v);
    });
    
    // Brush size - sync slider and spinbox
    connect(m_brushSizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_brushSizeSpin->setValue(v);
        m_toolManager->setBrushSize(v);
    });
    connect(m_brushSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_brushSizeSlider->setValue(v);
        m_toolManager->setBrushSize(v);
    });
    
    // Edge softening - sync slider and spinbox with blocking preview
    connect(m_softeningSlider, &QSlider::valueChanged, this, [this](int v) {
        m_softeningSpin->blockSignals(true);
        m_softeningSpin->setValue(v);
        m_softeningSpin->blockSignals(false);
        
        // Disable controls during processing
        m_softeningSlider->setEnabled(false);
        m_softeningSpin->setEnabled(false);
        showProgress(true, "Softening...");
        statusBar()->showMessage("Applying edge softening...");
        QApplication::processEvents();
        
        // Apply effect
        m_canvas->setEdgeSoftening(v);
        
        // Re-enable controls
        m_softeningSlider->setEnabled(true);
        m_softeningSpin->setEnabled(true);
        showProgress(false);
        statusBar()->showMessage(v > 0 ? QString("Edge softening: %1").arg(v) : "Ready", 1500);
    });
    connect(m_softeningSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_softeningSlider->blockSignals(true);
        m_softeningSlider->setValue(v);
        m_softeningSlider->blockSignals(false);
        
        // Disable controls during processing
        m_softeningSlider->setEnabled(false);
        m_softeningSpin->setEnabled(false);
        showProgress(true, "Softening...");
        statusBar()->showMessage("Applying edge softening...");
        QApplication::processEvents();
        
        // Apply effect
        m_canvas->setEdgeSoftening(v);
        
        // Re-enable controls
        m_softeningSlider->setEnabled(true);
        m_softeningSpin->setEnabled(true);
        showProgress(false);
        statusBar()->showMessage(v > 0 ? QString("Edge softening: %1").arg(v) : "Ready", 1500);
    });
    
    connect(m_bgGroup, &QButtonGroup::idClicked, this, &MainWindow::onBackgroundChanged);

    connect(m_canvas, &CanvasWidget::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(m_canvas, &CanvasWidget::cursorPositionChanged, this, [this](int x, int y) {
        m_positionLabel->setText(QString("X: %1  Y: %2").arg(x).arg(y));
    });
    connect(m_canvas, &CanvasWidget::imageModified, this, &MainWindow::updateStatusBar);

    connect(m_historyManager, &HistoryManager::historyChanged, this, &MainWindow::updateStatusBar);
    
    // Compare button - hold to compare
    connect(m_compareBtn, &QPushButton::pressed, this, [this]() {
        if (m_processor->hasImage()) {
            m_canvas->setShowOriginal(true);
        }
    });
    connect(m_compareBtn, &QPushButton::released, this, [this]() {
        m_canvas->setShowOriginal(false);
    });
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmSaveBeforeClose()) {
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
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QString path = mimeData->urls().first().toLocalFile();
        if (!path.isEmpty() && confirmSaveBeforeClose()) {
            loadImageFile(path);
        }
    }
}

void MainWindow::loadImageFile(const QString& path) {
    // Show loading indicator
    showProgress(true, "Loading image...");
    statusBar()->showMessage("Loading image...");
    QApplication::processEvents();
    
    if (m_processor->loadImage(path)) {
        m_currentFilePath = path;
        m_historyManager->saveInitialState();  // Save initial state for undo
        m_canvas->loadImage(path);
        m_canvas->fitToScreen();
        updateStatusBar();
        
        QFileInfo fi(path);
        setWindowTitle(QString("PixelEraser Pro - %1").arg(fi.fileName()));
        
        showProgress(false);
        statusBar()->showMessage(QString("Loaded: %1 x %2").arg(m_processor->getWidth()).arg(m_processor->getHeight()), 3000);
    } else {
        showProgress(false);
        QMessageBox::critical(this, "Error", "Failed to load image:\n" + path);
    }
}

void MainWindow::openFile() {
    if (!confirmSaveBeforeClose()) return;
    
    QString filename = QFileDialog::getOpenFileName(
        this, "Open Image", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp *.webp *.tiff);;All Files (*.*)"
    );

    if (!filename.isEmpty()) {
        loadImageFile(filename);
    }
}

void MainWindow::discardImage() {
    if (!m_processor->hasImage()) {
        return;
    }
    
    // Confirm before discarding to prevent accidental clicks
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Discard Image?");
    msgBox.setText("Are you sure you want to discard the current image?");
    msgBox.setInformativeText("This will permanently remove the image without saving. This action cannot be undone.");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    
    if (msgBox.exec() == QMessageBox::Yes) {
        // Clear everything without asking to save
        m_processor->clear();
        m_historyManager->clear();
        m_canvas->updateDisplay();
        m_currentFilePath.clear();
        setWindowTitle("PixelEraser Pro");
        updateStatusBar();
        statusBar()->showMessage("Image discarded", 2000);
    }
}

void MainWindow::quickExport() {
    if (!m_processor->hasImage()) return;
    
    // Use original filename by default
    QString defaultPath;
    if (!m_currentFilePath.isEmpty()) {
        QFileInfo fi(m_currentFilePath);
        defaultPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".png";
    } else {
        defaultPath = "exported.png";
    }
    
    QString path = QFileDialog::getSaveFileName(this, "Export Image", defaultPath, "PNG (*.png)");
    
    if (!path.isEmpty()) {
        if (!path.endsWith(".png", Qt::CaseInsensitive)) {
            path += ".png";
        }
        
        // Ask if user wants to resize before export
        QMessageBox resizeBox(this);
        resizeBox.setWindowTitle("Resize Before Export?");
        resizeBox.setText(QString("Current size: %1 x %2\n\nDo you want to resize before exporting?")
            .arg(m_processor->getWidth()).arg(m_processor->getHeight()));
        resizeBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        resizeBox.setDefaultButton(QMessageBox::No);
        
        if (resizeBox.exec() == QMessageBox::Yes) {
            ResizeDialog resizeDialog(m_processor->getWidth(), m_processor->getHeight(), this);
            if (resizeDialog.exec() != QDialog::Accepted) {
                return; // User cancelled resize
            }
            
            int newW = resizeDialog.getNewWidth();
            int newH = resizeDialog.getNewHeight();
            
            if (newW != m_processor->getWidth() || newH != m_processor->getHeight()) {
                m_historyManager->saveStateBeforeChange();
                m_processor->resize(newW, newH);
                m_historyManager->saveState();
                m_canvas->updateDisplay();
                updateStatusBar();
            }
        }
        
        // Show progress
        showProgress(true, "Exporting...");
        statusBar()->showMessage("Exporting...");
        QApplication::processEvents();
        
        // Get softening value from slider
        int softening = m_softeningSlider->value();
        
        if (m_processor->exportImage(path, softening)) {
            showProgress(false);
            QMessageBox::information(this, "Export Successful", 
                QString("Image exported successfully!\n\n%1").arg(path));
            statusBar()->showMessage("Export complete", 3000);
        } else {
            showProgress(false);
            QMessageBox::critical(this, "Error", "Failed to export image.");
        }
    }
}

void MainWindow::exportFile() {
    // Just use quickExport - no need for separate dialog
    quickExport();
}

void MainWindow::resizeImage() {
    if (!m_processor->hasImage()) {
        QMessageBox::warning(this, "Resize", "No image loaded.");
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
            statusBar()->showMessage(QString("Resized to %1 x %2").arg(newW).arg(newH), 3000);
        }
    }
}

void MainWindow::upscaleImage() {
    if (!m_processor->hasImage()) {
        QMessageBox::warning(this, "Upscale", "No image loaded.");
        return;
    }
    
    // Check if image has been modified (has transparency changes)
    if (m_historyManager->canUndo()) {
        QMessageBox::warning(this, "Upscale", 
            "Upscaling is only available for unmodified images.\n"
            "Please open a fresh image to use this feature.");
        return;
    }
    
    UpscaleDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        Upscaler::Model model = dialog.getSelectedModel();
        int scale = dialog.getScale();
        
        // Get input image
        cv::Mat inputImage = m_processor->getCurrentImage().clone();
        
        // Create progress dialog
        QProgressDialog* progressDialog = new QProgressDialog(this);
        progressDialog->setWindowTitle("AI Upscaling");
        progressDialog->setLabelText("Processing with Real-ESRGAN AI...\n\nThis may take a moment for large images.");
        progressDialog->setRange(0, 0);  // Indeterminate initially
        progressDialog->setMinimumDuration(0);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->setCancelButton(nullptr);  // Can't cancel mid-upscale
        progressDialog->setMinimumWidth(350);
        progressDialog->show();
        QApplication::processEvents();
        
        // Create upscaler and connect progress signal (use Qt::QueuedConnection for thread safety)
        Upscaler* upscaler = new Upscaler();
        connect(upscaler, &Upscaler::progressChanged, this, [progressDialog](int progress) {
            if (progressDialog->maximum() == 0) {
                progressDialog->setRange(0, 100);  // Switch to determinate
            }
            progressDialog->setValue(progress);
            progressDialog->setLabelText(QString("AI Upscaling: %1% complete\n\nProcessing tiles...").arg(progress));
        });
        
        // Run upscaling in background thread
        QFutureWatcher<cv::Mat>* watcher = new QFutureWatcher<cv::Mat>(this);
        
        connect(watcher, &QFutureWatcher<cv::Mat>::finished, this, [this, watcher, progressDialog, upscaler, scale]() {
            cv::Mat result = watcher->result();
            
            progressDialog->close();
            progressDialog->deleteLater();
            upscaler->deleteLater();
            watcher->deleteLater();
            
            if (!result.empty()) {
                m_historyManager->saveStateBeforeChange();
                
                // Replace current image with upscaled result
                m_processor->getCurrentImage() = result;
                
                // CRITICAL: Update original image AND LAB cache for accurate Auto Color Remove
                m_processor->updateOriginalImage();
                
                // Force complete rebuild of display cache
                m_canvas->updateDisplay();
                
                m_historyManager->saveState();
                m_canvas->fitToScreen();
                updateStatusBar();
                statusBar()->showMessage(QString("Upscaled %1x to %2 x %3")
                    .arg(scale)
                    .arg(result.cols)
                    .arg(result.rows), 3000);
            } else {
                QMessageBox::critical(this, "Error", "Failed to upscale image.");
            }
        });
        
        // Start the upscaling in background
        QFuture<cv::Mat> future = QtConcurrent::run([upscaler, inputImage, model, scale]() {
            return upscaler->upscale(inputImage, model, scale);
        });
        watcher->setFuture(future);
    }
}

void MainWindow::toggleSidebar() {
    m_toolDock->setVisible(!m_toolDock->isVisible());
}

void MainWindow::showShortcuts() {
    QMessageBox::information(this, "Keyboard Shortcuts",
        "FILE\n"
        "  Ctrl+N            New\n"
        "  Ctrl+O            Open\n"
        "  Ctrl+D            Discard Image\n"
        "  Ctrl+S            Save\n"
        "  Ctrl+Shift+S      Save As\n"
        "  Ctrl+E            Quick Export\n"
        "  Ctrl+Shift+E      Export\n\n"
        "EDIT\n"
        "  Ctrl+Z            Undo\n"
        "  Ctrl+Y            Redo\n"
        "  Ctrl+Shift+Z      Redo\n\n"
        "VIEW\n"
        "  Ctrl++            Zoom In\n"
        "  Ctrl+-            Zoom Out\n"
        "  Ctrl+0            Fit to Screen\n"
        "  Ctrl+1            100%\n"
        "  Tab               Toggle Sidebar\n"
        "  H                 Compare Original\n"
        "  Space+Drag        Pan\n\n"
        "TOOLS\n"
        "  A                 Auto Color\n"
        "  E                 Eraser\n"
        "  R                 Repair\n"
        "  [                 Smaller Brush\n"
        "  ]                 Larger Brush"
    );
}

void MainWindow::showProgress(bool show, const QString& message) {
    m_progressBar->setVisible(show);
    if (show) {
        m_progressBar->setRange(0, 0); // Indeterminate
        m_progressBar->setFormat(message.isEmpty() ? "Processing..." : message);
    }
}

void MainWindow::undo() {
    if (m_historyManager->canUndo()) {
        m_historyManager->undo();
        m_canvas->updateDisplay();
        statusBar()->showMessage(QString("Undo (%1 remaining)").arg(m_historyManager->undoSteps()), 1500);
    }
}

void MainWindow::redo() {
    if (m_historyManager->canRedo()) {
        m_historyManager->redo();
        m_canvas->updateDisplay();
        statusBar()->showMessage(QString("Redo (%1 remaining)").arg(m_historyManager->redoSteps()), 1500);
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
}

void MainWindow::onToolChanged(int toolId) {
    m_toolManager->setCurrentTool(static_cast<ToolManager::Tool>(toolId));
}

void MainWindow::onBackgroundChanged(int bgType) {
    m_canvas->setBackgroundType(static_cast<CanvasWidget::BackgroundType>(bgType));
}

void MainWindow::onZoomChanged(double zoom) {
    m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(zoom * 100)));
}

void MainWindow::updateStatusBar() {
    if (m_processor->hasImage()) {
        m_imageSizeLabel->setText(QString("%1 x %2")
            .arg(m_processor->getWidth())
            .arg(m_processor->getHeight()));
    } else {
        m_imageSizeLabel->setText("No image");
    }

    // Undo/Redo counter - show actual available steps
    int undoCount = m_historyManager->undoSteps();
    int redoCount = m_historyManager->redoSteps();
    m_historyLabel->setText(QString("Undo: %1 | Redo: %2").arg(undoCount).arg(redoCount));

    m_undoAction->setEnabled(m_historyManager->canUndo());
    m_redoAction->setEnabled(m_historyManager->canRedo());
}

void MainWindow::checkForUpdates() {
    statusBar()->showMessage("Checking for updates...");
    m_updateChecker->checkForUpdates(false);  // Not silent - show dialogs
}

void MainWindow::onUpdateAvailable(const QString& version, const QString& downloadUrl, const QString& notes) {
    statusBar()->showMessage("Update available!", 3000);
    
    QString message = QString(
        "<h3>A new version is available!</h3>"
        "<p><b>Current version:</b> %1<br>"
        "<b>New version:</b> %2</p>"
    ).arg(APP_VERSION).arg(version);
    
    if (!notes.isEmpty()) {
        // Truncate long release notes
        QString truncatedNotes = notes.left(500);
        if (notes.length() > 500) truncatedNotes += "...";
        message += QString("<p><b>What's new:</b><br>%1</p>").arg(truncatedNotes.toHtmlEscaped().replace("\n", "<br>"));
    }
    
    message += "<p>Would you like to download the update now?</p>";
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Update Available");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    
    if (msgBox.exec() == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl(downloadUrl));
    }
}

void MainWindow::onNoUpdateAvailable() {
    statusBar()->showMessage("You're up to date!", 3000);
    QMessageBox::information(this, "No Updates Available",
        QString("You are running the latest version of PixelEraser Pro (v%1).").arg(APP_VERSION));
}

void MainWindow::onUpdateCheckFailed(const QString& error) {
    statusBar()->showMessage("Update check failed", 3000);
    QMessageBox::warning(this, "Update Check Failed",
        QString("Could not check for updates.\n\nError: %1\n\n"
                "Please check your internet connection and try again.").arg(error));
}
