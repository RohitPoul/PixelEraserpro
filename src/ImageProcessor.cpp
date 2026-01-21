#include "ImageProcessor.h"
#include <QFile>
#include <cmath>
#include <algorithm>
#include <stack>
#include <thread>

ImageProcessor::ImageProcessor() = default;
ImageProcessor::~ImageProcessor() = default;

bool ImageProcessor::loadImage(const QString& path) {
    std::string stdPath = path.toStdString();
    m_originalImage = cv::imread(stdPath, cv::IMREAD_UNCHANGED);

    if (m_originalImage.empty()) {
        return false;
    }

    ensureAlphaChannel(m_originalImage);
    m_currentImage = m_originalImage.clone();
    
    // Pre-convert to LAB for smart color matching
    cv::Mat bgr;
    cv::cvtColor(m_currentImage, bgr, cv::COLOR_BGRA2BGR);
    cv::cvtColor(bgr, m_labImage, cv::COLOR_BGR2Lab);

    return true;
}

void ImageProcessor::updateLabCache() {
    if (m_currentImage.empty()) return;
    cv::Mat bgr;
    cv::cvtColor(m_currentImage, bgr, cv::COLOR_BGRA2BGR);
    cv::cvtColor(bgr, m_labImage, cv::COLOR_BGR2Lab);
}

void ImageProcessor::resize(int newWidth, int newHeight) {
    if (m_currentImage.empty()) return;
    
    cv::Mat resized;
    cv::resize(m_currentImage, resized, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LANCZOS4);
    m_currentImage = resized;
    
    cv::resize(m_originalImage, resized, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LANCZOS4);
    m_originalImage = resized;
    
    updateLabCache();
}

void ImageProcessor::updateOriginalImage() {
    if (m_currentImage.empty()) return;
    m_originalImage = m_currentImage.clone();
    updateLabCache();  // Sync LAB cache for Auto Color Remove tool
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
    if (m_currentImage.empty()) return false;
    std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 6};
    return cv::imwrite(path.toStdString(), m_currentImage, params);
}

bool ImageProcessor::exportImage(const QString& path, int edgeSoftenLevel) {
    if (m_currentImage.empty()) return false;

    cv::Mat exportImage = m_currentImage.clone();
    
    if (edgeSoftenLevel > 0) {
        exportImage = applySoftening(exportImage, edgeSoftenLevel);
    }

    std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 6};
    return cv::imwrite(path.toStdString(), exportImage, params);
}

QImage ImageProcessor::getDisplayImage() const {
    if (m_currentImage.empty()) return QImage();
    
    cv::Mat rgba;
    cv::cvtColor(m_currentImage, rgba, cv::COLOR_BGRA2RGBA);
    return QImage(rgba.data, rgba.cols, rgba.rows,
                 static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
}

void ImageProcessor::updateDisplayRegion(QImage& target, const QRect& region) const {
    if (m_currentImage.empty() || target.isNull()) return;
    
    // Clamp region to image bounds
    int x1 = std::max(0, region.left());
    int y1 = std::max(0, region.top());
    int x2 = std::min(m_currentImage.cols, region.right() + 1);
    int y2 = std::min(m_currentImage.rows, region.bottom() + 1);
    
    if (x1 >= x2 || y1 >= y2) return;
    
    // Direct pixel copy with color conversion (BGRA -> RGBA)
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
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows, 
                     static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    } else if (mat.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows,
                     static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    }
    return QImage();
}

// Smart color matching using LAB color space - perceptually accurate
void ImageProcessor::autoColorRemove(int x, int y, int tolerance, const QRect& viewportBounds) {
    if (m_currentImage.empty() || m_labImage.empty()) return;
    if (x < 0 || x >= m_currentImage.cols || y < 0 || y >= m_currentImage.rows) return;

    cv::Vec4b seedBGRA = m_currentImage.at<cv::Vec4b>(y, x);
    if (seedBGRA[3] == 0) return;
    
    cv::Vec3b seedLab = m_labImage.at<cv::Vec3b>(y, x);

    int minX = viewportBounds.isValid() ? std::max(0, viewportBounds.left()) : 0;
    int minY = viewportBounds.isValid() ? std::max(0, viewportBounds.top()) : 0;
    int maxX = viewportBounds.isValid() ? std::min(m_currentImage.cols - 1, viewportBounds.right()) : m_currentImage.cols - 1;
    int maxY = viewportBounds.isValid() ? std::min(m_currentImage.rows - 1, viewportBounds.bottom()) : m_currentImage.rows - 1;

    cv::Mat visited = cv::Mat::zeros(m_currentImage.rows, m_currentImage.cols, CV_8UC1);

    // Use vector instead of stack for better memory locality
    std::vector<cv::Point> queue;
    queue.reserve(10000);
    queue.push_back(cv::Point(x, y));
    
    int queuePos = 0;
    float maxDeltaE = static_cast<float>(tolerance);
    float maxDeltaESq = maxDeltaE * maxDeltaE; // Use squared distance to avoid sqrt

    // Pre-calculate seed values
    float seedL = static_cast<float>(seedLab[0]);
    float seedA = static_cast<float>(seedLab[1]);
    float seedB = static_cast<float>(seedLab[2]);

    while (queuePos < queue.size()) {
        cv::Point pt = queue[queuePos++];

        if (pt.x < minX || pt.x > maxX || pt.y < minY || pt.y > maxY) continue;
        if (visited.at<uchar>(pt.y, pt.x)) continue;

        cv::Vec4b& pixel = m_currentImage.at<cv::Vec4b>(pt.y, pt.x);
        if (pixel[3] == 0) continue;
        
        cv::Vec3b pixelLab = m_labImage.at<cv::Vec3b>(pt.y, pt.x);
        
        // Fast squared deltaE (skip sqrt)
        float dL = static_cast<float>(pixelLab[0]) - seedL;
        float da = static_cast<float>(pixelLab[1]) - seedA;
        float db = static_cast<float>(pixelLab[2]) - seedB;
        float deltaESq = dL*dL + da*da + db*db;
        
        if (deltaESq > maxDeltaESq) continue;

        visited.at<uchar>(pt.y, pt.x) = 1;
        pixel[3] = 0; // Make transparent

        // Add neighbors (4-connected)
        if (pt.x > minX) queue.push_back(cv::Point(pt.x - 1, pt.y));
        if (pt.x < maxX) queue.push_back(cv::Point(pt.x + 1, pt.y));
        if (pt.y > minY) queue.push_back(cv::Point(pt.x, pt.y - 1));
        if (pt.y < maxY) queue.push_back(cv::Point(pt.x, pt.y + 1));
    }
    
    updateLabCache();
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
    int dx = end.x() - start.x();
    int dy = end.y() - start.y();
    int distSq = dx*dx + dy*dy;
    
    if (distSq == 0) {
        eraseWithBrush(start.x(), start.y(), diameter, hardness);
        return;
    }
    
    float distance = std::sqrt(static_cast<float>(distSq));
    int radius = diameter / 2;
    if (radius < 1) radius = 1;
    
    // Adaptive step - smaller steps for small brushes
    float stepSize = std::max(1.0f, radius * 0.3f);
    int steps = static_cast<int>(distance / stepSize) + 1;

    float stepX = static_cast<float>(dx) / steps;
    float stepY = static_cast<float>(dy) / steps;

    for (int i = 0; i <= steps; ++i) {
        int x = start.x() + static_cast<int>(i * stepX);
        int y = start.y() + static_cast<int>(i * stepY);
        eraseWithBrush(x, y, diameter, hardness);
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
    int dx = end.x() - start.x();
    int dy = end.y() - start.y();
    int distSq = dx*dx + dy*dy;
    
    if (distSq == 0) {
        repairWithBrush(start.x(), start.y(), diameter);
        return;
    }
    
    float distance = std::sqrt(static_cast<float>(distSq));
    int radius = diameter / 2;
    if (radius < 1) radius = 1;
    
    float stepSize = std::max(1.0f, radius * 0.3f);
    int steps = static_cast<int>(distance / stepSize) + 1;

    float stepX = static_cast<float>(dx) / steps;
    float stepY = static_cast<float>(dy) / steps;

    for (int i = 0; i <= steps; ++i) {
        int x = start.x() + static_cast<int>(i * stepX);
        int y = start.y() + static_cast<int>(i * stepY);
        repairWithBrush(x, y, diameter);
    }
}

// Edge Softening
cv::Mat ImageProcessor::applySoftening(const cv::Mat& image, int level) {
    if (level == 0 || image.empty()) return image.clone();

    cv::Mat result = image.clone();
    
    std::vector<cv::Mat> channels;
    cv::split(result, channels);
    cv::Mat alpha = channels[3].clone();
    
    int morphSize = 2 + level;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(morphSize, morphSize));
    
    cv::Mat dilated, eroded;
    cv::dilate(alpha, dilated, kernel);
    cv::erode(alpha, eroded, kernel);
    cv::Mat edgeMask = dilated - eroded;
    
    int blurSize = 3 + level * 4;
    if (blurSize % 2 == 0) blurSize++;
    cv::Mat blurredAlpha;
    cv::GaussianBlur(alpha, blurredAlpha, cv::Size(blurSize, blurSize), 0);
    
    for (int y = 0; y < result.rows; ++y) {
        for (int x = 0; x < result.cols; ++x) {
            uchar edgeVal = edgeMask.at<uchar>(y, x);
            if (edgeVal > 0) {
                float blend = (edgeVal / 255.0f) * (level / 3.0f);
                blend = std::min(1.0f, blend);
                result.at<cv::Vec4b>(y, x)[3] = static_cast<uchar>(
                    alpha.at<uchar>(y, x) * (1.0f - blend) + 
                    blurredAlpha.at<uchar>(y, x) * blend
                );
            }
        }
    }

    return result;
}

cv::Mat ImageProcessor::captureState() const {
    return m_currentImage.clone();
}

void ImageProcessor::restoreState(const cv::Mat& state) {
    m_currentImage = state.clone();
    updateLabCache();
}
