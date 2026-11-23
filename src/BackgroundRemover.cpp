#include "BackgroundRemover.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QMutexLocker>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <map>
#include <thread>
#include <vector>
#include "OnnxRuntimeHelper.h"

BackgroundRemover::BackgroundRemover(QObject* parent)
    : QObject(parent)
{
}

BackgroundRemover::~BackgroundRemover() {
    requestCancel();
    unloadModel();
}

void BackgroundRemover::setPaused(bool paused) {
    QMutexLocker locker(&m_pauseMutex);
    m_paused.store(paused);
    if (!paused) {
        m_pauseCond.wakeAll();
    }
}

void BackgroundRemover::requestCancel() {
    {
        QMutexLocker locker(&m_pauseMutex);
        m_cancelled.store(true);
        m_paused.store(false);
        m_pauseCond.wakeAll();
    }
    std::lock_guard<std::mutex> lock(m_terminateMutex);
    if (m_activeTerminateFn && m_activeTerminateCtx) {
        m_activeTerminateFn(m_activeTerminateCtx);
    }
}

void BackgroundRemover::resetControlFlags() {
    QMutexLocker locker(&m_pauseMutex);
    m_cancelled.store(false);
    m_paused.store(false);
    m_lastPercent = 0;
}

QString BackgroundRemover::activeBackend() const {
    return (m_session && !m_session->backendLabel.isEmpty()) ? m_session->backendLabel : QStringLiteral("CPU");
}

void BackgroundRemover::emitStage(int basePercent, int nextPercent, const QString& statusText) {
    m_lastPercent = basePercent;
    emit progressChanged(basePercent, statusText);
    emit progressStageChanged(basePercent, nextPercent, statusText);
}

bool BackgroundRemover::waitIfPaused() {
    QMutexLocker locker(&m_pauseMutex);
    while (m_paused.load() && !m_cancelled.load()) {
        locker.unlock();
        emit progressChanged(m_lastPercent, "Paused — Click Resume to continue");
        locker.relock();
        if (m_paused.load() && !m_cancelled.load()) {
            m_pauseCond.wait(&m_pauseMutex);
        }
    }
    return !m_cancelled.load();
}

QString BackgroundRemover::getModelUrl() {
    return "https://huggingface.co/datasets/RohitPoul/15-09-2026/resolve/main/g4.onnx";
}

QString BackgroundRemover::getModelPath() const {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir modelDir(appData + "/models");
    if (!modelDir.exists()) {
        modelDir.mkpath(".");
    }
    return modelDir.filePath("g4.onnx");
}

bool BackgroundRemover::isModelAvailable() const {
    QFileInfo fi(getModelPath());
    return fi.exists() && fi.isFile() && fi.size() > 1024 * 1024;
}

int BackgroundRemover::estimateTileCount(int width, int height, int tileSize, float overlap, bool enableTiling) {
    const int effTile = (tileSize > 0) ? tileSize : MODEL_SIZE;
    if (!enableTiling || std::max(width, height) <= effTile) {
        return 0;
    }
    const int stride = std::max(1, static_cast<int>(effTile * (1.0f - overlap)));
    int ny = 0;
    int lastY = -effTile;
    for (int y = 0; y < std::max(1, height - effTile + 1); y += stride) {
        ny++;
        lastY = y;
    }
    if (ny == 0 || lastY + effTile < height) ny++;

    int nx = 0;
    int lastX = -effTile;
    for (int x = 0; x < std::max(1, width - effTile + 1); x += stride) {
        nx++;
        lastX = x;
    }
    if (nx == 0 || lastX + effTile < width) nx++;

    return nx * ny;
}

bool BackgroundRemover::downloadModel(std::function<void(qint64, qint64)> progressCallback) {
    QString url = getModelUrl();
    QString path = getModelPath();

    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = manager.get(request);
    QFile file(path + ".part");
    if (!file.open(QIODevice::WriteOnly)) {
        emit error("Cannot open file for writing: " + path);
        reply->deleteLater();
        return false;
    }

    QEventLoop loop;
    connect(reply, &QNetworkReply::readyRead, [&file, reply]() {
        file.write(reply->readAll());
    });
    if (progressCallback) {
        connect(reply, &QNetworkReply::downloadProgress, [progressCallback](qint64 received, qint64 total) {
            progressCallback(received, total);
        });
    }
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    file.write(reply->readAll());
    file.close();

    if (reply->error() != QNetworkReply::NoError) {
        QFile::remove(path + ".part");
        emit error("Download failed: " + reply->errorString());
        reply->deleteLater();
        return false;
    }

    reply->deleteLater();
    if (QFile::exists(path)) {
        QFile::remove(path);
    }
    QFile::rename(path + ".part", path);
    return true;
}

bool BackgroundRemover::loadModel(bool allowGpu) {
    QString modelPath = getModelPath();
    if (!QFile::exists(modelPath)) {
        emit error("G4 ONNX model file not found: " + modelPath);
        return false;
    }

    if (m_session && m_session->session && m_loadedModelPath == modelPath && (allowGpu || !m_session->usingGpu)) {
        return true;
    }

    try {
        m_session = OnnxRuntimeHelper::createSession("G4BackgroundRemover", modelPath.toStdWString(), allowGpu);
        m_loadedModelPath = modelPath;
        emit backendChanged(m_session->backendLabel);
        return true;
    } catch (const Ort::Exception& e) {
        QString msg = QString("ONNX Runtime error loading G4 model: %1").arg(e.what());
        qDebug() << msg;
        emit error(msg);
        m_session.reset();
        return false;
    } catch (const std::exception& e) {
        QString msg = QString("Error loading G4 model: %1").arg(e.what());
        qDebug() << msg;
        emit error(msg);
        m_session.reset();
        return false;
    }
}

void BackgroundRemover::unloadModel() {
    m_session.reset();
    m_loadedModelPath.clear();
    m_inputBuffer.clear();
    m_inputBuffer.shrink_to_fit();
}

const cv::Mat& BackgroundRemover::getHannWindow(int size) {
    static std::map<int, cv::Mat> cache;
    static QMutex cacheMutex;
    QMutexLocker locker(&cacheMutex);
    auto it = cache.find(size);
    if (it != cache.end()) {
        return it->second;
    }

    constexpr double PI = 3.14159265358979323846;
    std::vector<float> w1d(size);
    for (int i = 0; i < size; ++i) {
        double s = std::sin(PI * (i + 0.5) / size);
        w1d[i] = static_cast<float>(s * s);
    }
    cv::Mat hann2D(size, size, CV_32FC1);
    for (int y = 0; y < size; ++y) {
        float* row = hann2D.ptr<float>(y);
        float wy = w1d[y];
        for (int x = 0; x < size; ++x) {
            row[x] = std::max(wy * w1d[x], 1e-5f);
        }
    }
    auto inserted = cache.emplace(size, std::move(hann2D));
    return inserted.first->second;
}

cv::Mat BackgroundRemover::runSinglePass1536(const cv::Mat& rgb1536) {
    constexpr int S = MODEL_SIZE;
    constexpr size_t planeSize = static_cast<size_t>(S) * S;

    std::vector<int64_t> inputShape = {1, 3, S, S};
    if (m_inputBuffer.size() != 3 * planeSize) {
        m_inputBuffer.resize(3 * planeSize);
    }

    std::vector<cv::Mat> u8Channels(3);
    cv::split(rgb1536, u8Channels);

    constexpr double mean[3] = {0.485, 0.456, 0.406};
    constexpr double stdVal[3] = {0.229, 0.224, 0.225};

    for (int c = 0; c < 3; ++c) {
        cv::Mat dstPlane(S, S, CV_32FC1, m_inputBuffer.data() + c * planeSize);
        double alpha = 1.0 / (255.0 * stdVal[c]);
        double beta = -mean[c] / stdVal[c];
        u8Channels[c].convertTo(dstPlane, CV_32FC1, alpha, beta);
    }

    if (m_cancelled.load()) return cv::Mat();

    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto runOnce = [&]() -> cv::Mat {
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            m_inputBuffer.data(),
            m_inputBuffer.size(),
            inputShape.data(),
            inputShape.size()
        );
        OnnxRuntimeHelper::ScopedRunOptions ro(
            m_terminateMutex, m_activeTerminateFn, m_activeTerminateCtx, m_cancelled, m_session->usingGpu
        );
        auto outputTensors = m_session->session->Run(
            ro.get(),
            m_session->inputNames.data(),
            &inputTensor,
            1,
            m_session->outputNames.data(),
            1
        );
        const float* outputData = outputTensors[0].GetTensorData<float>();
        cv::Mat logits(S, S, CV_32FC1);
        std::memcpy(logits.data, outputData, planeSize * sizeof(float));
        return logits;
    };

    try {
        return runOnce();
    } catch (const Ort::Exception& e) {
        if (m_cancelled.load()) {
            return cv::Mat();
        }
        if (m_session && m_session->usingGpu) {
            qWarning() << "G4 CUDA pass hit runtime/VRAM limit, switching cleanly to CPU fallback:" << e.what();
            m_session.reset();
            if (loadModel(false)) {
                try {
                    return runOnce();
                } catch (const Ort::Exception&) {
                    if (m_cancelled.load()) return cv::Mat();
                    throw;
                }
            }
        }
        throw;
    }
}

cv::Mat BackgroundRemover::predictGlobalLogits(const cv::Mat& imgRgb) {
    if (!waitIfPaused()) return cv::Mat();

    const int H = imgRgb.rows;
    const int W = imgRgb.cols;
    const double scale = static_cast<double>(MODEL_SIZE) / std::max(H, W);
    const int nH = std::max(1, static_cast<int>(std::lround(H * scale)));
    const int nW = std::max(1, static_cast<int>(std::lround(W * scale)));
    const int interp = (scale < 1.0) ? cv::INTER_AREA : cv::INTER_LINEAR;

    cv::Mat imgR;
    cv::resize(imgRgb, imgR, cv::Size(nW, nH), 0, 0, interp);

    const int ph = std::max(0, MODEL_SIZE - nH);
    const int pw = std::max(0, MODEL_SIZE - nW);
    const int top = ph / 2;
    const int bottom = ph - top;
    const int left = pw / 2;
    const int right = pw - left;

    cv::Mat gPad;
    cv::copyMakeBorder(imgR, gPad, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    cv::Mat gSq = runSinglePass1536(gPad);
    if (gSq.empty() || m_cancelled.load()) return cv::Mat();
    cv::Mat gCrop = gSq(cv::Rect(left, top, nW, nH));

    cv::Mat gLogit;
    cv::resize(gCrop, gLogit, cv::Size(W, H), 0, 0, cv::INTER_LINEAR);
    return gLogit;
}

cv::Mat BackgroundRemover::predictTiledLogits(
    const cv::Mat& imgRgb, int tileSize, float overlap, int progBase, int progSpan, const QString& passTag
) {
    const int H = imgRgb.rows;
    const int W = imgRgb.cols;
    const int stride = std::max(1, static_cast<int>(tileSize * (1.0f - overlap)));

    std::vector<int> ys;
    for (int y = 0; y < std::max(1, H - tileSize + 1); y += stride) {
        ys.push_back(y);
    }
    if (ys.empty() || ys.back() + tileSize < H) {
        ys.push_back(std::max(0, H - tileSize));
    }

    std::vector<int> xs;
    for (int x = 0; x < std::max(1, W - tileSize + 1); x += stride) {
        xs.push_back(x);
    }
    if (xs.empty() || xs.back() + tileSize < W) {
        xs.push_back(std::max(0, W - tileSize));
    }

    struct TileCoord { int y0, y1, x0, x1; };
    std::vector<TileCoord> coords;
    for (int y0 : ys) {
        for (int x0 : xs) {
            coords.push_back({y0, std::min(y0 + tileSize, H), x0, std::min(x0 + tileSize, W)});
        }
    }

    const int nTiles = static_cast<int>(coords.size());
    const cv::Mat& hann = getHannWindow(tileSize);
    cv::Mat accum = cv::Mat::zeros(H, W, CV_32FC1);
    cv::Mat weight = cv::Mat::zeros(H, W, CV_32FC1);

    for (int i = 0; i < nTiles; ++i) {
        if (!waitIfPaused()) return cv::Mat();

        const auto& c = coords[i];
        int th = c.y1 - c.y0;
        int tw = c.x1 - c.x0;

        int curPct = progBase + (progSpan * i) / std::max(1, nTiles);
        int nextPct = progBase + (progSpan * (i + 1)) / std::max(1, nTiles);
        emitStage(
            curPct,
            nextPct,
            QString("[%1] Tile %2 / %3 (%4px crop) · %5").arg(passTag).arg(i + 1).arg(nTiles).arg(tileSize).arg(activeBackend())
        );

        cv::Mat sub = imgRgb(cv::Rect(c.x0, c.y0, tw, th));
        cv::Mat paddedSub;
        if (th < tileSize || tw < tileSize) {
            cv::copyMakeBorder(sub, paddedSub, 0, tileSize - th, 0, tileSize - tw, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
        } else {
            paddedSub = sub;
        }

        // Adapt arbitrary crop size (1024 / 1536 / 2048) cleanly to native 1536 G4 Swin-w18 ONNX input
        cv::Mat tileLogits;
        if (tileSize == MODEL_SIZE) {
            tileLogits = runSinglePass1536(paddedSub);
        } else {
            cv::Mat modelInput;
            const int interp = (tileSize > MODEL_SIZE) ? cv::INTER_AREA : cv::INTER_LINEAR;
            cv::resize(paddedSub, modelInput, cv::Size(MODEL_SIZE, MODEL_SIZE), 0, 0, interp);
            cv::Mat rawLogits = runSinglePass1536(modelInput);
            if (rawLogits.empty() || m_cancelled.load()) return cv::Mat();
            cv::resize(rawLogits, tileLogits, cv::Size(tileSize, tileSize), 0, 0, cv::INTER_LINEAR);
        }
        if (tileLogits.empty() || m_cancelled.load()) return cv::Mat();

        cv::Rect roi(c.x0, c.y0, tw, th);
        cv::Rect tileRoi(0, 0, tw, th);
        cv::Mat gw = hann(tileRoi);
        cv::Mat weightedLogit;
        cv::multiply(tileLogits(tileRoi), gw, weightedLogit);
        accum(roi) += weightedLogit;
        weight(roi) += gw;
    }

    if (!waitIfPaused()) return cv::Mat();

    cv::max(weight, 1e-6f, weight);
    cv::Mat tLogit;
    cv::divide(accum, weight, tLogit);
    return tLogit;
}

cv::Mat BackgroundRemover::logitsToProb(const cv::Mat& logits) {
    cv::Mat prob;
    cv::min(cv::max(logits, -30.0f), 30.0f, prob);
    prob = -prob;
    cv::exp(prob, prob);
    prob += 1.0f;
    cv::divide(1.0f, prob, prob);
    return prob;
}

cv::Mat BackgroundRemover::fuseGlobalAndTiled(const cv::Mat& gLogit, const cv::Mat& tLogit) {
    const int H = gLogit.rows;
    const int W = gLogit.cols;

    double gFgCount = cv::countNonZero(gLogit > 0.0f);
    double tFgCount = cv::countNonZero(tLogit > 0.0f);
    double totalPx = static_cast<double>(H) * W;

    cv::Mat fused;
    if ((gFgCount / totalPx) < 0.005 && (tFgCount / totalPx) >= 0.005) {
        fused = tLogit.clone();
    } else {
        const int workMax = 1024;
        const double scale = std::min(1.0, static_cast<double>(workMax) / std::max(H, W));
        cv::Mat gSmall;
        int wh = H, ww = W;
        if (scale < 1.0) {
            wh = std::max(1, static_cast<int>(std::lround(H * scale)));
            ww = std::max(1, static_cast<int>(std::lround(W * scale)));
            cv::resize(gLogit, gSmall, cv::Size(ww, wh), 0, 0, cv::INTER_LINEAR);
        } else {
            gSmall = gLogit;
        }

        cv::Mat gBin = (gSmall > 0.0f);
        cv::Mat k3 = cv::Mat::ones(3, 3, CV_8UC1);
        cv::Mat dil, ero;
        cv::dilate(gBin, dil, k3);
        cv::erode(gBin, ero, k3);
        cv::Mat zeroCross = (dil != ero);

        cv::Mat absG;
        cv::absdiff(gSmall, cv::Scalar(0.0f), absG);
        cv::Mat lowConf = (absG <= 2.5f);

        cv::Mat uncertain;
        cv::bitwise_or(lowConf, zeroCross, uncertain);
        uncertain.convertTo(uncertain, CV_8UC1, 1.0 / 255.0);

        int rad = std::max(15, static_cast<int>(std::lround(std::max(wh, ww) * 0.025))) | 1;
        cv::Mat kDil = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(rad, rad));
        cv::Mat corridorU8, corridor;
        cv::dilate(uncertain, corridorU8, kDil);
        corridorU8.convertTo(corridor, CV_32FC1);

        int kSm = std::max(11, (rad / 2) | 1);
        cv::Mat wTileS;
        cv::GaussianBlur(corridor, wTileS, cv::Size(kSm, kSm), 0);

        cv::Mat coreBandU8, coreBand;
        cv::dilate(uncertain, coreBandU8, cv::Mat::ones(5, 5, CV_8UC1));
        coreBandU8.convertTo(coreBand, CV_32FC1);
        cv::max(wTileS, coreBand, wTileS);

        cv::Mat wTile;
        if (wh != H || ww != W) {
            cv::resize(wTileS, wTile, cv::Size(W, H), 0, 0, cv::INTER_LINEAR);
        } else {
            wTile = wTileS;
        }
        cv::min(cv::max(wTile, 0.0f), 1.0f, wTile);

        cv::Mat diff = tLogit - gLogit;
        cv::multiply(diff, wTile, fused);
        fused += gLogit;
    }

    return logitsToProb(fused);
}

cv::Mat BackgroundRemover::applyEdgeOnlyClean(const cv::Mat& prob, float eps) {
    if (eps <= 0.0f) {
        cv::Mat clamped;
        cv::min(cv::max(prob, 0.0f), 1.0f, clamped);
        return clamped;
    }

    cv::Mat hard = (prob > 0.5f);
    cv::Mat k5 = cv::Mat::ones(5, 5, CV_8UC1);
    cv::Mat dil, ero;
    cv::dilate(hard, dil, k5);
    cv::erode(hard, ero, k5);
    cv::Mat edgeRing = (dil != ero);

    cv::Mat result = prob.clone();
    const float highThresh = 1.0f - eps;
    for (int y = 0; y < result.rows; ++y) {
        const uchar* ringRow = edgeRing.ptr<uchar>(y);
        float* probRow = result.ptr<float>(y);
        for (int x = 0; x < result.cols; ++x) {
            float p = std::clamp(probRow[x], 0.0f, 1.0f);
            if (!ringRow[x]) {
                if (p <= eps) p = 0.0f;
                else if (p >= highThresh) p = 1.0f;
            }
            probRow[x] = p;
        }
    }
    return result;
}

cv::Mat BackgroundRemover::runOneDirection(
    const cv::Mat& imgRgb, const Settings& settings, int progBase, int progSpan, const QString& passTag
) {
    if (!waitIfPaused()) return cv::Mat();

    const int H = imgRgb.rows;
    const int W = imgRgb.cols;
    const int effTile = (settings.tileSize > 0) ? settings.tileSize : MODEL_SIZE;
    const bool singlePassEnough = (!settings.enableTiling) || (std::max(H, W) <= effTile);

    if (singlePassEnough) {
        emitStage(progBase + 2, progBase + progSpan,
                  QString("[%1] Running G4 1536px Global Pass · %2").arg(passTag, activeBackend()));
        cv::Mat gLogit = predictGlobalLogits(imgRgb);
        if (gLogit.empty() || m_cancelled.load()) return cv::Mat();
        return logitsToProb(gLogit);
    }

    cv::Mat gLogit;
    int tileProgBase = progBase;
    int tileProgSpan = progSpan;

    if (settings.useGlobalAnchor) {
        int anchorEnd = progBase + static_cast<int>(progSpan * 0.18f);
        emitStage(progBase, anchorEnd,
                  QString("[%1] Running G4 1536px Global Anchor · %2").arg(passTag, activeBackend()));
        gLogit = predictGlobalLogits(imgRgb);
        if (gLogit.empty() || m_cancelled.load()) return cv::Mat();
        tileProgBase = anchorEnd;
        tileProgSpan = progBase + progSpan - anchorEnd;
    }

    // OOM-Safe Tiled Pass: falls back gracefully to Global Anchor on memory exception
    try {
        cv::Mat tLogit = predictTiledLogits(imgRgb, effTile, settings.overlap, tileProgBase, tileProgSpan, passTag);
        if (tLogit.empty() || m_cancelled.load()) return cv::Mat(); // Cancelled cleanly
        if (!gLogit.empty()) {
            emitStage(progBase + progSpan - 2, progBase + progSpan,
                      QString("[%1] Fusing Global Anchor + %2px Hann Tiles...").arg(passTag).arg(effTile));
            return fuseGlobalAndTiled(gLogit, tLogit);
        }
        return logitsToProb(tLogit);
    } catch (const std::exception& e) {
        if (m_cancelled.load()) return cv::Mat();
        qWarning() << "Tiled pass memory fallback triggered:" << e.what();
        if (!gLogit.empty()) {
            return logitsToProb(gLogit);
        }
        cv::Mat fallbackLogit = predictGlobalLogits(imgRgb);
        if (fallbackLogit.empty() || m_cancelled.load()) return cv::Mat();
        return logitsToProb(fallbackLogit);
    }
}

cv::Mat BackgroundRemover::removeBackground(const cv::Mat& input, const Settings& settings) {
    if (input.empty()) return cv::Mat();

    resetControlFlags();
    emitStage(2, 5, "Initializing G4 AI Engine...");
    if (!waitIfPaused() || !loadModel(true)) {
        unloadModel();
        return cv::Mat();
    }

    try {
        cv::Mat rgb;
        if (input.channels() == 4) {
            cv::cvtColor(input, rgb, cv::COLOR_BGRA2RGB);
        } else if (input.channels() == 3) {
            cv::cvtColor(input, rgb, cv::COLOR_BGR2RGB);
        } else {
            cv::cvtColor(input, rgb, cv::COLOR_GRAY2RGB);
        }

        const int H = rgb.rows;
        const int W = rgb.cols;
        cv::Mat prob;

        if (settings.ttaHFlip) {
            cv::Mat probFwd = runOneDirection(rgb, settings, 5, 45, "Fwd");
            if (probFwd.empty() || !waitIfPaused()) {
                unloadModel();
                return cv::Mat();
            }
            try {
                cv::Mat flippedRgb;
                cv::flip(rgb, flippedRgb, 1);
                cv::Mat probFlip = runOneDirection(flippedRgb, settings, 50, 44, "H-Flip TTA");
                if (probFlip.empty() || !waitIfPaused()) {
                    unloadModel();
                    return cv::Mat();
                }
                cv::Mat unflippedProb;
                cv::flip(probFlip, unflippedProb, 1);
                cv::addWeighted(probFwd, 0.5, unflippedProb, 0.5, 0.0, prob);
            } catch (const std::exception& e) {
                if (m_cancelled.load()) {
                    unloadModel();
                    return cv::Mat();
                }
                qWarning() << "H-Flip TTA memory fallback triggered, using forward pass:" << e.what();
                prob = probFwd;
            }
        } else {
            prob = runOneDirection(rgb, settings, 5, 88, "Fwd");
            if (prob.empty() || !waitIfPaused()) {
                unloadModel();
                return cv::Mat();
            }
        }

        emitStage(95, 100, "Applying G4 SmartEdge Deep Clean...");
        prob = applyEdgeOnlyClean(prob, settings.deepCleanEps);

        cv::Mat output;
        if (input.channels() == 4) {
            output = input.clone();
        } else if (input.channels() == 3) {
            cv::cvtColor(input, output, cv::COLOR_BGR2BGRA);
        } else {
            cv::cvtColor(input, output, cv::COLOR_GRAY2BGRA);
        }

        for (int y = 0; y < H; ++y) {
            const float* probRow = prob.ptr<float>(y);
            cv::Vec4b* outRow = output.ptr<cv::Vec4b>(y);
            for (int x = 0; x < W; ++x) {
                int a = static_cast<int>(std::lround(probRow[x] * 255.0f));
                outRow[x][3] = static_cast<uchar>(std::clamp(a, 0, 255));
            }
        }

        emitStage(100, 100, "Done");
        unloadModel();
        return output;
    } catch (const Ort::Exception& e) {
        if (m_cancelled.load()) {
            unloadModel();
            return cv::Mat();
        }
        QString msg = QString("ONNX Runtime error during G4 removal: %1").arg(e.what());
        qDebug() << msg;
        emit error(msg);
        unloadModel();
        return cv::Mat();
    } catch (const std::exception& e) {
        if (m_cancelled.load()) {
            unloadModel();
            return cv::Mat();
        }
        QString msg = QString("Error during G4 removal: %1").arg(e.what());
        qDebug() << msg;
        emit error(msg);
        unloadModel();
        return cv::Mat();
    }
}
