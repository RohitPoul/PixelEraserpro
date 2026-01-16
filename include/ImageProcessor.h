#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <opencv2/opencv.hpp>
#include <QImage>
#include <QString>
#include <QRect>
#include <memory>
#include <functional>

class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();

    // File operations
    bool loadImage(const QString& path);
    bool saveImage(const QString& path);
    bool exportImage(const QString& path, int edgeSoftenLevel = 0);

    // Image access
    QImage getDisplayImage() const;
    QImage getOriginalAsQImage() const;
    void updateDisplayRegion(QImage& target, const QRect& region) const;
    cv::Mat& getCurrentImage() { return m_currentImage; }
    const cv::Mat& getCurrentImage() const { return m_currentImage; }
    const cv::Mat& getOriginalImage() const { return m_originalImage; }

    // Image info
    int getWidth() const { return m_currentImage.cols; }
    int getHeight() const { return m_currentImage.rows; }
    bool hasImage() const { return !m_currentImage.empty(); }
    
    // Image operations
    void resize(int newWidth, int newHeight);
    void clear();

    // Tools - viewport bounded
    void autoColorRemove(int x, int y, int tolerance, const QRect& viewportBounds);
    void eraseWithBrush(int x, int y, int radius, float hardness = 0.8f);
    void eraseAlongPath(const QPoint& start, const QPoint& end, int radius, float hardness = 0.8f);
    void repairWithBrush(int x, int y, int radius);
    void repairAlongPath(const QPoint& start, const QPoint& end, int radius);

    // Edge softening for export
    cv::Mat applySoftening(const cv::Mat& image, int level);

    // State management
    cv::Mat captureState() const;
    void restoreState(const cv::Mat& state);

    // Progress callback for long operations
    using ProgressCallback = std::function<void(int percent)>;
    void setProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }

private:
    void ensureAlphaChannel(cv::Mat& image);
    QImage matToQImage(const cv::Mat& mat) const;
    bool colorMatches(const cv::Vec4b& c1, const cv::Vec4b& c2, int tolerance) const;

    cv::Mat m_originalImage;
    cv::Mat m_currentImage;
    cv::Mat m_labImage;  // LAB color space cache for smart color matching
    QImage m_qImageCache;
    
    void updateLabCache();

    ProgressCallback m_progressCallback;
};

#endif // IMAGEPROCESSOR_H
