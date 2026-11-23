#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <opencv2/opencv.hpp>
#include <QImage>
#include <QString>
#include <QRect>
#include <QSize>
#include <memory>
#include <functional>

class ImageProcessor {
public:
    enum MaskApplyStatus {
        MaskSuccessLuminance,      // Applied from grayscale / B&W boundary mask
        MaskSuccessAlpha,          // Applied from transparent RGBA cutout image
        MaskSuccessEmbeddedRgb,    // Injected original RGB memory into an existing transparent cutout
        MaskErrorLoadFailed,       // File could not be read
        MaskErrorDimensionMismatch // Mask dimensions != current image dimensions
    };

    struct LoadedImageData {
        cv::Mat originalImage;
        cv::Mat currentImage;
        cv::Mat initialAlpha;
        QImage displayPremul;
        QImage displayMip4;
        bool hasEmbeddedOriginal = false;
        bool ok = false;
    };

    ImageProcessor();
    ~ImageProcessor();

    // File operations (sync + thread-safe static worker paths for non-blocking UI)
    bool loadImage(const QString& path);
    static LoadedImageData decodeImageFile(const QString& path, bool buildDisplayCaches = false);
    void applyLoadedData(LoadedImageData&& data);

    bool saveImage(const QString& path);
    bool exportImage(const QString& path, int edgeSoftenLevel = 0);
    static bool exportImageData(const cv::Mat& currentImg, const cv::Mat& originalImg,
                                const QString& path, int edgeSoftenLevel = 0);

    // Apply external mask or transparent cutout image (requires matching width x height)
    MaskApplyStatus applyExternalMask(const QString& maskPath, QSize* outMaskSize = nullptr);

    // Self-remembering PNG support: restore full un-erased original RGB (alpha = 255)
    bool hasEmbeddedOriginal() const { return m_hasEmbeddedOriginal; }
    void restoreFullOriginal();

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
    bool isPixelTransparent(int x, int y) const;
    
    // Image operations
    void resize(int newWidth, int newHeight);
    void updateOriginalImage();  // Sync original with current (after upscale)
    void clear();

    // Tools - viewport bounded
    void autoColorRemove(int x, int y, int tolerance, const QRect& viewportBounds, QRect* outDirtyRect = nullptr);
    void eraseWithBrush(int x, int y, int radius, float hardness = 0.8f);
    void eraseAlongPath(const QPoint& start, const QPoint& end, int radius, float hardness = 0.8f);
    void repairWithBrush(int x, int y, int radius);
    void repairAlongPath(const QPoint& start, const QPoint& end, int radius);

    // Edge softening for export
    static cv::Mat applySoftening(const cv::Mat& image, int level);

    // State management
    cv::Mat captureState() const;
    void restoreState(const cv::Mat& state);

    // Progress callback for long operations
    using ProgressCallback = std::function<void(int percent)>;
    void setProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }

private:
    static void ensureAlphaChannel(cv::Mat& image);
    static void fillPremulDisplayRegion(const cv::Mat& srcBgra, QImage& target, const QRect& region);
    QImage matToQImage(const cv::Mat& mat) const;
    bool colorMatches(const cv::Vec4b& c1, const cv::Vec4b& c2, int tolerance) const;

    cv::Mat m_originalImage;
    cv::Mat m_currentImage;
    cv::Mat m_labImage;  // LAB color space cache for smart color matching
    QImage m_qImageCache;
    bool m_hasEmbeddedOriginal = false;
    
    void updateLabCache();

    ProgressCallback m_progressCallback;
};

#endif // IMAGEPROCESSOR_H
