#ifndef BACKGROUNDREMOVER_H
#define BACKGROUNDREMOVER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <opencv2/opencv.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <functional>

namespace OnnxRuntimeHelper {
struct SessionBundle;
}

class BackgroundRemover : public QObject {
    Q_OBJECT

public:
    struct Settings {
        int tileSize = 0;           // 0 = Auto (1536 native), 1024 (zoomed detail crop), 1536, 2048 (wide crop)
        bool useGlobalAnchor = true;
        bool enableTiling = true;   // false = single 1536px global pass (lowest memory & fastest)
        float overlap = 0.35f;      // 0.10 .. 0.75
        bool ttaHFlip = false;      // Horizontal-flip test-time augmentation (2x passes)
        float deepCleanEps = 0.02f; // 0.00 .. 0.05 edge-only deep clean threshold
    };

    explicit BackgroundRemover(QObject* parent = nullptr);
    ~BackgroundRemover();

    // Locate or check if the G4 ONNX model is available locally
    bool isModelAvailable() const;
    QString getModelPath() const;

    // Optional download fallback if g4.onnx is not present locally
    bool downloadModel(std::function<void(qint64, qint64)> progressCallback = nullptr);

    // Pause / Resume / Cancel controls (thread-safe, callable from UI thread)
    void setPaused(bool paused);
    bool isPaused() const { return m_paused.load(); }
    void requestCancel();
    bool isCancelled() const { return m_cancelled.load(); }
    void resetControlFlags();

    // Run full G4 background removal pipeline with OOM auto-fallback & pause/cancel support
    cv::Mat removeBackground(const cv::Mat& input, const Settings& settings);
    cv::Mat removeBackground(const cv::Mat& input) { return removeBackground(input, Settings{}); }
    QString activeBackend() const;

    static QString getModelUrl();
    static int estimateTileCount(int width, int height, int tileSize, float overlap, bool enableTiling);

signals:
    void progressChanged(int percent, const QString& statusText);
    void progressStageChanged(int basePercent, int nextPercent, const QString& statusText);
    void backendChanged(const QString& backendLabel);
    void error(const QString& message);

private:
    std::unique_ptr<OnnxRuntimeHelper::SessionBundle> m_session;
    QString m_loadedModelPath;
    std::vector<float> m_inputBuffer; // Reused across passes to avoid repeated allocations

    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_cancelled{false};
    mutable QMutex m_pauseMutex;
    QWaitCondition m_pauseCond;
    std::mutex m_terminateMutex;
    void (*m_activeTerminateFn)(void*) = nullptr;
    void* m_activeTerminateCtx = nullptr;
    int m_lastPercent = 0;

    bool waitIfPaused();
    bool loadModel(bool allowGpu = true);
    void unloadModel();
    void emitStage(int basePercent, int nextPercent, const QString& statusText);

    cv::Mat runSinglePass1536(const cv::Mat& rgb1536);
    cv::Mat predictGlobalLogits(const cv::Mat& imgRgb);
    cv::Mat predictTiledLogits(const cv::Mat& imgRgb, int tileSize, float overlap, int progBase, int progSpan, const QString& passTag);
    cv::Mat runOneDirection(const cv::Mat& imgRgb, const Settings& settings, int progBase, int progSpan, const QString& passTag);

    static cv::Mat logitsToProb(const cv::Mat& logits);
    static cv::Mat fuseGlobalAndTiled(const cv::Mat& gLogit, const cv::Mat& tLogit);
    static cv::Mat applyEdgeOnlyClean(const cv::Mat& prob, float eps);
    static const cv::Mat& getHannWindow(int size);

    static constexpr int MODEL_SIZE = 1536;
};

#endif // BACKGROUNDREMOVER_H
