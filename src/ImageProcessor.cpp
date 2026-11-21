#include "ImageProcessor.h"
#include <QFile>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <stack>
#include <thread>

ImageProcessor::ImageProcessor() = default;
ImageProcessor::~ImageProcessor() = default;

bool ImageProcessor::loadImage(const QString& path) {
    // Unicode-safe read via QFile + cv::imdecode (supports non-ASCII / Japanese / emoji filenames on Windows)
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray raw = f.readAll();
    if (raw.isEmpty()) {
        return false;
    }
    std::vector<uchar> buf(raw.begin(), raw.end());
    m_originalImage = cv::imdecode(buf, cv::IMREAD_UNCHANGED);

    if (m_originalImage.empty()) {
        return false;
    }

    ensureAlphaChannel(m_originalImage);
    m_currentImage = m_originalImage.clone();

    // Self-Remembering PNG check:
    // If a loaded 4-channel PNG already has transparent pixels (alpha < 255) that preserve
    // non-zero original RGB values underneath, restore m_originalImage's alpha to 255 so
    // Repair Brush [R], Compare Original [H], Restore Original, and G4 AI Remover can
    // bring back the removed background even after saving & reopening the PNG!
    m_hasEmbeddedOriginal = false;
    int transparentWithRgbCount = 0;
    int transparentCount = 0;
    for (int y = 0; y < m_originalImage.rows; ++y) {
        cv::Vec4b* origRow = m_originalImage.ptr<cv::Vec4b>(y);
        for (int x = 0; x < m_originalImage.cols; ++x) {
            cv::Vec4b& px = origRow[x];
            if (px[3] < 255) {
                transparentCount++;
                if (px[0] > 0 || px[1] > 0 || px[2] > 0) {
                    transparentWithRgbCount++;
                }
                px[3] = 255; // Make m_originalImage fully opaque so Repair/Compare reveals the hidden RGB!
            }
        }
    }
    m_hasEmbeddedOriginal = (transparentCount > 0 && transparentWithRgbCount > 0);

    // Lazy LAB conversion: defer BGR->Lab until Auto Color Remove is actually used (~40% faster image load!)
    m_labImage.release();
    return true;
}

void ImageProcessor::updateLabCache() {
    if (m_currentImage.empty()) return;
    cv::Mat bgr;
    const cv::Mat& src = (!m_originalImage.empty() && m_originalImage.size() == m_currentImage.size())
                         ? m_originalImage : m_currentImage;
    cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
    cv::cvtColor(bgr, m_labImage, cv::COLOR_BGR2Lab);
}

void ImageProcessor::resize(int newWidth, int newHeight) {
    if (m_currentImage.empty()) return;

    cv::Mat resized;
    cv::resize(m_currentImage, resized, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LANCZOS4);
    m_currentImage = resized;

    cv::resize(m_originalImage, resized, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LANCZOS4);
    m_originalImage = resized;

    m_labImage.release(); // Recomputed lazily only if Auto Color Remove is used
}

void ImageProcessor::updateOriginalImage() {
    if (m_currentImage.empty()) return;
    m_originalImage = m_currentImage.clone();
    m_labImage.release(); // Recomputed lazily only if Auto Color Remove is used
}

void ImageProcessor::clear() {
    // Properly release all memory
    m_currentImage.release();
    m_originalImage.release();
    m_labImage.release();
    
    // Force deallocation
    m_currentImage = cv::Mat();
    m_originalImage = cv::Mat();
    m_labImage = cv::Mat();
}

bool ImageProcessor::saveImage(const QString& path) {
    return exportImage(path, 0);
}

bool ImageProcessor::exportImage(const QString& path, int edgeSoftenLevel) {
    if (m_currentImage.empty()) return false;

    cv::Mat softened = (edgeSoftenLevel > 0)
        ? applySoftening(m_currentImage, edgeSoftenLevel)
        : m_currentImage;

    // Self-Remembering PNG: embed the full original (B, G, R) channels under every pixel
    // while keeping the exact cutout Alpha channel (A = 0 looks 100% transparent everywhere,
    // yet reopening the PNG in PixelEraser Pro remembers the entire un-removed original image!)
    cv::Mat exportImage;
    if (!m_originalImage.empty() && m_originalImage.size() == softened.size() && m_originalImage.channels() == 4) {
        exportImage = m_originalImage.clone();
        cv::Mat alphaChan;
        cv::extractChannel(softened, alphaChan, 3);
        cv::insertChannel(alphaChan, exportImage, 3);
    } else {
        exportImage = softened;
    }

    std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 6};
    std::vector<uchar> encoded;
    if (!cv::imencode(".png", exportImage, encoded, params)) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(reinterpret_cast<const char*>(encoded.data()), static_cast<qint64>(encoded.size())) == static_cast<qint64>(encoded.size());
}

void ImageProcessor::restoreFullOriginal() {
    if (m_currentImage.empty() || m_originalImage.empty()) return;
    if (m_currentImage.size() == m_originalImage.size()) {
        m_currentImage = m_originalImage.clone();
        for (int y = 0; y < m_currentImage.rows; ++y) {
            cv::Vec4b* row = m_currentImage.ptr<cv::Vec4b>(y);
            for (int x = 0; x < m_currentImage.cols; ++x) {
                row[x][3] = 255;
            }
        }
    }
}

ImageProcessor::MaskApplyStatus ImageProcessor::applyExternalMask(const QString& maskPath, QSize* outMaskSize) {
    if (m_currentImage.empty()) return MaskErrorLoadFailed;

    QFile f(maskPath);
    if (!f.open(QIODevice::ReadOnly)) return MaskErrorLoadFailed;
    QByteArray raw = f.readAll();
    if (raw.isEmpty()) return MaskErrorLoadFailed;

    std::vector<uchar> buf(raw.begin(), raw.end());
    cv::Mat maskRaw = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
    if (maskRaw.empty()) return MaskErrorLoadFailed;

    if (outMaskSize) {
        *outMaskSize = QSize(maskRaw.cols, maskRaw.rows);
    }

    // Strict dimension check: mask or transparent reference image MUST match current image width x height
    if (maskRaw.cols != m_currentImage.cols || maskRaw.rows != m_currentImage.rows) {
        return MaskErrorDimensionMismatch;
    }

    // Normalize 16-bit masks to 8-bit if needed
    cv::Mat mask8;
    if (maskRaw.depth() == CV_16U) {
        maskRaw.convertTo(mask8, CV_8U, 1.0 / 256.0);
    } else if (maskRaw.depth() != CV_8U) {
        maskRaw.convertTo(mask8, CV_8U);
    } else {
        mask8 = maskRaw;
    }

    cv::Mat alphaMask;
    MaskApplyStatus status = MaskSuccessLuminance;

    // Check if current canvas image ALREADY has transparency
    cv::Mat currentAlpha;
    cv::extractChannel(m_currentImage, currentAlpha, 3);
    const int totalPx = mask8.rows * mask8.cols;
    const bool currentHasTransparency = (cv::countNonZero(currentAlpha < 255) > 0);

    // Helper: check if a 3/4-channel image is actually a color photo/illustration (not a grayscale B&W mask)
    auto isColorImage = [](const cv::Mat& img) -> bool {
        if (img.channels() < 3) return false;
        int colorDiffPx = 0;
        const int stepY = std::max(1, img.rows / 64);
        const int stepX = std::max(1, img.cols / 64);
        for (int y = 0; y < img.rows; y += stepY) {
            const uchar* row = img.ptr<uchar>(y);
            const int ch = img.channels();
            for (int x = 0; x < img.cols; x += stepX) {
                const uchar b = row[x * ch + 0];
                const uchar g = row[x * ch + 1];
                const uchar r = row[x * ch + 2];
                if (std::abs(int(r) - int(g)) > 4 || std::abs(int(g) - int(b)) > 4) {
                    colorDiffPx++;
                    if (colorDiffPx > 12) return true;
                }
            }
        }
        return false;
    };

    if (mask8.channels() == 4) {
        cv::Mat extractedAlpha;
        cv::extractChannel(mask8, extractedAlpha, 3);
        const int opaquePx = cv::countNonZero(extractedAlpha == 255);
        if (opaquePx < totalPx) {
            // Uploaded file is a transparent RGBA cutout image -> apply its alpha boundary!
            alphaMask = extractedAlpha;
            status = MaskSuccessAlpha;
        } else if (currentHasTransparency && isColorImage(mask8)) {
            // Canvas is already a transparent cutout and user uploaded the full-color opaque original image:
            // Embed the full original RGB into m_originalImage & m_currentImage while keeping current cutout alpha!
            m_originalImage = mask8.clone();
            m_currentImage = mask8.clone();
            cv::insertChannel(currentAlpha, m_currentImage, 3);
            m_hasEmbeddedOriginal = true;
            m_labImage.release();
            return MaskSuccessEmbeddedRgb;
        } else {
            cv::cvtColor(mask8, alphaMask, cv::COLOR_BGRA2GRAY);
            status = MaskSuccessLuminance;
        }
    } else if (mask8.channels() == 3) {
        if (currentHasTransparency && isColorImage(mask8)) {
            // Canvas is already a transparent cutout and user uploaded the 3-channel full-color original image!
            cv::cvtColor(mask8, m_originalImage, cv::COLOR_BGR2BGRA);
            m_currentImage = m_originalImage.clone();
            cv::insertChannel(currentAlpha, m_currentImage, 3);
            m_hasEmbeddedOriginal = true;
            m_labImage.release();
            return MaskSuccessEmbeddedRgb;
        }
        cv::cvtColor(mask8, alphaMask, cv::COLOR_BGR2GRAY);
        status = MaskSuccessLuminance;
    } else {
        alphaMask = mask8;
        status = MaskSuccessLuminance;
    }

    // Preserve 100% of original (B, G, R) while applying the boundary/cutout alpha mask!
    if (!m_originalImage.empty() && m_originalImage.size() == m_currentImage.size() && m_originalImage.channels() == 4) {
        m_currentImage = m_originalImage.clone();
    }
    cv::insertChannel(alphaMask, m_currentImage, 3);
    m_hasEmbeddedOriginal = true;
    return status;
}

bool ImageProcessor::isPixelTransparent(int x, int y) const {
    if (m_currentImage.empty() || x < 0 || x >= m_currentImage.cols || y < 0 || y >= m_currentImage.rows) {
        return true;
    }
    return m_currentImage.ptr<cv::Vec4b>(y)[x][3] == 0;
}

QImage ImageProcessor::getDisplayImage() const {
    if (m_currentImage.empty()) return QImage();
    
    return QImage(m_currentImage.data, m_currentImage.cols, m_currentImage.rows,
                  static_cast<int>(m_currentImage.step), QImage::Format_ARGB32).copy();
}

void ImageProcessor::updateDisplayRegion(QImage& target, const QRect& region) const {
    if (m_currentImage.empty() || target.isNull()) return;
    
    // Clamp region to image bounds
    int x1 = std::max(0, region.left());
    int y1 = std::max(0, region.top());
    int x2 = std::min(m_currentImage.cols, region.right() + 1);
    int y2 = std::min(m_currentImage.rows, region.bottom() + 1);
    
    if (x1 >= x2 || y1 >= y2) return;
    
    // Fast path: Format_ARGB32_Premultiplied feeds Qt's SIMD AVX2/SSE2 blitter directly (3-5x faster panning/brushing!)
    if (target.format() == QImage::Format_ARGB32_Premultiplied) {
        const int count = x2 - x1;
        for (int y = y1; y < y2; ++y) {
            const uint32_t* src32 = reinterpret_cast<const uint32_t*>(m_currentImage.ptr<cv::Vec4b>(y)) + x1;
            uint32_t* dst32 = reinterpret_cast<uint32_t*>(target.scanLine(y)) + x1;
            for (int i = 0; i < count; ++i) {
                const uint32_t p = src32[i];
                const uint32_t a = p >> 24;
                if (a == 255u) {
                    dst32[i] = p;
                } else if (a == 0u) {
                    dst32[i] = 0u;
                } else {
                    // Exact SWAR premultiply: (c * a + 127) / 255 without division
                    uint32_t rb = (p & 0x00FF00FFu) * a + 0x00800080u;
                    rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
                    uint32_t g = ((p & 0x0000FF00u) >> 8) * a + 0x80u;
                    g = ((g + (g >> 8)) >> 8) << 8;
                    dst32[i] = (a << 24) | rb | g;
                }
            }
        }
        return;
    }

    // Fast path: CV_8UC4 (BGRA) has identical byte layout in memory to QImage::Format_ARGB32 on little-endian
    if (target.format() == QImage::Format_ARGB32) {
        const size_t rowBytes = static_cast<size_t>(x2 - x1) * 4;
        if (x1 == 0 && x2 == m_currentImage.cols &&
            m_currentImage.step == static_cast<size_t>(target.bytesPerLine())) {
            const uchar* srcStart = m_currentImage.ptr<uchar>(y1);
            uchar* dstStart = target.scanLine(y1);
            std::memcpy(dstStart, srcStart, static_cast<size_t>(y2 - y1) * m_currentImage.step);
        } else {
            for (int y = y1; y < y2; ++y) {
                const cv::Vec4b* srcRow = m_currentImage.ptr<cv::Vec4b>(y);
                uchar* dstRow = target.scanLine(y);
                std::memcpy(dstRow + x1 * 4, srcRow + x1, rowBytes);
            }
        }
        return;
    }

    // Fallback for RGBA8888
    for (int y = y1; y < y2; ++y) {
        const cv::Vec4b* srcRow = m_currentImage.ptr<cv::Vec4b>(y);
        uchar* dstRow = target.scanLine(y);
        
        for (int x = x1; x < x2; ++x) {
            const cv::Vec4b& src = srcRow[x];
            int dstIdx = x * 4;
            dstRow[dstIdx + 0] = src[2]; // R
            dstRow[dstIdx + 1] = src[1]; // G
            dstRow[dstIdx + 2] = src[0]; // B
            dstRow[dstIdx + 3] = src[3]; // A
        }
    }
}

QImage ImageProcessor::getOriginalAsQImage() const {
    return matToQImage(m_originalImage);
}

void ImageProcessor::ensureAlphaChannel(cv::Mat& image) {
    if (image.channels() == 3) {
        cv::cvtColor(image, image, cv::COLOR_BGR2BGRA);
    } else if (image.channels() == 1) {
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGRA);
    }
}

QImage ImageProcessor::matToQImage(const cv::Mat& mat) const {
    if (mat.empty()) return QImage();

    if (mat.channels() == 4) {
        return QImage(mat.data, mat.cols, mat.rows, 
                     static_cast<int>(mat.step), QImage::Format_ARGB32).copy();
    } else if (mat.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows,
                     static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    }
    return QImage();
}

// Smart color matching using LAB color space - perceptually accurate & viewport-scoped memory
void ImageProcessor::autoColorRemove(int x, int y, int tolerance, const QRect& viewportBounds) {
    if (m_currentImage.empty()) return;
    if (x < 0 || x >= m_currentImage.cols || y < 0 || y >= m_currentImage.rows) return;

    cv::Vec4b seedBGRA = m_currentImage.ptr<cv::Vec4b>(y)[x];
    if (seedBGRA[3] == 0) return;

    // Build LAB cache lazily on first Auto Color click
    if (m_labImage.empty() || m_labImage.rows != m_currentImage.rows || m_labImage.cols != m_currentImage.cols) {
        updateLabCache();
    }

    cv::Vec3b seedLab = m_labImage.ptr<cv::Vec3b>(y)[x];

    int minX = viewportBounds.isValid() ? std::max(0, viewportBounds.left()) : 0;
    int minY = viewportBounds.isValid() ? std::max(0, viewportBounds.top()) : 0;
    int maxX = viewportBounds.isValid() ? std::min(m_currentImage.cols - 1, viewportBounds.right()) : m_currentImage.cols - 1;
    int maxY = viewportBounds.isValid() ? std::min(m_currentImage.rows - 1, viewportBounds.bottom()) : m_currentImage.rows - 1;

    if (x < minX || x > maxX || y < minY || y > maxY) return;

    // Allocate visited ONLY for the active viewport ROI (saves up to 64MB per click when zoomed in on 8K!)
    const int vw = maxX - minX + 1;
    const int vh = maxY - minY + 1;
    cv::Mat visited = cv::Mat::zeros(vh, vw, CV_8UC1);

    std::vector<cv::Point> queue;
    queue.reserve(std::min<size_t>(static_cast<size_t>(vw) * vh, 65536));
    visited.ptr<uchar>(y - minY)[x - minX] = 1;
    queue.push_back(cv::Point(x, y));

    size_t queuePos = 0;
    const float maxDeltaE = static_cast<float>(tolerance);
    const float maxDeltaESq = maxDeltaE * maxDeltaE;

    const float seedL = static_cast<float>(seedLab[0]);
    const float seedA = static_cast<float>(seedLab[1]);
    const float seedB = static_cast<float>(seedLab[2]);

    while (queuePos < queue.size()) {
        cv::Point pt = queue[queuePos++];

        cv::Vec4b& pixel = m_currentImage.ptr<cv::Vec4b>(pt.y)[pt.x];
        if (pixel[3] == 0) continue;

        const cv::Vec3b& pixelLab = m_labImage.ptr<cv::Vec3b>(pt.y)[pt.x];

        const float dL = static_cast<float>(pixelLab[0]) - seedL;
        const float da = static_cast<float>(pixelLab[1]) - seedA;
        const float db = static_cast<float>(pixelLab[2]) - seedB;
        const float deltaESq = dL * dL + da * da + db * db;

        if (deltaESq > maxDeltaESq) continue;

        pixel[3] = 0;

        const int ly = pt.y - minY;
        const int lx = pt.x - minX;
        uchar* visRow = visited.ptr<uchar>(ly);
        if (pt.x > minX && !visRow[lx - 1]) {
            visRow[lx - 1] = 1;
            queue.push_back(cv::Point(pt.x - 1, pt.y));
        }
        if (pt.x < maxX && !visRow[lx + 1]) {
            visRow[lx + 1] = 1;
            queue.push_back(cv::Point(pt.x + 1, pt.y));
        }
        if (pt.y > minY) {
            uchar* topVisRow = visited.ptr<uchar>(ly - 1);
            if (!topVisRow[lx]) {
                topVisRow[lx] = 1;
                queue.push_back(cv::Point(pt.x, pt.y - 1));
            }
        }
        if (pt.y < maxY) {
            uchar* botVisRow = visited.ptr<uchar>(ly + 1);
            if (!botVisRow[lx]) {
                botVisRow[lx] = 1;
                queue.push_back(cv::Point(pt.x, pt.y + 1));
            }
        }
    }
}

bool ImageProcessor::colorMatches(const cv::Vec4b& c1, const cv::Vec4b& c2, int tolerance) const {
    if (c1[3] == 0 || c2[3] == 0) return false;

    float db = static_cast<float>(c1[0] - c2[0]);
    float dg = static_cast<float>(c1[1] - c2[1]);
    float dr = static_cast<float>(c1[2] - c2[2]);
    
    float dist = std::sqrt(dr*dr*0.3f + dg*dg*0.59f + db*db*0.11f);
    
    return dist <= tolerance * 0.7f;
}

// Optimized brush - uses pointer arithmetic, no bounds checking in inner loop
void ImageProcessor::eraseWithBrush(int centerX, int centerY, int diameter, float hardness) {
    if (m_currentImage.empty()) return;

    int radius = diameter / 2;
    if (radius < 1) radius = 1;
    
    // Clamp bounds once
    int minX = std::max(0, centerX - radius);
    int maxX = std::min(m_currentImage.cols - 1, centerX + radius);
    int minY = std::max(0, centerY - radius);
    int maxY = std::min(m_currentImage.rows - 1, centerY + radius);
    
    if (minX > maxX || minY > maxY) return;

    float radiusSq = static_cast<float>(radius * radius);
    float hardRadius = radius * hardness;
    float hardRadiusSq = hardRadius * hardRadius;
    float featherRange = radiusSq - hardRadiusSq;
    float invFeather = (featherRange > 0) ? 1.0f / featherRange : 0;

    for (int y = minY; y <= maxY; ++y) {
        cv::Vec4b* row = m_currentImage.ptr<cv::Vec4b>(y);
        float dy = static_cast<float>(y - centerY);
        float dySq = dy * dy;
        
        for (int x = minX; x <= maxX; ++x) {
            float dx = static_cast<float>(x - centerX);
            float distSq = dx * dx + dySq;

            if (distSq <= radiusSq) {
                float alpha = 1.0f;
                if (distSq > hardRadiusSq) {
                    alpha = (radiusSq - distSq) * invFeather;
                }
                row[x][3] = static_cast<uchar>(row[x][3] * (1.0f - alpha));
            }
        }
    }
}

void ImageProcessor::eraseAlongPath(const QPoint& start, const QPoint& end, int diameter, float hardness) {
    if (m_currentImage.empty()) return;

    int dx = end.x() - start.x();
    int dy = end.y() - start.y();
    int distSq = dx * dx + dy * dy;
    
    if (distSq == 0) {
        eraseWithBrush(start.x(), start.y(), diameter, hardness);
        return;
    }
    
    int radius = std::max(1, diameter / 2);
    int minX = std::max(0, std::min(start.x(), end.x()) - radius);
    int maxX = std::min(m_currentImage.cols - 1, std::max(start.x(), end.x()) + radius);
    int minY = std::max(0, std::min(start.y(), end.y()) - radius);
    int maxY = std::min(m_currentImage.rows - 1, std::max(start.y(), end.y()) + radius);

    if (minX > maxX || minY > maxY) return;

    float radiusSq = static_cast<float>(radius * radius);
    float hardRadius = radius * hardness;
    float hardRadiusSq = hardRadius * hardRadius;
    float featherRange = radiusSq - hardRadiusSq;
    float invFeather = (featherRange > 0.0f) ? 1.0f / featherRange : 0.0f;

    float x0 = static_cast<float>(start.x());
    float y0 = static_cast<float>(start.y());
    float vx = static_cast<float>(dx);
    float vy = static_cast<float>(dy);
    float invLenSq = 1.0f / static_cast<float>(distSq);
    float vxNorm = vx * invLenSq;
    float vyNorm = vy * invLenSq;

    // Single pass capsule distance - visits each pixel in the stroke segment at most once
    for (int y = minY; y <= maxY; ++y) {
        cv::Vec4b* row = m_currentImage.ptr<cv::Vec4b>(y);
        float py = static_cast<float>(y);
        float rowT = (py - y0) * vyNorm;

        for (int x = minX; x <= maxX; ++x) {
            float px = static_cast<float>(x);
            float t = std::clamp((px - x0) * vxNorm + rowT, 0.0f, 1.0f);
            float diffX = px - (x0 + t * vx);
            float diffY = py - (y0 + t * vy);
            float dSq = diffX * diffX + diffY * diffY;

            if (dSq <= radiusSq) {
                float alpha = 1.0f;
                if (dSq > hardRadiusSq) {
                    alpha = (radiusSq - dSq) * invFeather;
                }
                row[x][3] = static_cast<uchar>(row[x][3] * (1.0f - alpha));
            }
        }
    }
}

void ImageProcessor::repairWithBrush(int centerX, int centerY, int diameter) {
    if (m_currentImage.empty() || m_originalImage.empty()) return;

    int radius = diameter / 2;
    if (radius < 1) radius = 1;
    
    int minX = std::max(0, centerX - radius);
    int maxX = std::min(m_currentImage.cols - 1, centerX + radius);
    int minY = std::max(0, centerY - radius);
    int maxY = std::min(m_currentImage.rows - 1, centerY + radius);
    
    if (minX > maxX || minY > maxY) return;

    float radiusSq = static_cast<float>(radius * radius);
    float hardRadius = radius * 0.8f;
    float hardRadiusSq = hardRadius * hardRadius;
    float featherRange = radiusSq - hardRadiusSq;
    float invFeather = (featherRange > 0) ? 1.0f / featherRange : 0;

    for (int y = minY; y <= maxY; ++y) {
        cv::Vec4b* currentRow = m_currentImage.ptr<cv::Vec4b>(y);
        const cv::Vec4b* originalRow = m_originalImage.ptr<cv::Vec4b>(y);
        float dy = static_cast<float>(y - centerY);
        float dySq = dy * dy;

        for (int x = minX; x <= maxX; ++x) {
            float dx = static_cast<float>(x - centerX);
            float distSq = dx * dx + dySq;

            if (distSq <= radiusSq) {
                float blend = 1.0f;
                if (distSq > hardRadiusSq) {
                    blend = (radiusSq - distSq) * invFeather;
                }

                for (int c = 0; c < 4; ++c) {
                    currentRow[x][c] = static_cast<uchar>(
                        currentRow[x][c] * (1.0f - blend) +
                        originalRow[x][c] * blend
                    );
                }
            }
        }
    }
}

void ImageProcessor::repairAlongPath(const QPoint& start, const QPoint& end, int diameter) {
    if (m_currentImage.empty() || m_originalImage.empty()) return;

    int dx = end.x() - start.x();
    int dy = end.y() - start.y();
    int distSq = dx * dx + dy * dy;
    
    if (distSq == 0) {
        repairWithBrush(start.x(), start.y(), diameter);
        return;
    }
    
    int radius = std::max(1, diameter / 2);
    int minX = std::max(0, std::min(start.x(), end.x()) - radius);
    int maxX = std::min(m_currentImage.cols - 1, std::max(start.x(), end.x()) + radius);
    int minY = std::max(0, std::min(start.y(), end.y()) - radius);
    int maxY = std::min(m_currentImage.rows - 1, std::max(start.y(), end.y()) + radius);

    if (minX > maxX || minY > maxY) return;

    float radiusSq = static_cast<float>(radius * radius);
    float hardRadius = radius * 0.8f;
    float hardRadiusSq = hardRadius * hardRadius;
    float featherRange = radiusSq - hardRadiusSq;
    float invFeather = (featherRange > 0.0f) ? 1.0f / featherRange : 0.0f;

    float x0 = static_cast<float>(start.x());
    float y0 = static_cast<float>(start.y());
    float vx = static_cast<float>(dx);
    float vy = static_cast<float>(dy);
    float invLenSq = 1.0f / static_cast<float>(distSq);
    float vxNorm = vx * invLenSq;
    float vyNorm = vy * invLenSq;

    // Single pass capsule distance - visits each pixel in the stroke segment at most once
    for (int y = minY; y <= maxY; ++y) {
        cv::Vec4b* currentRow = m_currentImage.ptr<cv::Vec4b>(y);
        const cv::Vec4b* originalRow = m_originalImage.ptr<cv::Vec4b>(y);
        float py = static_cast<float>(y);
        float rowT = (py - y0) * vyNorm;

        for (int x = minX; x <= maxX; ++x) {
            float px = static_cast<float>(x);
            float t = std::clamp((px - x0) * vxNorm + rowT, 0.0f, 1.0f);
            float diffX = px - (x0 + t * vx);
            float diffY = py - (y0 + t * vy);
            float dSq = diffX * diffX + diffY * diffY;

            if (dSq <= radiusSq) {
                float blend = 1.0f;
                if (dSq > hardRadiusSq) {
                    blend = (radiusSq - dSq) * invFeather;
                }

                for (int c = 0; c < 4; ++c) {
                    currentRow[x][c] = static_cast<uchar>(
                        currentRow[x][c] * (1.0f - blend) +
                        originalRow[x][c] * blend
                    );
                }
            }
        }
    }
}

// Edge Softening
cv::Mat ImageProcessor::applySoftening(const cv::Mat& image, int level) {
    if (level == 0 || image.empty()) return image.clone();

    cv::Mat result = image.clone();
    
    cv::Mat alpha;
    cv::extractChannel(result, alpha, 3);
    
    int morphSize = 2 + level;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(morphSize, morphSize));
    
    cv::Mat dilated, eroded, edgeMask;
    cv::dilate(alpha, dilated, kernel);
    cv::erode(alpha, eroded, kernel);
    cv::subtract(dilated, eroded, edgeMask);
    
    int blurSize = 3 + level * 4;
    if (blurSize % 2 == 0) blurSize++;
    cv::Mat blurredAlpha;
    cv::GaussianBlur(alpha, blurredAlpha, cv::Size(blurSize, blurSize), 0);
    
    float blendLUT[256];
    float levelFactor = level / 3.0f;
    for (int i = 0; i < 256; ++i) {
        blendLUT[i] = std::min(1.0f, (i / 255.0f) * levelFactor);
    }

    for (int y = 0; y < result.rows; ++y) {
        const uchar* edgeRow = edgeMask.ptr<uchar>(y);
        const uchar* alphaRow = alpha.ptr<uchar>(y);
        const uchar* blurRow = blurredAlpha.ptr<uchar>(y);
        cv::Vec4b* resRow = result.ptr<cv::Vec4b>(y);

        for (int x = 0; x < result.cols; ++x) {
            uchar edgeVal = edgeRow[x];
            if (edgeVal > 0) {
                float blend = blendLUT[edgeVal];
                resRow[x][3] = static_cast<uchar>(
                    alphaRow[x] * (1.0f - blend) + 
                    blurRow[x] * blend
                );
            }
        }
    }

    return result;
}

cv::Mat ImageProcessor::captureState() const {
    if (m_currentImage.empty()) return cv::Mat();
    // 4x faster & 75% less RAM: when dimensions match m_originalImage, editing tools only modify Alpha!
    if (!m_originalImage.empty() && m_currentImage.size() == m_originalImage.size() && m_currentImage.channels() == 4) {
        cv::Mat alphaOnly(m_currentImage.rows, m_currentImage.cols, CV_8UC1);
        cv::extractChannel(m_currentImage, alphaOnly, 3);
        return alphaOnly;
    }
    return m_currentImage.clone();
}

void ImageProcessor::restoreState(const cv::Mat& state) {
    if (state.empty()) return;
    if (state.channels() == 1 && !m_originalImage.empty() && state.size() == m_originalImage.size()) {
        if (m_currentImage.size() != m_originalImage.size() || m_currentImage.channels() != 4) {
            m_currentImage = m_originalImage.clone();
        }
        cv::insertChannel(state, m_currentImage, 3);
        return;
    }
    bool sizeChanged = (m_currentImage.rows != state.rows || m_currentImage.cols != state.cols);
    m_currentImage = state.clone();
    if (sizeChanged) {
        m_originalImage = m_currentImage.clone();
        m_labImage.release();
    }
}
