#include "ImageProcessor.h"
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <stack>
#include <thread>

ImageProcessor::ImageProcessor() = default;
ImageProcessor::~ImageProcessor() = default;

ImageProcessor::LoadedImageData ImageProcessor::decodeImageFile(const QString& path, bool buildDisplayCaches) {
    LoadedImageData res;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return res;
    }
    QByteArray raw = f.readAll();
    if (raw.isEmpty()) {
        return res;
    }

    // Zero-copy wrapper around QByteArray for cv::imdecode (avoids extra multi-MB vector copy)
    cv::Mat rawBuf(1, static_cast<int>(raw.size()), CV_8UC1, raw.data());
    res.originalImage = cv::imdecode(rawBuf, cv::IMREAD_UNCHANGED);
    if (res.originalImage.empty()) {
        return res;
    }

    ensureAlphaChannel(res.originalImage);
    res.currentImage = res.originalImage.clone();

    const int H = res.originalImage.rows;
    const int W = res.originalImage.cols;
    res.initialAlpha.create(H, W, CV_8UC1);

    // Single pass: extract initial alpha for undo + detect & restore Self-Remembering PNG RGB
    int transparentWithRgbCount = 0;
    int transparentCount = 0;
    uchar* const origBase = res.originalImage.data;
    const size_t origStep = res.originalImage.step;
    uchar* const alphaBase = res.initialAlpha.data;
    const size_t alphaStep = res.initialAlpha.step;

    for (int y = 0; y < H; ++y) {
        uint32_t* row32 = reinterpret_cast<uint32_t*>(origBase + static_cast<size_t>(y) * origStep);
        uchar* aRow = alphaBase + static_cast<size_t>(y) * alphaStep;
        for (int x = 0; x < W; ++x) {
            const uint32_t px = row32[x];
            const uint32_t a = px >> 24;
            aRow[x] = static_cast<uchar>(a);
            if (a < 255u) {
                transparentCount++;
                if ((px & 0x00FFFFFFu) != 0u) {
                    transparentWithRgbCount++;
                }
                row32[x] = px | 0xFF000000u; // Reveal hidden background RGB for Repair [R] / Compare [H] / Restore
            }
        }
    }
    res.hasEmbeddedOriginal = (transparentCount > 0 && transparentWithRgbCount > 0);

    if (buildDisplayCaches) {
        res.displayPremul = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
        fillPremulDisplayRegion(res.currentImage, res.displayPremul, QRect(0, 0, W, H));
        if (static_cast<qint64>(W) * H > 2000000) {
            const int mw = std::max(1, W / 4);
            const int mh = std::max(1, H / 4);
            cv::Mat srcView(H, W, CV_8UC4, const_cast<uchar*>(res.displayPremul.constBits()),
                            static_cast<size_t>(res.displayPremul.bytesPerLine()));
            res.displayMip4 = QImage(mw, mh, QImage::Format_ARGB32_Premultiplied);
            cv::Mat dstView(mh, mw, CV_8UC4, res.displayMip4.bits(),
                            static_cast<size_t>(res.displayMip4.bytesPerLine()));
            cv::resize(srcView, dstView, cv::Size(mw, mh), 0, 0, cv::INTER_AREA);
        }
    }

    res.ok = true;
    return res;
}

void ImageProcessor::applyLoadedData(LoadedImageData&& data) {
    m_originalImage = std::move(data.originalImage);
    m_currentImage = std::move(data.currentImage);
    m_hasEmbeddedOriginal = data.hasEmbeddedOriginal;
    // Lazy LAB conversion: defer BGR->Lab until Auto Color Remove is actually used
    m_labImage.release();
}

bool ImageProcessor::loadImage(const QString& path) {
    LoadedImageData data = decodeImageFile(path, false);
    if (!data.ok) {
        return false;
    }
    applyLoadedData(std::move(data));
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

bool ImageProcessor::exportImageData(const cv::Mat& currentImg, const cv::Mat& originalImg,
                                     const QString& path, int edgeSoftenLevel) {
    if (currentImg.empty()) return false;

    cv::Mat softened = (edgeSoftenLevel > 0)
        ? applySoftening(currentImg, edgeSoftenLevel)
        : currentImg;

    // Self-Remembering PNG: embed the full original (B, G, R) channels under every pixel
    // while keeping the exact cutout Alpha channel
    cv::Mat exportImage;
    if (!originalImg.empty() && originalImg.size() == softened.size() && originalImg.channels() == 4) {
        exportImage = originalImg.clone();
        cv::Mat alphaChan;
        cv::extractChannel(softened, alphaChan, 3);
        cv::insertChannel(alphaChan, exportImage, 3);
    } else {
        exportImage = softened;
    }

    const QString ext = QFileInfo(path).suffix().toLower();
    std::vector<uchar> encoded;

    if (ext == "jpg" || ext == "jpeg" || ext == "bmp") {
        // Formats without alpha: cleanly composite onto solid white so transparent areas never leak original pixels
        cv::Mat bgr(softened.rows, softened.cols, CV_8UC3);
        for (int y = 0; y < softened.rows; ++y) {
            const cv::Vec4b* srcRow = softened.ptr<cv::Vec4b>(y);
            cv::Vec3b* dstRow = bgr.ptr<cv::Vec3b>(y);
            for (int x = 0; x < softened.cols; ++x) {
                const uint32_t a = srcRow[x][3];
                const uint32_t invA = 255u - a;
                dstRow[x][0] = static_cast<uchar>((srcRow[x][0] * a + 255u * invA + 127u) / 255u);
                dstRow[x][1] = static_cast<uchar>((srcRow[x][1] * a + 255u * invA + 127u) / 255u);
                dstRow[x][2] = static_cast<uchar>((srcRow[x][2] * a + 255u * invA + 127u) / 255u);
            }
        }
        if (ext == "bmp") {
            if (!cv::imencode(".bmp", bgr, encoded)) return false;
        } else {
            std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 95};
            if (!cv::imencode(".jpg", bgr, encoded, params)) return false;
        }
    } else if (ext == "webp") {
        std::vector<int> params = {cv::IMWRITE_WEBP_QUALITY, 101};
        if (!cv::imencode(".webp", exportImage, encoded, params)) return false;
    } else if (ext == "tiff" || ext == "tif") {
        if (!cv::imencode(".tiff", exportImage, encoded)) return false;
    } else {
        // Fast zlib compression level 2: 3-5x faster export on large 4K/8K Self-Remembering PNGs while preserving exact lossless RGBA
        std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 2};
        if (!cv::imencode(".png", exportImage, encoded, params)) return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(reinterpret_cast<const char*>(encoded.data()), static_cast<qint64>(encoded.size())) == static_cast<qint64>(encoded.size());
}

bool ImageProcessor::exportImage(const QString& path, int edgeSoftenLevel) {
    return exportImageData(m_currentImage, m_originalImage, path, edgeSoftenLevel);
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

void ImageProcessor::fillPremulDisplayRegion(const cv::Mat& srcBgra, QImage& target, const QRect& region) {
    if (srcBgra.empty() || target.isNull()) return;
    int x1 = std::max(0, region.left());
    int y1 = std::max(0, region.top());
    int x2 = std::min(srcBgra.cols, region.right() + 1);
    int y2 = std::min(srcBgra.rows, region.bottom() + 1);
    if (x1 >= x2 || y1 >= y2) return;

    const uchar* const srcBase = srcBgra.data;
    const size_t srcStep = srcBgra.step;
    uchar* const dstBase = target.bits();
    const int dstStride = target.bytesPerLine();

    if (target.format() == QImage::Format_ARGB32_Premultiplied) {
        const int count = x2 - x1;
        for (int y = y1; y < y2; ++y) {
            const uint32_t* src32 = reinterpret_cast<const uint32_t*>(srcBase + static_cast<size_t>(y) * srcStep) + x1;
            uint32_t* dst32 = reinterpret_cast<uint32_t*>(dstBase + static_cast<size_t>(y) * dstStride) + x1;
            for (int i = 0; i < count; ++i) {
                const uint32_t p = src32[i];
                const uint32_t a = p >> 24;
                if (a == 255u) {
                    dst32[i] = p;
                } else if (a == 0u) {
                    dst32[i] = 0u;
                } else {
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

    if (target.format() == QImage::Format_ARGB32) {
        const size_t rowBytes = static_cast<size_t>(x2 - x1) * 4;
        for (int y = y1; y < y2; ++y) {
            std::memcpy(dstBase + static_cast<size_t>(y) * dstStride + x1 * 4,
                        srcBase + static_cast<size_t>(y) * srcStep + x1 * 4, rowBytes);
        }
        return;
    }

    for (int y = y1; y < y2; ++y) {
        const cv::Vec4b* srcRow = reinterpret_cast<const cv::Vec4b*>(srcBase + static_cast<size_t>(y) * srcStep);
        uchar* dstRow = dstBase + static_cast<size_t>(y) * dstStride;
        for (int x = x1; x < x2; ++x) {
            const cv::Vec4b& src = srcRow[x];
            int dstIdx = x * 4;
            dstRow[dstIdx + 0] = src[2];
            dstRow[dstIdx + 1] = src[1];
            dstRow[dstIdx + 2] = src[0];
            dstRow[dstIdx + 3] = src[3];
        }
    }
}

void ImageProcessor::updateDisplayRegion(QImage& target, const QRect& region) const {
    fillPremulDisplayRegion(m_currentImage, target, region);
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
void ImageProcessor::autoColorRemove(int x, int y, int tolerance, const QRect& viewportBounds, QRect* outDirtyRect) {
    if (outDirtyRect) *outDirtyRect = QRect();
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

    int modMinX = maxX + 1, modMinY = maxY + 1, modMaxX = minX - 1, modMaxY = minY - 1;

    uchar* const curBase = m_currentImage.data;
    const size_t curStep = m_currentImage.step;
    const uchar* const labBase = m_labImage.data;
    const size_t labStep = m_labImage.step;
    uchar* const visBase = visited.data;
    const size_t visStep = visited.step;

    while (queuePos < queue.size()) {
        const cv::Point pt = queue[queuePos++];

        cv::Vec4b* curRow = reinterpret_cast<cv::Vec4b*>(curBase + static_cast<size_t>(pt.y) * curStep);
        cv::Vec4b& pixel = curRow[pt.x];
        if (pixel[3] == 0) continue;

        const cv::Vec3b* labRow = reinterpret_cast<const cv::Vec3b*>(labBase + static_cast<size_t>(pt.y) * labStep);
        const cv::Vec3b& pixelLab = labRow[pt.x];

        const float dL = static_cast<float>(pixelLab[0]) - seedL;
        const float da = static_cast<float>(pixelLab[1]) - seedA;
        const float db = static_cast<float>(pixelLab[2]) - seedB;
        const float deltaESq = dL * dL + da * da + db * db;

        if (deltaESq > maxDeltaESq) continue;

        pixel[3] = 0;
        if (pt.x < modMinX) modMinX = pt.x;
        if (pt.x > modMaxX) modMaxX = pt.x;
        if (pt.y < modMinY) modMinY = pt.y;
        if (pt.y > modMaxY) modMaxY = pt.y;

        const int ly = pt.y - minY;
        const int lx = pt.x - minX;
        uchar* visRow = visBase + static_cast<size_t>(ly) * visStep;
        if (pt.x > minX && !visRow[lx - 1]) {
            visRow[lx - 1] = 1;
            queue.emplace_back(pt.x - 1, pt.y);
        }
        if (pt.x < maxX && !visRow[lx + 1]) {
            visRow[lx + 1] = 1;
            queue.emplace_back(pt.x + 1, pt.y);
        }
        if (pt.y > minY) {
            uchar* topVisRow = visRow - visStep;
            if (!topVisRow[lx]) {
                topVisRow[lx] = 1;
                queue.emplace_back(pt.x, pt.y - 1);
            }
        }
        if (pt.y < maxY) {
            uchar* botVisRow = visRow + visStep;
            if (!botVisRow[lx]) {
                botVisRow[lx] = 1;
                queue.emplace_back(pt.x, pt.y + 1);
            }
        }
    }

    if (outDirtyRect && modMaxX >= modMinX && modMaxY >= modMinY) {
        *outDirtyRect = QRect(modMinX, modMinY, modMaxX - modMinX + 1, modMaxY - modMinY + 1);
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

    const float radiusF = std::max(0.5f, diameter * 0.5f);
    const int boundR = static_cast<int>(std::ceil(radiusF));

    // Clamp bounds once
    int minX = std::max(0, centerX - boundR);
    int maxX = std::min(m_currentImage.cols - 1, centerX + boundR);
    int minY = std::max(0, centerY - boundR);
    int maxY = std::min(m_currentImage.rows - 1, centerY + boundR);

    if (minX > maxX || minY > maxY) return;

    const float radiusSq = radiusF * radiusF;
    const float hardRadius = radiusF * hardness;
    const float hardRadiusSq = hardRadius * hardRadius;
    const float featherRange = radiusSq - hardRadiusSq;
    const float invFeather = (featherRange > 1e-6f) ? 1.0f / featherRange : 0.0f;

    for (int y = minY; y <= maxY; ++y) {
        cv::Vec4b* row = m_currentImage.ptr<cv::Vec4b>(y);
        float dy = static_cast<float>(y - centerY);
        float dySq = dy * dy;

        for (int x = minX; x <= maxX; ++x) {
            float dx = static_cast<float>(x - centerX);
            float distSq = dx * dx + dySq;

            if (distSq <= radiusSq) {
                float alpha = 1.0f;
                if (distSq > hardRadiusSq && invFeather > 0.0f) {
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

    const float radiusF = std::max(0.5f, diameter * 0.5f);
    const int boundR = static_cast<int>(std::ceil(radiusF));
    int minX = std::max(0, std::min(start.x(), end.x()) - boundR);
    int maxX = std::min(m_currentImage.cols - 1, std::max(start.x(), end.x()) + boundR);
    int minY = std::max(0, std::min(start.y(), end.y()) - boundR);
    int maxY = std::min(m_currentImage.rows - 1, std::max(start.y(), end.y()) + boundR);

    if (minX > maxX || minY > maxY) return;

    const float radiusSq = radiusF * radiusF;
    const float hardRadius = radiusF * hardness;
    const float hardRadiusSq = hardRadius * hardRadius;
    const float featherRange = radiusSq - hardRadiusSq;
    const float invFeather = (featherRange > 1e-6f) ? 1.0f / featherRange : 0.0f;

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
                if (dSq > hardRadiusSq && invFeather > 0.0f) {
                    alpha = (radiusSq - dSq) * invFeather;
                }
                row[x][3] = static_cast<uchar>(row[x][3] * (1.0f - alpha));
            }
        }
    }
}

void ImageProcessor::repairWithBrush(int centerX, int centerY, int diameter) {
    if (m_currentImage.empty() || m_originalImage.empty()) return;

    const float radiusF = std::max(0.5f, diameter * 0.5f);
    const int boundR = static_cast<int>(std::ceil(radiusF));

    int minX = std::max(0, centerX - boundR);
    int maxX = std::min(m_currentImage.cols - 1, centerX + boundR);
    int minY = std::max(0, centerY - boundR);
    int maxY = std::min(m_currentImage.rows - 1, centerY + boundR);

    if (minX > maxX || minY > maxY) return;

    const float radiusSq = radiusF * radiusF;
    const float hardRadius = radiusF * 0.8f;
    const float hardRadiusSq = hardRadius * hardRadius;
    const float featherRange = radiusSq - hardRadiusSq;
    const float invFeather = (featherRange > 1e-6f) ? 1.0f / featherRange : 0.0f;

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
                if (distSq > hardRadiusSq && invFeather > 0.0f) {
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

    const float radiusF = std::max(0.5f, diameter * 0.5f);
    const int boundR = static_cast<int>(std::ceil(radiusF));
    int minX = std::max(0, std::min(start.x(), end.x()) - boundR);
    int maxX = std::min(m_currentImage.cols - 1, std::max(start.x(), end.x()) + boundR);
    int minY = std::max(0, std::min(start.y(), end.y()) - boundR);
    int maxY = std::min(m_currentImage.rows - 1, std::max(start.y(), end.y()) + boundR);

    if (minX > maxX || minY > maxY) return;

    const float radiusSq = radiusF * radiusF;
    const float hardRadius = radiusF * 0.8f;
    const float hardRadiusSq = hardRadius * hardRadius;
    const float featherRange = radiusSq - hardRadiusSq;
    const float invFeather = (featherRange > 1e-6f) ? 1.0f / featherRange : 0.0f;

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
                if (dSq > hardRadiusSq && invFeather > 0.0f) {
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
