#ifndef UPSCALER_H
#define UPSCALER_H

#include <QString>
#include <QObject>
#include <opencv2/opencv.hpp>
#include <memory>

class Upscaler : public QObject {
    Q_OBJECT

public:
    enum Model {
        RealESRGAN_x2,       // 2x upscaling - faster
        RealESRGAN_x4,       // 4x upscaling - best quality
        RealESRGAN_x4_anime  // 4x upscaling - optimized for anime/illustrations
    };

    explicit Upscaler(QObject* parent = nullptr);
    ~Upscaler();

    // Check if model is downloaded
    bool isModelAvailable(Model model);
    
    // Download model if not available - callback receives (bytesReceived, bytesTotal)
    bool downloadModel(Model model, std::function<void(qint64, qint64)> progressCallback = nullptr);
    
    // Upscale image
    cv::Mat upscale(const cv::Mat& input, Model model, int scale = 4);
    
    // Get model info
    static QString getModelName(Model model);
    static QString getModelDescription(Model model);
    static QString getModelUrl(Model model);
    static int getModelScale(Model model);

signals:
    void progressChanged(int percent);
    void error(const QString& message);

private:
    QString getModelPath(Model model);
    bool loadModel(Model model);
    void unloadModel();
    
    struct OnnxSession;
    std::unique_ptr<OnnxSession> m_session;
    Model m_currentModel = RealESRGAN_x4;
};

#endif // UPSCALER_H
