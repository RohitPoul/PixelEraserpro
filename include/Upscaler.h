#ifndef UPSCALER_H
#define UPSCALER_H

#include <QString>
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <opencv2/opencv.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace OnnxRuntimeHelper {
struct SessionBundle;
}

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

    // Pause / Resume / Cancel controls (thread-safe, callable from UI thread)
    void setPaused(bool paused);
    bool isPaused() const { return m_paused.load(); }
    void requestCancel();
    bool isCancelled() const { return m_cancelled.load(); }
    void resetControlFlags();

    // Upscale image (supports live pause/resume & cancellation)
    cv::Mat upscale(const cv::Mat& input, Model model, int scale = 4);
    QString activeBackend() const;

    // Get model info
    static QString getModelName(Model model);
    static QString getModelDescription(Model model);
    static QString getModelUrl(Model model);
    static int getModelScale(Model model);

signals:
    void progressChanged(int percent);
    void statusChanged(const QString& statusText);
    void progressStageChanged(int basePercent, int nextPercent, const QString& statusText);
    void backendChanged(const QString& backendLabel);
    void error(const QString& message);

private:
    QString getModelPath(Model model);
    bool loadModel(Model model, bool allowGpu = true);
    void unloadModel();
    bool waitIfPaused();

    std::unique_ptr<OnnxRuntimeHelper::SessionBundle> m_session;
    Model m_currentModel = RealESRGAN_x4;

    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_cancelled{false};
    mutable QMutex m_pauseMutex;
    QWaitCondition m_pauseCond;
    std::mutex m_terminateMutex;
    void (*m_activeTerminateFn)(void*) = nullptr;
    void* m_activeTerminateCtx = nullptr;
};

#endif // UPSCALER_H
