#include "CanvasWidget.h"
#include "ImageProcessor.h"
#include "ToolManager.h"
#include "HistoryManager.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <opencv2/opencv.hpp>
#include <cmath>

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
}

CanvasWidget::~CanvasWidget() = default;

void CanvasWidget::setImageProcessor(ImageProcessor* processor) {
    m_processor = processor;
}

void CanvasWidget::setToolManager(ToolManager* toolManager) {
    m_toolManager = toolManager;
}

void CanvasWidget::setHistoryManager(HistoryManager* historyManager) {
    m_historyManager = historyManager;
}

void CanvasWidget::loadImage(const QString& path) {
    Q_UNUSED(path);
    rebuildFullCache();
}

void CanvasWidget::updateDisplay() {
    rebuildFullCache();
    
    // For large images, immediately render visible area
    if (m_isLargeImage) {
        renderVisibleArea();
    }
}

void CanvasWidget::rebuildFullCache() {
    if (m_processor && m_processor->hasImage()) {
        int w = m_processor->getWidth();
        int h = m_processor->getHeight();
        
        // Create display image buffer
        m_displayImage = QImage(w, h, QImage::Format_RGBA8888);
        m_displayImage.fill(Qt::transparent);
        
        // Reset rendered region tracking
        m_renderedRegion = QRect();
        
        // Check if large image
        int pixels = w * h;
        m_isLargeImage = (pixels > LARGE_IMAGE_THRESHOLD);
        
        if (!m_isLargeImage) {
            // Small image - render everything now
            m_processor->updateDisplayRegion(m_displayImage, QRect(0, 0, w, h));
            m_renderedRegion = QRect(0, 0, w, h);
        }
        
        m_originalImage = m_processor->getOriginalAsQImage();
        
        // Reapply softening if active
        if (m_edgeSoftening > 0) {
            cv::Mat result = m_processor->applySoftening(m_processor->getCurrentImage(), m_edgeSoftening);
            cv::Mat rgba;
            cv::cvtColor(result, rgba, cv::COLOR_BGRA2RGBA);
            m_softenedImage = QImage(rgba.data, rgba.cols, rgba.rows,
                                     static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
        } else {
            m_softenedImage = QImage();
        }
    } else {
        m_displayImage = QImage();
        m_originalImage = QImage();
        m_softenedImage = QImage();
        m_isLargeImage = false;
        m_renderedRegion = QRect();
    }
    update();
}

void CanvasWidget::setShowOriginal(bool show) {
    m_showOriginal = show;
    update();
}

void CanvasWidget::setCompareOpacity(double opacity) {
    m_compareOpacity = opacity;
    update();
}

void CanvasWidget::setEdgeSoftening(int level) {
    m_edgeSoftening = level;
    
    if (!m_processor || !m_processor->hasImage()) {
        m_softenedImage = QImage();
        update();
        return;
    }
    
    if (level > 0) {
        cv::Mat result = m_processor->applySoftening(m_processor->getCurrentImage(), level);
        cv::Mat rgba;
        cv::cvtColor(result, rgba, cv::COLOR_BGRA2RGBA);
        m_softenedImage = QImage(rgba.data, rgba.cols, rgba.rows,
                                 static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    } else {
        m_softenedImage = QImage();
    }
    
    update();
}

void CanvasWidget::updateRegion(const QRect& imageRect) {
    if (m_displayImage.isNull() || !m_processor) return;
    
    // Direct update of dirty region only
    m_processor->updateDisplayRegion(m_displayImage, imageRect);
    
    // Repaint only affected screen region
    QRect screenRect = imageRectToScreen(imageRect);
    update(screenRect.adjusted(-2, -2, 2, 2));
}

QRect CanvasWidget::imageRectToScreen(const QRect& imageRect) const {
    return QRect(
        static_cast<int>(imageRect.left() * m_zoom + m_panOffset.x()),
        static_cast<int>(imageRect.top() * m_zoom + m_panOffset.y()),
        static_cast<int>(imageRect.width() * m_zoom) + 1,
        static_cast<int>(imageRect.height() * m_zoom) + 1
    );
}

void CanvasWidget::zoomIn() {
    setZoom(m_zoom * 1.25);
}

void CanvasWidget::zoomOut() {
    setZoom(m_zoom / 1.25);
}

void CanvasWidget::fitToScreen() {
    if (!m_processor || !m_processor->hasImage()) return;

    double scaleX = static_cast<double>(width()) / m_processor->getWidth();
    double scaleY = static_cast<double>(height()) / m_processor->getHeight();
    double scale = std::min(scaleX, scaleY) * 0.95;

    m_zoom = std::clamp(scale, MIN_ZOOM, MAX_ZOOM);
    
    double imgW = m_processor->getWidth() * m_zoom;
    double imgH = m_processor->getHeight() * m_zoom;
    m_panOffset = QPointF((width() - imgW) / 2, (height() - imgH) / 2);

    emit zoomChanged(m_zoom);
    
    // Render visible area for large images
    if (m_isLargeImage) {
        renderVisibleArea();
    }
    
    update();
}

void CanvasWidget::setZoom(double zoom) {
    zoom = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
    if (std::abs(zoom - m_zoom) > 0.001) {
        m_zoom = zoom;
        emit zoomChanged(m_zoom);
        
        if (m_isLargeImage) {
            renderVisibleArea();
        }
        
        update();
    }
}

void CanvasWidget::renderVisibleArea() {
    if (!m_processor || !m_processor->hasImage() || m_displayImage.isNull()) return;
    
    QRect visible = getVisibleImageRect();
    if (visible.isEmpty()) return;
    
    // Expand slightly for smooth scrolling
    int margin = 100;
    visible.adjust(-margin, -margin, margin, margin);
    visible = visible.intersected(QRect(0, 0, m_processor->getWidth(), m_processor->getHeight()));
    
    // Only render if this area hasn't been rendered yet
    if (!m_renderedRegion.contains(visible)) {
        m_processor->updateDisplayRegion(m_displayImage, visible);
        m_renderedRegion = m_renderedRegion.united(visible);
    }
}

QRect CanvasWidget::getVisibleImageRect() const {
    if (!m_processor || !m_processor->hasImage()) {
        return QRect();
    }

    QPoint topLeft = screenToImage(QPoint(0, 0));
    QPoint bottomRight = screenToImage(QPoint(width(), height()));

    int x1 = std::max(0, topLeft.x());
    int y1 = std::max(0, topLeft.y());
    int x2 = std::min(m_processor->getWidth(), bottomRight.x());
    int y2 = std::min(m_processor->getHeight(), bottomRight.y());

    return QRect(x1, y1, x2 - x1, y2 - y1);
}

void CanvasWidget::setBackgroundType(BackgroundType type) {
    m_bgType = type;
    update();
}

void CanvasWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    
    // Use fast rendering when panning/zooming, quality when still
    bool fastMode = m_isPanning || m_isDrawing;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, !fastMode && m_zoom < 1.0);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QRect dirtyRect = event->rect();
    
    drawCheckerboard(painter, dirtyRect);

    if (!m_displayImage.isNull()) {
        // For large images, ensure visible area is rendered
        if (m_isLargeImage && !m_isPanning) {
            renderVisibleArea();
        }
        drawImage(painter, dirtyRect);
    }

    // Draw brush cursor
    if (!m_spaceHeld && !m_isPanning && m_mouseInWidget && m_toolManager && 
        m_toolManager->currentTool() != ToolManager::AutoColor) {
        drawBrushCursor(painter);
    }
}

void CanvasWidget::drawCheckerboard(QPainter& painter, const QRect& clipRect) {
    QColor color1, color2;

    switch (m_bgType) {
        case Dark:
            color1 = QColor(42, 42, 46);
            color2 = QColor(54, 54, 58);
            break;
        case Light:
            color1 = QColor(200, 200, 204);
            color2 = QColor(220, 220, 224);
            break;
        case AMOLED:
            color1 = QColor(0, 0, 0);
            color2 = QColor(18, 18, 18);
            break;
        case White:
            painter.fillRect(clipRect, Qt::white);
            return;
    }

    const int size = CHECKER_SIZE;
    int startX = (clipRect.left() / size) * size;
    int startY = (clipRect.top() / size) * size;
    
    for (int y = startY; y <= clipRect.bottom(); y += size) {
        for (int x = startX; x <= clipRect.right(); x += size) {
            bool isDark = ((x / size) + (y / size)) % 2;
            painter.fillRect(x, y, size, size, isDark ? color1 : color2);
        }
    }
}

void CanvasWidget::drawImage(QPainter& painter, const QRect& clipRect) {
    Q_UNUSED(clipRect);
    
    if (m_displayImage.isNull()) return;
    
    QImage* imgToDraw = &m_displayImage;
    if (m_edgeSoftening > 0 && !m_softenedImage.isNull()) {
        imgToDraw = &m_softenedImage;
    }
    
    QRectF targetRect(
        m_panOffset.x(),
        m_panOffset.y(),
        m_processor->getWidth() * m_zoom,
        m_processor->getHeight() * m_zoom
    );
    
    // Always draw the processed image first
    painter.drawImage(targetRect, *imgToDraw);
    
    // If comparing, draw blurred original on top with opacity
    if (m_showOriginal && !m_originalImage.isNull()) {
        // Create blurred version of original for overlay
        QImage blurredOriginal = m_originalImage.scaled(
            m_originalImage.width() / 4, 
            m_originalImage.height() / 4,
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        ).scaled(
            m_originalImage.width(),
            m_originalImage.height(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        
        painter.setOpacity(m_compareOpacity * 0.7);
        painter.drawImage(targetRect, blurredOriginal);
        painter.setOpacity(1.0);
        
        // Show label
        painter.setPen(QColor(255, 255, 255, 200));
        painter.setFont(QFont("Segoe UI", 12, QFont::Bold));
        painter.drawText(rect().adjusted(10, 10, -10, -10), Qt::AlignTop | Qt::AlignHCenter, "COMPARING ORIGINAL");
    }
}

void CanvasWidget::drawBrushCursor(QPainter& painter) {
    if (!m_toolManager) return;

    QPoint mousePos = mapFromGlobal(QCursor::pos());
    int radius = static_cast<int>((m_toolManager->brushSize() / 2.0) * m_zoom);
    radius = std::max(4, radius);

    painter.setRenderHint(QPainter::Antialiasing, true);
    
    bool isEraser = (m_toolManager->currentTool() == ToolManager::ManualErase);
    QColor mainColor = isEraser ? QColor(255, 90, 90) : QColor(90, 160, 255);
    
    // Simple fast cursor - just outline + center
    painter.setPen(QPen(mainColor, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(mousePos, radius, radius);
    
    // Center dot
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(mousePos, 2, 2);
}

QPoint CanvasWidget::screenToImage(const QPoint& screenPos) const {
    return QPoint(
        static_cast<int>((screenPos.x() - m_panOffset.x()) / m_zoom),
        static_cast<int>((screenPos.y() - m_panOffset.y()) / m_zoom)
    );
}

QPoint CanvasWidget::imageToScreen(const QPoint& imagePos) const {
    return QPoint(
        static_cast<int>(imagePos.x() * m_zoom + m_panOffset.x()),
        static_cast<int>(imagePos.y() * m_zoom + m_panOffset.y())
    );
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();

    // Space or H held = pan mode only
    if (m_spaceHeld || m_showOriginal) {
        if (event->button() == Qt::LeftButton) {
            m_isPanning = true;
            setCursor(Qt::ClosedHandCursor);
        }
        return;
    }

    if (event->button() == Qt::MiddleButton || 
        (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        m_isPanning = true;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    
    if (event->button() == Qt::LeftButton && m_processor && m_processor->hasImage()) {
        QPoint imagePos = screenToImage(event->pos());

        if (imagePos.x() >= 0 && imagePos.x() < m_processor->getWidth() &&
            imagePos.y() >= 0 && imagePos.y() < m_processor->getHeight()) {

            if (m_toolManager->currentTool() == ToolManager::AutoColor) {
                // Check if pixel is already transparent - no need to do anything
                QImage img = m_processor->getDisplayImage();
                QRgb pixel = img.pixel(imagePos.x(), imagePos.y());
                if (qAlpha(pixel) == 0) {
                    return; // Already transparent, skip
                }
                
                m_historyManager->saveStateBeforeChange();
                handleAutoColorTool(imagePos);
                m_historyManager->saveState();
                m_renderedRegion = QRect();
                rebuildFullCache();
                if (m_isLargeImage) renderVisibleArea();
                emit imageModified();
            } else {
                m_isDrawing = true;
                m_lastDrawPos = imagePos;
                m_historyManager->saveStateBeforeChange();
                
                int brushSize = m_toolManager->brushSize();
                QRect dirtyRect(imagePos.x() - brushSize, imagePos.y() - brushSize,
                               brushSize * 2, brushSize * 2);

                if (m_toolManager->currentTool() == ToolManager::ManualErase) {
                    m_processor->eraseWithBrush(imagePos.x(), imagePos.y(), brushSize, 
                        m_toolManager->brushHardness());
                } else {
                    m_processor->repairWithBrush(imagePos.x(), imagePos.y(), brushSize);
                }
                
                updateRegion(dirtyRect);
            }
        }
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    QPoint imagePos = screenToImage(event->pos());

    if (m_processor && m_processor->hasImage()) {
        if (imagePos.x() >= 0 && imagePos.x() < m_processor->getWidth() &&
            imagePos.y() >= 0 && imagePos.y() < m_processor->getHeight()) {
            emit cursorPositionChanged(imagePos.x(), imagePos.y());
        }
    }

    // Space or H held = always pan
    if (m_spaceHeld || m_showOriginal) {
        if (m_isPanning) {
            QPoint delta = event->pos() - m_lastMousePos;
            m_panOffset += QPointF(delta);
            m_lastMousePos = event->pos();
            update();
        }
        return;
    }

    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_panOffset += QPointF(delta);
        m_lastMousePos = event->pos();
        update();
        return;
    }
    
    if (m_isDrawing && m_processor && m_processor->hasImage()) {
        int brushSize = m_toolManager->brushSize();
        
        int minX = std::min(m_lastDrawPos.x(), imagePos.x()) - brushSize;
        int minY = std::min(m_lastDrawPos.y(), imagePos.y()) - brushSize;
        int maxX = std::max(m_lastDrawPos.x(), imagePos.x()) + brushSize;
        int maxY = std::max(m_lastDrawPos.y(), imagePos.y()) + brushSize;
        QRect dirtyRect(minX, minY, maxX - minX, maxY - minY);

        if (m_toolManager->currentTool() == ToolManager::ManualErase) {
            m_processor->eraseAlongPath(m_lastDrawPos, imagePos, brushSize, 
                m_toolManager->brushHardness());
        } else {
            m_processor->repairAlongPath(m_lastDrawPos, imagePos, brushSize);
        }
        
        m_lastDrawPos = imagePos;
        updateRegion(dirtyRect);
        return;
    }

    // Cursor update - repaint old and new position
    if (m_toolManager) {
        if (m_toolManager->currentTool() == ToolManager::AutoColor) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::BlankCursor);
            // Repaint both old and new cursor areas
            int r = static_cast<int>(m_toolManager->brushSize() * m_zoom / 2) + 10;
            QRect oldRect(m_lastMousePos.x() - r, m_lastMousePos.y() - r, r * 2, r * 2);
            QRect newRect(event->pos().x() - r, event->pos().y() - r, r * 2, r * 2);
            update(oldRect.united(newRect));
        }
    }
    m_lastMousePos = event->pos();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);

    if (m_isPanning) {
        m_isPanning = false;
        setCursor((m_spaceHeld || m_showOriginal) ? Qt::OpenHandCursor : Qt::ArrowCursor);
        
        if (m_isLargeImage) {
            renderVisibleArea();
            update();
        }
    }

    if (m_isDrawing) {
        m_isDrawing = false;
        // Save state AFTER the brush stroke is complete
        m_historyManager->saveState();
        emit imageModified();
    }
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        QPointF oldImagePos = QPointF(screenToImage(event->position().toPoint()));

        double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        double newZoom = std::clamp(m_zoom * factor, MIN_ZOOM, MAX_ZOOM);
        
        if (std::abs(newZoom - m_zoom) > 0.001) {
            m_zoom = newZoom;
            QPointF newScreenPos = QPointF(imageToScreen(oldImagePos.toPoint()));
            m_panOffset += event->position() - newScreenPos;
            emit zoomChanged(m_zoom);
            
            if (m_isLargeImage) {
                renderVisibleArea();
            }
            
            update();
        }
    } else {
        m_panOffset += QPointF(event->angleDelta()) / 4.0;
        
        if (m_isLargeImage) {
            renderVisibleArea();
        }
        
        update();
    }
}

void CanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    if (m_isLargeImage) {
        renderVisibleArea();
    }
}

void CanvasWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceHeld = true;
        setCursor(Qt::OpenHandCursor);
        update();
    }
    if (event->key() == Qt::Key_H && !event->isAutoRepeat()) {
        m_showOriginal = true;
        setCursor(Qt::OpenHandCursor);
        update();
    }
    QWidget::keyPressEvent(event);
}

void CanvasWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceHeld = false;
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        update();
    }
    if (event->key() == Qt::Key_H && !event->isAutoRepeat()) {
        m_showOriginal = false;
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        update();
    }
    QWidget::keyReleaseEvent(event);
}

void CanvasWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    m_mouseInWidget = true;
    if (m_toolManager && m_toolManager->currentTool() != ToolManager::AutoColor) {
        setCursor(Qt::BlankCursor);
    }
}

void CanvasWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    m_mouseInWidget = false;
    setCursor(Qt::ArrowCursor);
    update();
}

void CanvasWidget::handleAutoColorTool(const QPoint& imagePos) {
    if (!m_processor || !m_toolManager) return;
    QRect visibleRect = getVisibleImageRect();
    m_processor->autoColorRemove(imagePos.x(), imagePos.y(), 
        m_toolManager->tolerance(), visibleRect);
}

void CanvasWidget::handleEraseTool(const QPoint& imagePos) {
    if (!m_processor || !m_toolManager) return;
    m_processor->eraseWithBrush(imagePos.x(), imagePos.y(),
        m_toolManager->brushSize(), m_toolManager->brushHardness());
}

void CanvasWidget::handleRepairTool(const QPoint& imagePos) {
    if (!m_processor || !m_toolManager) return;
    m_processor->repairWithBrush(imagePos.x(), imagePos.y(), m_toolManager->brushSize());
}
