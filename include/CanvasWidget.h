#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QKeyEvent>
#include <QEnterEvent>
#include <QTimer>
#include <QFutureWatcher>
#include <opencv2/opencv.hpp>

class ImageProcessor;
class ToolManager;
class HistoryManager;

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);
    ~CanvasWidget();

    void setImageProcessor(ImageProcessor* processor);
    void setToolManager(ToolManager* toolManager);
    void setHistoryManager(HistoryManager* historyManager);

    void loadImage(const QString& path);
    void setLoadedCaches(const QImage& displayPremul, const QImage& displayMip4);
    void updateDisplay();

    void zoomIn();
    void zoomOut();
    void fitToScreen();
    void setZoom(double zoom);
    double getZoom() const { return m_zoom; }

    QRect getVisibleImageRect() const;

    // Whole-canvas surfaces: Classic neutral gray Dark checkerboard, complete White, complete Black, and Infrared Alpha Dust Inspector
    enum BackgroundType { Dark, White, Black, Infrared };
    void setBackgroundType(BackgroundType type);
    BackgroundType backgroundType() const { return m_bgType; }

    int infraredDustChunks() const { return m_dustChunkCount; }
    int infraredGhostPixels() const { return m_ghostPixelCount; }
    int cleanInfraredDustPixels();

    void setShowOriginal(bool show);
    bool isShowingOriginal() const { return m_showOriginal; }
    void setCompareOpacity(double opacity);
    void setEdgeSoftening(int level);
    void setSpaceHeld(bool held);

    // Real-time frosted glass backdrop blur renderer for floating GlassPanels
    void paintBlurredBackdrop(QPainter& painter, const QRect& panelRectInCanvas) const;

signals:
    void zoomChanged(double zoom);
    void cursorPositionChanged(int x, int y);
    void imageModified();
    void openRequested();
    void infraredStatsChanged(int dustChunks, int ghostPixels);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void drawCheckerboard(QPainter& painter, const QRect& clipRect);
    void drawImage(QPainter& painter, const QRect& clipRect);
    void drawBrushCursor(QPainter& painter);
    void updateCheckerTile();

    void rebuildFullCache();
    void rebuildMipCache();
    void updateDisplayMipRegion(const QRect& imageRect);
    void ensureBlurredOriginal();
    void rebuildInfraredCache();
    void rebuildInfraredBaseOnly();
    void rebuildInfraredMipRegion(const QRect& imageRect);
    void updateInfraredRegion(const QRect& imageRect);
    void scheduleInfraredHaloScan(int delayMs);
    void startInfraredHaloScan();
    void syncActiveCursor();
    void renderVisibleArea();
    void updateRegion(const QRect& imageRect);
    QRect imageRectToScreen(const QRect& imageRect) const;

    QPoint screenToImage(const QPoint& screenPos) const;
    QPoint imageToScreen(const QPoint& imagePos) const;

    QRect handleAutoColorTool(const QPoint& imagePos);
    void handleEraseTool(const QPoint& imagePos);
    void handleRepairTool(const QPoint& imagePos);

    struct InfraredHaloResult {
        quint64 jobId = 0;
        cv::Mat dustMask;
        cv::Mat dustHalo;
        int dustChunks = 0;
        int ghostCount = 0;
    };

    ImageProcessor* m_processor = nullptr;
    ToolManager* m_toolManager = nullptr;
    HistoryManager* m_historyManager = nullptr;

    QImage m_displayImage;
    QImage m_displayMip4;      // 1/4-scale premultiplied mipmap for instant 120 FPS zoomed-out pan/zoom on 4K-8K images!
    bool m_displayDirty = false;
    bool m_displayMipDirty = false;
    QImage m_originalImage;
    QImage m_softenedImage;
    QImage m_blurredOriginal;  // Cached blurred original for compare
    QImage m_infraredImage;    // Cached Infrared Alpha Dust & Stray Chunk visualization
    QImage m_infraredMip4;     // 1/4-scale max-visibility Infrared mipmap so hover/pan/zoom stay 120 FPS!
    cv::Mat m_irDustMask;      // Cached dust mask from last async scan so brushing never wipes untouched pink dust
    cv::Mat m_irDustHalo;      // Cached crimson radar halo mask from last async scan
    QTimer* m_infraredHaloTimer = nullptr;
    QFutureWatcher<InfraredHaloResult>* m_infraredWatcher = nullptr;
    quint64 m_infraredJobId = 0;
    QPixmap m_checkerTile;     // Cached 2x2 neutral gray checkerboard tile
    QRectF m_openHeroBtnRect;  // Hit target for empty-state "Open Image" pill CTA
    double m_zoom = 1.0;
    QPointF m_panOffset;
    BackgroundType m_bgType = Dark;
    bool m_showOriginal = false;
    double m_compareOpacity = 1.0;
    int m_edgeSoftening = 0;
    int m_dustChunkCount = 0;
    int m_ghostPixelCount = 0;
    QPoint m_lastEmittedImagePos{-1, -1};

    // Large image optimization
    bool m_isLargeImage = false;
    QRect m_renderedRegion;

    bool m_isPanning = false;
    bool m_isDrawing = false;
    bool m_isWheelInteracting = false;
    QTimer* m_wheelSettleTimer = nullptr;
    bool m_spaceHeld = false;
    bool m_mouseInWidget = false;
    QPoint m_lastMousePos;
    QPoint m_lastDrawPos;

    static constexpr double MIN_ZOOM = 0.02;
    static constexpr double MAX_ZOOM = 32.0;
    static constexpr int CHECKER_SIZE = 16;
    static constexpr int LARGE_IMAGE_THRESHOLD = 8300000;
};

#endif // CANVASWIDGET_H
