#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QKeyEvent>
#include <QEnterEvent>
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
    void updateDisplay();

    void zoomIn();
    void zoomOut();
    void fitToScreen();
    void setZoom(double zoom);
    double getZoom() const { return m_zoom; }

    QRect getVisibleImageRect() const;

    enum BackgroundType { Dark, Light, AMOLED, White };
    void setBackgroundType(BackgroundType type);
    
    void setShowOriginal(bool show);
    bool isShowingOriginal() const { return m_showOriginal; }
    void setCompareOpacity(double opacity);
    void setEdgeSoftening(int level);

signals:
    void zoomChanged(double zoom);
    void cursorPositionChanged(int x, int y);
    void imageModified();

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
    
    void rebuildFullCache();
    void renderVisibleArea();
    void updateRegion(const QRect& imageRect);
    QRect imageRectToScreen(const QRect& imageRect) const;

    QPoint screenToImage(const QPoint& screenPos) const;
    QPoint imageToScreen(const QPoint& imagePos) const;

    void handleAutoColorTool(const QPoint& imagePos);
    void handleEraseTool(const QPoint& imagePos);
    void handleRepairTool(const QPoint& imagePos);

    ImageProcessor* m_processor = nullptr;
    ToolManager* m_toolManager = nullptr;
    HistoryManager* m_historyManager = nullptr;

    QImage m_displayImage;
    QImage m_originalImage;
    QImage m_softenedImage;
    double m_zoom = 1.0;
    QPointF m_panOffset;
    BackgroundType m_bgType = Dark;
    bool m_showOriginal = false;
    double m_compareOpacity = 1.0;
    int m_edgeSoftening = 0;
    
    // Large image optimization
    bool m_isLargeImage = false;
    QRect m_renderedRegion;

    bool m_isPanning = false;
    bool m_isDrawing = false;
    bool m_spaceHeld = false;
    bool m_mouseInWidget = false;
    QPoint m_lastMousePos;
    QPoint m_lastDrawPos;

    static constexpr double MIN_ZOOM = 0.02;
    static constexpr double MAX_ZOOM = 32.0;
    static constexpr int CHECKER_SIZE = 16;
    static constexpr int LARGE_IMAGE_THRESHOLD = 8300000; // ~4K (3840x2160 = 8.3MP)
};

#endif // CANVASWIDGET_H
