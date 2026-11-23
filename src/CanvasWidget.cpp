#include "CanvasWidget.h"
#include "ImageProcessor.h"
#include "ToolManager.h"
#include "HistoryManager.h"
#include "GlassWidgets.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtConcurrent>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

constexpr uint32_t kIrHaloColor = (175u << 24) | (175u << 16) | (19u << 8) | 47u;
constexpr uint32_t kIrDustColor = (255u << 24) | (255u << 16) | (20u << 8) | 90u;

const std::array<uint32_t, 256>& infraredAlphaLut() {
    static const std::array<uint32_t, 256> lut = []() {
        std::array<uint32_t, 256> t{};
        t[0] = 0u;
        for (int a = 1; a <= 45; ++a) {
            float f = static_cast<float>(a) / 45.0f;
            uint32_t b = static_cast<uint32_t>(35.0f * (1.0f - f));
            uint32_t g = static_cast<uint32_t>(25.0f + 95.0f * f);
            t[a] = 0xFF000000u | (255u << 16) | (g << 8) | b;
        }
        for (int a = 46; a <= 230; ++a) {
            float f = static_cast<float>(a - 45) / 185.0f;
            uint32_t b = static_cast<uint32_t>(15.0f + 85.0f * f);
            uint32_t g = static_cast<uint32_t>(145.0f + 95.0f * f);
            t[a] = 0xFF000000u | (255u << 16) | (g << 8) | b;
        }
        return t;
    }();
    return lut;
}

// Row-level shading helper avoids both code duplication and per-pixel Debug call overhead
inline void shadeInfraredRect(const cv::Mat& cur, QImage& irImg, const QRect& rect,
                              const cv::Mat* dustMask = nullptr, const cv::Mat* dustHalo = nullptr) {
    const uint32_t* const lut = infraredAlphaLut().data();
    const uchar* const curBase = cur.data;
    const size_t curStep = cur.step;
    uchar* const irBase = irImg.bits();
    const int irStride = irImg.bytesPerLine();
    const int left = rect.left();
    const int right = rect.right();
    const bool hasMasks = dustMask && dustHalo &&
                          dustMask->rows == cur.rows && dustMask->cols == cur.cols &&
                          dustHalo->rows == cur.rows && dustHalo->cols == cur.cols;

    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        const cv::Vec4b* src = reinterpret_cast<const cv::Vec4b*>(curBase + static_cast<size_t>(y) * curStep);
        uint32_t* dst = reinterpret_cast<uint32_t*>(irBase + static_cast<size_t>(y) * irStride);
        const uchar* dRow = hasMasks ? dustMask->ptr<uchar>(y) : nullptr;
        const uchar* hRow = hasMasks ? dustHalo->ptr<uchar>(y) : nullptr;
        for (int x = left; x <= right; ++x) {
            const uchar a = src[x][3];
            if (a == 0) {
                // Keep untouched surrounding halo during strokes; erased dust pixels clear immediately to black
                dst[x] = (hasMasks && hRow[x] && !dRow[x]) ? kIrHaloColor : 0u;
            } else if (hasMasks && dRow[x]) {
                dst[x] = kIrDustColor;
            } else if (a <= 230) {
                dst[x] = lut[a];
            } else {
                const uint32_t lum = (static_cast<uint32_t>(src[x][2]) * 77u +
                                      static_cast<uint32_t>(src[x][1]) * 150u +
                                      static_cast<uint32_t>(src[x][0]) * 29u) >> 8;
                const uint32_t dim = 24u + (lum * 72u) / 255u;
                const uint32_t outB = (dim + 18u <= 255u) ? (dim + 18u) : 255u;
                const uint32_t outG = (dim + 6u <= 255u) ? (dim + 6u) : 255u;
                dst[x] = 0xFF000000u | (dim << 16) | (outG << 8) | outB;
            }
        }
    }
}

} // namespace

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    updateCheckerTile();

    m_wheelSettleTimer = new QTimer(this);
    m_wheelSettleTimer->setSingleShot(true);
    m_wheelSettleTimer->setInterval(130);
    connect(m_wheelSettleTimer, &QTimer::timeout, this, [this]() {
        m_isWheelInteracting = false;
        update();
    });

    m_infraredHaloTimer = new QTimer(this);
    m_infraredHaloTimer->setSingleShot(true);
    connect(m_infraredHaloTimer, &QTimer::timeout, this, &CanvasWidget::startInfraredHaloScan);

    m_infraredWatcher = new QFutureWatcher<InfraredHaloResult>(this);
    connect(m_infraredWatcher, &QFutureWatcher<InfraredHaloResult>::finished, this, [this]() {
        if (!m_infraredWatcher || m_bgType != Infrared || !m_processor || !m_processor->hasImage()) {
            return;
        }
        const InfraredHaloResult res = m_infraredWatcher->result();
        if (res.jobId != m_infraredJobId || m_isDrawing) {
            return;
        }
        const cv::Mat& cur = m_processor->getCurrentImage();
        if (cur.empty() || cur.channels() != 4 ||
            m_infraredImage.width() != cur.cols || m_infraredImage.height() != cur.rows ||
            res.dustMask.rows != cur.rows || res.dustMask.cols != cur.cols) {
            return;
        }

        m_irDustMask = res.dustMask;
        m_irDustHalo = res.dustHalo;

        const int H = cur.rows;
        const int W = cur.cols;
        shadeInfraredRect(cur, m_infraredImage, QRect(0, 0, W, H), &m_irDustMask, &m_irDustHalo);

        rebuildInfraredMipRegion(QRect(0, 0, W, H));
        m_dustChunkCount = res.dustChunks;
        m_ghostPixelCount = res.ghostCount;
        emit infraredStatsChanged(res.dustChunks, res.ghostCount);
        update();
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

CanvasWidget::~CanvasWidget() {
    if (m_infraredHaloTimer) {
        m_infraredHaloTimer->stop();
    }
    ++m_infraredJobId;
    if (m_infraredWatcher && m_infraredWatcher->isRunning()) {
        m_infraredWatcher->waitForFinished();
    }
}

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
    m_originalImage = QImage();
    m_blurredOriginal = QImage();
    rebuildFullCache();
}

void CanvasWidget::setLoadedCaches(const QImage& displayPremul, const QImage& displayMip4) {
    m_originalImage = QImage();
    m_blurredOriginal = QImage();
    m_softenedImage = QImage();

    if (displayPremul.isNull()) {
        rebuildFullCache();
        return;
    }

    m_displayImage = displayPremul;
    m_displayMip4 = displayMip4;
    m_displayDirty = false;
    m_displayMipDirty = false;
    m_renderedRegion = QRect(0, 0, displayPremul.width(), displayPremul.height());
    m_isLargeImage = false;

    if (m_edgeSoftening > 0 && m_bgType != Infrared && m_processor && m_processor->hasImage()) {
        cv::Mat result = m_processor->applySoftening(m_processor->getCurrentImage(), m_edgeSoftening);
        m_softenedImage = QImage(result.data, result.cols, result.rows,
                                 static_cast<int>(result.step), QImage::Format_ARGB32)
                                 .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    if (m_bgType == Infrared) {
        rebuildInfraredBaseOnly();
        scheduleInfraredHaloScan(0);
    } else {
        m_infraredImage = QImage();
        m_infraredMip4 = QImage();
        m_irDustMask.release();
        m_irDustHalo.release();
        ++m_infraredJobId;
    }
    update();
}

void CanvasWidget::updateDisplay() {
    rebuildFullCache();
}

void CanvasWidget::rebuildFullCache() {
    if (m_processor && m_processor->hasImage()) {
        int w = m_processor->getWidth();
        int h = m_processor->getHeight();

        if (m_displayImage.width() != w || m_displayImage.height() != h ||
            m_displayImage.format() != QImage::Format_ARGB32_Premultiplied) {
            m_displayImage = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        }

        m_renderedRegion = QRect(0, 0, w, h);
        m_isLargeImage = false;

        if (!m_blurredOriginal.isNull() &&
            (m_blurredOriginal.width() != w || m_blurredOriginal.height() != h)) {
            m_blurredOriginal = QImage();
            m_originalImage = QImage();
        }
        if (m_showOriginal) {
            ensureBlurredOriginal();
        }

        if (m_edgeSoftening > 0 && m_bgType != Infrared) {
            cv::Mat result = m_processor->applySoftening(m_processor->getCurrentImage(), m_edgeSoftening);
            m_softenedImage = QImage(result.data, result.cols, result.rows,
                                     static_cast<int>(result.step), QImage::Format_ARGB32)
                                     .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        } else {
            m_softenedImage = QImage();
        }

        if (m_bgType == Infrared) {
            rebuildInfraredBaseOnly();
            scheduleInfraredHaloScan(0);
            m_displayDirty = true;
            m_displayMipDirty = true;
        } else {
            m_processor->updateDisplayRegion(m_displayImage, QRect(0, 0, w, h));
            m_displayDirty = false;
            m_infraredImage = QImage();
            m_infraredMip4 = QImage();
            ++m_infraredJobId;
            rebuildMipCache();
            m_displayMipDirty = false;
        }
    } else {
        m_displayImage = QImage();
        m_displayMip4 = QImage();
        m_displayDirty = false;
        m_displayMipDirty = false;
        m_originalImage = QImage();
        m_softenedImage = QImage();
        m_blurredOriginal = QImage();
        m_infraredImage = QImage();
        m_infraredMip4 = QImage();
        m_irDustMask.release();
        m_irDustHalo.release();
        ++m_infraredJobId;
        m_dustChunkCount = 0;
        m_ghostPixelCount = 0;
        m_isLargeImage = false;
        m_renderedRegion = QRect();
    }
    update();
}

void CanvasWidget::rebuildMipCache() {
    if (m_displayImage.isNull() || (m_displayImage.width() * m_displayImage.height() <= 2000000)) {
        m_displayMip4 = QImage();
        m_displayMipDirty = false;
        return;
    }
    const int mw = std::max(1, m_displayImage.width() / 4);
    const int mh = std::max(1, m_displayImage.height() / 4);
    cv::Mat srcView(m_displayImage.height(), m_displayImage.width(), CV_8UC4,
                    const_cast<uchar*>(m_displayImage.constBits()),
                    static_cast<size_t>(m_displayImage.bytesPerLine()));
    if (m_displayMip4.width() != mw || m_displayMip4.height() != mh ||
        m_displayMip4.format() != QImage::Format_ARGB32_Premultiplied) {
        m_displayMip4 = QImage(mw, mh, QImage::Format_ARGB32_Premultiplied);
    }
    cv::Mat dstView(mh, mw, CV_8UC4, m_displayMip4.bits(), static_cast<size_t>(m_displayMip4.bytesPerLine()));
    cv::resize(srcView, dstView, cv::Size(mw, mh), 0, 0, cv::INTER_AREA);
    m_displayMipDirty = false;
}

void CanvasWidget::updateDisplayMipRegion(const QRect& imageRect) {
    if (m_displayImage.isNull()) return;
    if (m_displayImage.width() * m_displayImage.height() <= 2000000) {
        m_displayMip4 = QImage();
        m_displayMipDirty = false;
        return;
    }
    if (m_displayMip4.isNull() || m_displayMipDirty) {
        rebuildMipCache();
        return;
    }
    const int mw = m_displayMip4.width();
    const int mh = m_displayMip4.height();
    int mx0 = std::clamp(imageRect.left() / 4, 0, mw - 1);
    int my0 = std::clamp(imageRect.top() / 4, 0, mh - 1);
    int mx1 = std::clamp(imageRect.right() / 4 + 1, 0, mw);
    int my1 = std::clamp(imageRect.bottom() / 4 + 1, 0, mh);
    if (mx1 <= mx0 || my1 <= my0) return;

    int sx0 = mx0 * 4;
    int sy0 = my0 * 4;
    int sw = std::min(m_displayImage.width() - sx0, (mx1 - mx0) * 4);
    int sh = std::min(m_displayImage.height() - sy0, (my1 - my0) * 4);
    if (sw <= 0 || sh <= 0) return;

    cv::Mat srcView(sh, sw, CV_8UC4,
                    const_cast<uchar*>(m_displayImage.constScanLine(sy0)) + sx0 * 4,
                    static_cast<size_t>(m_displayImage.bytesPerLine()));
    cv::Mat dstView(my1 - my0, mx1 - mx0, CV_8UC4,
                    m_displayMip4.scanLine(my0) + mx0 * 4,
                    static_cast<size_t>(m_displayMip4.bytesPerLine()));
    cv::resize(srcView, dstView, cv::Size(mx1 - mx0, my1 - my0), 0, 0, cv::INTER_AREA);
}

void CanvasWidget::rebuildInfraredMipRegion(const QRect& imageRect) {
    if (m_infraredImage.isNull() || (m_infraredImage.width() * m_infraredImage.height() <= 1000000)) {
        m_infraredMip4 = QImage();
        return;
    }
    const int W = m_infraredImage.width();
    const int H = m_infraredImage.height();
    const int mw = std::max(1, W / 4);
    const int mh = std::max(1, H / 4);
    const bool fullReset = (m_infraredMip4.width() != mw || m_infraredMip4.height() != mh ||
                            m_infraredMip4.format() != QImage::Format_ARGB32_Premultiplied);
    if (fullReset) {
        m_infraredMip4 = QImage(mw, mh, QImage::Format_ARGB32_Premultiplied);
    }

    QRect roi = fullReset ? QRect(0, 0, W, H) : imageRect.intersected(QRect(0, 0, W, H));
    if (roi.isEmpty()) return;

    int mx0 = std::clamp(roi.left() / 4, 0, mw - 1);
    int my0 = std::clamp(roi.top() / 4, 0, mh - 1);
    int mx1 = std::clamp(roi.right() / 4 + 1, 0, mw);
    int my1 = std::clamp(roi.bottom() / 4 + 1, 0, mh);

    const uchar* const irBits = m_infraredImage.constBits();
    const int irStride = m_infraredImage.bytesPerLine();
    uchar* const mipBits = m_infraredMip4.bits();
    const int mipStride = m_infraredMip4.bytesPerLine();

    for (int my = my0; my < my1; ++my) {
        const int sy0 = my * 4;
        const int sy1 = std::min(H, sy0 + 4);
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(mipBits + static_cast<size_t>(my) * mipStride);
        for (int mx = mx0; mx < mx1; ++mx) {
            const int sx0 = mx * 4;
            const int sx1 = std::min(W, sx0 + 4);
            uint32_t bestPx = 0;
            int bestPrio = -1;
            for (int y = sy0; y < sy1; ++y) {
                const uint32_t* sRow = reinterpret_cast<const uint32_t*>(irBits + static_cast<size_t>(y) * irStride);
                for (int x = sx0; x < sx1; ++x) {
                    const uint32_t c = sRow[x];
                    const uint32_t a = c >> 24;
                    int prio = 0;
                    if (a != 0u) {
                        if (((c >> 16) & 0xFFu) == 255u) {
                            bestPx = c;
                            goto found_max;
                        }
                        prio = (a == 175u) ? 2 : 1;
                    }
                    if (prio > bestPrio) {
                        bestPrio = prio;
                        bestPx = c;
                    }
                }
            }
            found_max:
            dstRow[mx] = bestPx;
        }
    }
}

void CanvasWidget::rebuildInfraredBaseOnly() {
    m_irDustMask.release();
    m_irDustHalo.release();
    if (!m_processor || !m_processor->hasImage()) {
        m_infraredImage = QImage();
        m_infraredMip4 = QImage();
        m_dustChunkCount = 0;
        m_ghostPixelCount = 0;
        emit infraredStatsChanged(0, 0);
        return;
    }

    const cv::Mat& cur = m_processor->getCurrentImage();
    if (cur.empty() || cur.channels() != 4) {
        m_infraredImage = QImage();
        m_infraredMip4 = QImage();
        return;
    }

    const int H = cur.rows;
    const int W = cur.cols;
    if (m_infraredImage.width() != W || m_infraredImage.height() != H ||
        m_infraredImage.format() != QImage::Format_ARGB32_Premultiplied) {
        m_infraredImage = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    }

    const QRect fullRect(0, 0, W, H);
    shadeInfraredRect(cur, m_infraredImage, fullRect);
    rebuildInfraredMipRegion(fullRect);
}

void CanvasWidget::scheduleInfraredHaloScan(int delayMs) {
    if (m_bgType != Infrared || !m_infraredHaloTimer) return;
    ++m_infraredJobId;
    if (delayMs <= 0) {
        m_infraredHaloTimer->stop();
        startInfraredHaloScan();
    } else {
        m_infraredHaloTimer->start(delayMs);
    }
}

void CanvasWidget::startInfraredHaloScan() {
    if (m_bgType != Infrared || !m_processor || !m_processor->hasImage() || !m_infraredWatcher) return;
    if (m_isDrawing) {
        m_infraredHaloTimer->start(120);
        return;
    }
    if (m_infraredWatcher->isRunning()) {
        // Re-schedule shortly so latest edits aren't missed cleanly without queueing overlapping full scans
        m_infraredHaloTimer->start(90);
        return;
    }

    const cv::Mat& cur = m_processor->getCurrentImage();
    if (cur.empty() || cur.channels() != 4) return;

    cv::Mat alpha;
    cv::extractChannel(cur, alpha, 3);
    const quint64 jobId = m_infraredJobId;

    m_infraredWatcher->setFuture(QtConcurrent::run([alpha, jobId]() -> InfraredHaloResult {
        InfraredHaloResult res;
        res.jobId = jobId;
        const int H = alpha.rows;
        const int W = alpha.cols;

        cv::Mat nonZeroMask(H, W, CV_8UC1);
        int ghostCount = 0;
        for (int y = 0; y < H; ++y) {
            const uchar* aRow = alpha.ptr<uchar>(y);
            uchar* nzRow = nonZeroMask.ptr<uchar>(y);
            for (int x = 0; x < W; ++x) {
                uchar a = aRow[x];
                nzRow[x] = (a > 0) ? 255 : 0;
                if (a > 0 && a <= 40) ghostCount++;
            }
        }

        cv::Mat labels, stats, centroids;
        int numLabels = cv::connectedComponentsWithStats(nonZeroMask, labels, stats, centroids, 8, CV_32S);

        int maxArea = 0;
        for (int i = 1; i < numLabels; ++i) {
            maxArea = std::max(maxArea, stats.at<int>(i, cv::CC_STAT_AREA));
        }

        // Flag isolated stray specks (including single 1x1 solid alpha==255 pixels up to 120px)
        // without misclassifying real detached mask parts (ribbons, hair locks, accessories) as dust.
        constexpr int kMaxDustArea = 120;
        std::vector<uchar> isDustLabel(static_cast<size_t>(numLabels), 0);
        int dustChunks = 0;
        for (int i = 1; i < numLabels; ++i) {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area <= kMaxDustArea && (maxArea == 0 || area < static_cast<int>(maxArea * 0.15))) {
                isDustLabel[static_cast<size_t>(i)] = 1;
                dustChunks++;
            }
        }

        res.dustMask = cv::Mat::zeros(H, W, CV_8UC1);
        if (dustChunks > 0) {
            for (int y = 0; y < H; ++y) {
                const int* lblRow = labels.ptr<int>(y);
                uchar* dRow = res.dustMask.ptr<uchar>(y);
                for (int x = 0; x < W; ++x) {
                    int lbl = lblRow[x];
                    if (lbl > 0 && isDustLabel[static_cast<size_t>(lbl)]) {
                        dRow[x] = 255;
                    }
                }
            }
            cv::Mat kHalo = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9));
            cv::dilate(res.dustMask, res.dustHalo, kHalo);
        } else {
            res.dustHalo = cv::Mat::zeros(H, W, CV_8UC1);
        }

        res.dustChunks = dustChunks;
        res.ghostCount = ghostCount;
        return res;
    }));
}

void CanvasWidget::rebuildInfraredCache() {
    rebuildInfraredBaseOnly();
    scheduleInfraredHaloScan(0);
}

void CanvasWidget::updateInfraredRegion(const QRect& imageRect) {
    if (m_bgType != Infrared || m_infraredImage.isNull() || !m_processor || !m_processor->hasImage()) return;
    const cv::Mat& cur = m_processor->getCurrentImage();
    if (cur.empty() || cur.channels() != 4) return;

    QRect clamped = imageRect.intersected(QRect(0, 0, cur.cols, cur.rows));
    if (clamped.isEmpty()) return;

    shadeInfraredRect(cur, m_infraredImage, clamped, &m_irDustMask, &m_irDustHalo);
    rebuildInfraredMipRegion(clamped);
}

void CanvasWidget::paintBlurredBackdrop(QPainter& painter, const QRect& panelRectInCanvas) const {
    Q_UNUSED(painter);
    Q_UNUSED(panelRectInCanvas);
}

void CanvasWidget::ensureBlurredOriginal() {
    if (!m_processor || !m_processor->hasImage()) return;
    int w = m_processor->getWidth();
    int h = m_processor->getHeight();
    if (!m_blurredOriginal.isNull() && m_blurredOriginal.width() == w && m_blurredOriginal.height() == h) {
        return;
    }
    const cv::Mat& origMat = m_processor->getOriginalImage();
    if (!origMat.empty()) {
        cv::Mat smallMat, blurredMat;
        cv::resize(origMat, smallMat, cv::Size(std::max(1, w / 4), std::max(1, h / 4)), 0, 0, cv::INTER_AREA);
        cv::resize(smallMat, blurredMat, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
        m_blurredOriginal = QImage(blurredMat.data, blurredMat.cols, blurredMat.rows,
                                   static_cast<int>(blurredMat.step), QImage::Format_ARGB32)
                                   .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
}

void CanvasWidget::setShowOriginal(bool show) {
    m_showOriginal = show;
    if (show) {
        ensureBlurredOriginal();
    }
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
        m_softenedImage = QImage(result.data, result.cols, result.rows,
                                 static_cast<int>(result.step), QImage::Format_ARGB32)
                                 .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    } else {
        m_softenedImage = QImage();
    }

    update();
}

void CanvasWidget::updateRegion(const QRect& imageRect) {
    if (!m_processor || !m_processor->hasImage()) return;

    if (m_bgType == Infrared) {
        updateInfraredRegion(imageRect);
        m_displayDirty = true;
        m_displayMipDirty = true;
    } else {
        if (m_displayImage.isNull()) return;
        m_processor->updateDisplayRegion(m_displayImage, imageRect);
    }

    QRect screenRect = imageRectToScreen(imageRect);
    update(screenRect.adjusted(-12, -12, 12, 12));
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

    const double pad = 28.0;
    double availW = std::max(100.0, width() - pad * 2.0);
    double availH = std::max(100.0, height() - pad * 2.0);

    double scaleX = availW / m_processor->getWidth();
    double scaleY = availH / m_processor->getHeight();
    double scale = std::min(scaleX, scaleY) * 0.96;

    m_zoom = std::clamp(scale, MIN_ZOOM, MAX_ZOOM);

    double imgW = m_processor->getWidth() * m_zoom;
    double imgH = m_processor->getHeight() * m_zoom;
    m_panOffset = QPointF((width() - imgW) / 2.0, (height() - imgH) / 2.0);

    emit zoomChanged(m_zoom);
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

    int margin = 100;
    visible.adjust(-margin, -margin, margin, margin);
    visible = visible.intersected(QRect(0, 0, m_processor->getWidth(), m_processor->getHeight()));

    if (m_renderedRegion.isEmpty() || !m_renderedRegion.contains(visible)) {
        m_processor->updateDisplayRegion(m_displayImage, visible);
        if (m_renderedRegion.isEmpty()) {
            m_renderedRegion = visible;
        } else {
            m_renderedRegion = m_renderedRegion.united(visible);
        }
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
    updateCheckerTile();
    if (m_bgType == Infrared) {
        rebuildInfraredCache();
    } else {
        m_infraredImage = QImage();
        m_infraredMip4 = QImage();
        m_irDustMask.release();
        m_irDustHalo.release();
        ++m_infraredJobId;
        if (m_displayDirty && m_processor && m_processor->hasImage()) {
            const int w = m_processor->getWidth();
            const int h = m_processor->getHeight();
            if (m_displayImage.width() != w || m_displayImage.height() != h) {
                m_displayImage = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
            }
            m_processor->updateDisplayRegion(m_displayImage, QRect(0, 0, w, h));
            m_displayDirty = false;
            m_displayMipDirty = true;
            if (m_edgeSoftening > 0) {
                cv::Mat result = m_processor->applySoftening(m_processor->getCurrentImage(), m_edgeSoftening);
                m_softenedImage = QImage(result.data, result.cols, result.rows,
                                         static_cast<int>(result.step), QImage::Format_ARGB32)
                                         .convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }
        }
        if (m_displayMipDirty) {
            rebuildMipCache();
        }
    }
    update();
}

void CanvasWidget::updateCheckerTile() {
    if (m_bgType != Dark) {
        m_checkerTile = QPixmap();
        return;
    }

    // Classic OG neutral grayish dark checkerboard — zero bluish tint!
    const QColor color1(42, 42, 46);
    const QColor color2(54, 54, 58);

    const int s = CHECKER_SIZE;
    m_checkerTile = QPixmap(s * 2, s * 2);
    QPainter p(&m_checkerTile);
    p.fillRect(0, 0, s, s, color1);
    p.fillRect(s, 0, s, s, color2);
    p.fillRect(0, s, s, s, color2);
    p.fillRect(s, s, s, s, color1);
}

void CanvasWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    QRect dirtyRect = event->rect();
    const bool isPartialHover = (dirtyRect.width() < width() || dirtyRect.height() < height());
    const bool fastMode = m_isPanning || m_isDrawing || m_isWheelInteracting || isPartialHover;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, !fastMode && m_zoom < 1.0);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // ALWAYS fill the ENTIRE canvas viewport with the active surface (OG neutral gray checkerboard / White / Black / Infrared)!
    drawCheckerboard(painter, dirtyRect);

    if (!m_displayImage.isNull()) {
        drawImage(painter, dirtyRect);
    }

    if (!m_spaceHeld && !m_isPanning && m_mouseInWidget && m_toolManager &&
        m_toolManager->currentTool() != ToolManager::AutoColor && !m_displayImage.isNull()) {
        drawBrushCursor(painter);
    }
}

void CanvasWidget::drawCheckerboard(QPainter& painter, const QRect& clipRect) {
    switch (m_bgType) {
        case White:
            painter.fillRect(clipRect, Qt::white);
            return;
        case Black:
            painter.fillRect(clipRect, Qt::black);
            return;
        case Infrared:
            painter.fillRect(clipRect, QColor(8, 8, 10));
            return;
        case Dark:
        default:
            if (m_checkerTile.isNull()) {
                updateCheckerTile();
            }
            painter.fillRect(clipRect, QBrush(m_checkerTile));
            return;
    }
}

void CanvasWidget::drawImage(QPainter& painter, const QRect& clipRect) {
    if (m_displayImage.isNull()) return;

    const bool useInfraredMip = (m_bgType == Infrared && m_zoom <= 0.45 && !m_infraredMip4.isNull());
    const bool useDisplayMip = (m_bgType != Infrared && !m_isDrawing && m_zoom <= 0.45 &&
                                !m_displayMip4.isNull() && m_edgeSoftening == 0);

    QImage* imgToDraw = &m_displayImage;
    double srcScale = 1.0;
    if (useInfraredMip) {
        imgToDraw = &m_infraredMip4;
        srcScale = static_cast<double>(m_infraredMip4.width()) / std::max(1, m_displayImage.width());
    } else if (useDisplayMip) {
        imgToDraw = &m_displayMip4;
        srcScale = static_cast<double>(m_displayMip4.width()) / std::max(1, m_displayImage.width());
    } else if (m_bgType == Infrared && !m_infraredImage.isNull()) {
        imgToDraw = &m_infraredImage;
    } else if (!m_isDrawing && m_edgeSoftening > 0 && !m_softenedImage.isNull()) {
        imgToDraw = &m_softenedImage;
    }

    const int fullW = m_displayImage.width();
    const int fullH = m_displayImage.height();

    int sx1 = std::max(0, static_cast<int>(std::floor((clipRect.left() - m_panOffset.x()) / m_zoom)) - 2);
    int sy1 = std::max(0, static_cast<int>(std::floor((clipRect.top() - m_panOffset.y()) / m_zoom)) - 2);
    int sx2 = std::min(fullW, static_cast<int>(std::ceil((clipRect.right() + 1 - m_panOffset.x()) / m_zoom)) + 2);
    int sy2 = std::min(fullH, static_cast<int>(std::ceil((clipRect.bottom() + 1 - m_panOffset.y()) / m_zoom)) + 2);

    if (sx1 < sx2 && sy1 < sy2) {
        QRectF subSourceRect(sx1 * srcScale, sy1 * srcScale, (sx2 - sx1) * srcScale, (sy2 - sy1) * srcScale);
        QRectF subTargetRect(
            m_panOffset.x() + sx1 * m_zoom,
            m_panOffset.y() + sy1 * m_zoom,
            (sx2 - sx1) * m_zoom,
            (sy2 - sy1) * m_zoom
        );

        painter.drawImage(subTargetRect, *imgToDraw, subSourceRect);

        if (m_showOriginal && !m_blurredOriginal.isNull()) {
            painter.setOpacity(m_compareOpacity * 0.7);
            painter.drawImage(subTargetRect, m_blurredOriginal, QRectF(sx1, sy1, sx2 - sx1, sy2 - sy1));
            painter.setOpacity(1.0);
        }
    }

    if (m_showOriginal && !m_blurredOriginal.isNull()) {
        painter.setPen(QColor(255, 255, 255, 210));
        painter.setFont(QFont("Segoe UI", 11, QFont::Bold));
        painter.drawText(rect().adjusted(10, 14, -10, -10), Qt::AlignTop | Qt::AlignHCenter, "COMPARING ORIGINAL");
    }
}

void CanvasWidget::drawBrushCursor(QPainter& painter) {
    if (!m_toolManager) return;

    const QPointF mousePos = QPointF(mapFromGlobal(QCursor::pos()));
    const double exactRadius = std::max(1.0, (m_toolManager->brushSize() * 0.5) * m_zoom);

    painter.setRenderHint(QPainter::Antialiasing, true);

    bool isEraser = (m_toolManager->currentTool() == ToolManager::ManualErase);
    QColor mainColor = isEraser ? QColor(255, 90, 90) : QColor(120, 210, 255);

    painter.setPen(QPen(mainColor, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(mousePos, exactRadius, exactRadius);

    if (exactRadius >= 5.0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawEllipse(mousePos, 1.5, 1.5);
    }
}

QPoint CanvasWidget::screenToImage(const QPoint& screenPos) const {
    return QPoint(
        static_cast<int>(std::floor((screenPos.x() - m_panOffset.x()) / m_zoom)),
        static_cast<int>(std::floor((screenPos.y() - m_panOffset.y()) / m_zoom))
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

    if (m_spaceHeld) {
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
        const int imgW = m_processor->getWidth();
        const int imgH = m_processor->getHeight();
        const QRect imgBounds(0, 0, imgW, imgH);

        if (m_toolManager->currentTool() == ToolManager::AutoColor) {
            if (!imgBounds.contains(imagePos) || m_processor->isPixelTransparent(imagePos.x(), imagePos.y())) {
                return;
            }

            m_historyManager->saveStateBeforeChange();
            QRect dirtyRect = handleAutoColorTool(imagePos);
            m_historyManager->saveState();
            if (!dirtyRect.isEmpty()) {
                updateRegion(dirtyRect);
                if (m_bgType == Infrared) {
                    m_displayMipDirty = true;
                    scheduleInfraredHaloScan(140);
                } else {
                    updateDisplayMipRegion(dirtyRect);
                    if (m_edgeSoftening > 0 && m_processor && m_processor->hasImage()) {
                        cv::Mat result = m_processor->applySoftening(m_processor->getCurrentImage(), m_edgeSoftening);
                        m_softenedImage = QImage(result.data, result.cols, result.rows,
                                                 static_cast<int>(result.step), QImage::Format_ARGB32)
                                                 .convertToFormat(QImage::Format_ARGB32_Premultiplied);
                        update();
                    }
                }
                emit imageModified();
            }
        } else {
            // Allow Eraser & Repair strokes to start outside the image frame and sweep smoothly across edges
            m_isDrawing = true;
            m_lastDrawPos = imagePos;
            m_historyManager->saveStateBeforeChange();

            int brushSize = m_toolManager->brushSize();
            const int r = static_cast<int>(std::ceil(std::max(0.5f, brushSize * 0.5f))) + 1;
            QRect dirtyRect = QRect(imagePos.x() - r, imagePos.y() - r,
                                    r * 2 + 1, r * 2 + 1).intersected(imgBounds);

            if (!dirtyRect.isEmpty()) {
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
    if (!m_processor || !m_processor->hasImage()) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    QPoint imagePos = screenToImage(event->pos());

    if (!m_isPanning && !m_isDrawing && m_processor && m_processor->hasImage()) {
        if (imagePos != m_lastEmittedImagePos &&
            imagePos.x() >= 0 && imagePos.x() < m_processor->getWidth() &&
            imagePos.y() >= 0 && imagePos.y() < m_processor->getHeight()) {
            m_lastEmittedImagePos = imagePos;
            emit cursorPositionChanged(imagePos.x(), imagePos.y());
        }
    }

    if (m_spaceHeld) {
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
        const QRect imgBounds(0, 0, m_processor->getWidth(), m_processor->getHeight());
        const int r = static_cast<int>(std::ceil(std::max(0.5f, brushSize * 0.5f))) + 1;

        int minX = std::min(m_lastDrawPos.x(), imagePos.x()) - r;
        int minY = std::min(m_lastDrawPos.y(), imagePos.y()) - r;
        int maxX = std::max(m_lastDrawPos.x(), imagePos.x()) + r;
        int maxY = std::max(m_lastDrawPos.y(), imagePos.y()) + r;
        QRect dirtyRect = QRect(minX, minY, maxX - minX + 1, maxY - minY + 1).intersected(imgBounds);

        if (!dirtyRect.isEmpty()) {
            if (m_toolManager->currentTool() == ToolManager::ManualErase) {
                m_processor->eraseAlongPath(m_lastDrawPos, imagePos, brushSize,
                    m_toolManager->brushHardness());
            } else {
                m_processor->repairAlongPath(m_lastDrawPos, imagePos, brushSize);
            }
            updateRegion(dirtyRect);
        } else {
            // Keep brush cursor preview crisp while still sweeping outside the image frame
            int r = static_cast<int>(brushSize * m_zoom / 2) + 12;
            QRect oldRect(m_lastMousePos.x() - r, m_lastMousePos.y() - r, r * 2, r * 2);
            QRect newRect(event->pos().x() - r, event->pos().y() - r, r * 2, r * 2);
            update(oldRect.united(newRect));
        }

        m_lastDrawPos = imagePos;
        m_lastMousePos = event->pos();
        return;
    }

    if (m_toolManager) {
        syncActiveCursor();
        if (m_toolManager->currentTool() != ToolManager::AutoColor) {
            int r = static_cast<int>(m_toolManager->brushSize() * m_zoom / 2) + 10;
            QRect oldRect(m_lastMousePos.x() - r, m_lastMousePos.y() - r, r * 2, r * 2);
            QRect newRect(event->pos().x() - r, event->pos().y() - r, r * 2, r * 2);
            update(oldRect.united(newRect));
        }
    }
    m_lastMousePos = event->pos();
}

void CanvasWidget::syncActiveCursor() {
    if (!m_toolManager) return;
    Qt::CursorShape target = Qt::ArrowCursor;
    if (m_spaceHeld) {
        target = m_isPanning ? Qt::ClosedHandCursor : Qt::OpenHandCursor;
    } else if (m_mouseInWidget && m_processor && m_processor->hasImage()) {
        target = (m_toolManager->currentTool() == ToolManager::AutoColor) ? Qt::CrossCursor : Qt::BlankCursor;
    }
    if (cursor().shape() != target) {
        setCursor(target);
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);

    if (m_isPanning) {
        m_isPanning = false;
        syncActiveCursor();
        update();
    }

    if (m_isDrawing) {
        m_isDrawing = false;
        m_historyManager->saveState();
        if (m_bgType == Infrared) {
            m_displayMipDirty = true;
            scheduleInfraredHaloScan(140);
        } else {
            rebuildMipCache();
            m_displayMipDirty = false;
            if (m_edgeSoftening > 0 && m_processor && m_processor->hasImage()) {
                cv::Mat result = m_processor->applySoftening(m_processor->getCurrentImage(), m_edgeSoftening);
                m_softenedImage = QImage(result.data, result.cols, result.rows,
                                         static_cast<int>(result.step), QImage::Format_ARGB32)
                                         .convertToFormat(QImage::Format_ARGB32_Premultiplied);
                update();
            }
        }
        emit imageModified();
    }
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
    m_isWheelInteracting = true;
    m_wheelSettleTimer->start();

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

void CanvasWidget::setSpaceHeld(bool held) {
    if (m_spaceHeld == held) return;
    m_spaceHeld = held;
    if (m_spaceHeld) {
        if (m_isDrawing) {
            m_isDrawing = false;
            m_historyManager->saveState();
            if (m_bgType == Infrared) {
                m_displayMipDirty = true;
                scheduleInfraredHaloScan(140);
            }
            emit imageModified();
        }
        syncActiveCursor();
    } else {
        m_isPanning = false;
        syncActiveCursor();
    }
    update();
}

void CanvasWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        setSpaceHeld(true);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CanvasWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        setSpaceHeld(false);
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void CanvasWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    m_mouseInWidget = true;
    if (m_spaceHeld) {
        setCursor(Qt::OpenHandCursor);
    } else if (m_processor && m_processor->hasImage() && m_toolManager) {
        setCursor(m_toolManager->currentTool() == ToolManager::AutoColor ? Qt::CrossCursor : Qt::BlankCursor);
    }
}

void CanvasWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    m_mouseInWidget = false;
    setCursor(Qt::ArrowCursor);
    update();
}

QRect CanvasWidget::handleAutoColorTool(const QPoint& imagePos) {
    if (!m_processor || !m_toolManager) return QRect();
    QRect visibleRect = getVisibleImageRect();
    QRect dirtyRect;
    m_processor->autoColorRemove(imagePos.x(), imagePos.y(),
        m_toolManager->tolerance(), visibleRect, &dirtyRect);
    return dirtyRect;
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
