#ifndef ONNXRUNTIMEHELPER_H
#define ONNXRUNTIMEHELPER_H

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <onnxruntime_cxx_api.h>
#include <onnxruntime_run_options_config_keys.h>

namespace OnnxRuntimeHelper {

// Prepend CUDA 13 & cuDNN 9 DLL search directories once per process before ORT initializes
inline void ensureCudaDllPaths() {
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
#ifdef Q_OS_WIN
        QStringList candidateDirs;

        auto addIfValid = [&candidateDirs](const QString& dirPath, const QString& probeFile = QString()) {
            if (dirPath.isEmpty()) return;
            QDir d(dirPath);
            if (!d.exists()) return;
            if (!probeFile.isEmpty() && !QFileInfo::exists(d.filePath(probeFile))) return;
            QString native = QDir::toNativeSeparators(d.absolutePath());
            if (!candidateDirs.contains(native, Qt::CaseInsensitive)) {
                candidateDirs.append(native);
            }
        };

        // 1. CUDA Toolkit bin (prefer v13.* first since ORT CUDA EP 1.32 links cublas64_13.dll)
        QString cudaRoot = QString::fromLocal8Bit(qgetenv("CUDA_PATH"));
        addIfValid(cudaRoot + "/bin", "cublas64_13.dll");
        addIfValid(cudaRoot + "/bin/x64", "cublas64_13.dll");

        QDir cudaBase("C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA");
        if (cudaBase.exists()) {
            QStringList versions = cudaBase.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
            // Prioritize v13.* versions before other versions
            versions.sort();
            std::stable_partition(versions.begin(), versions.end(), [](const QString& v) {
                return v.startsWith("v13", Qt::CaseInsensitive);
            });
            for (const QString& ver : versions) {
                addIfValid(cudaBase.filePath(ver + "/bin"), "cublas64_13.dll");
                addIfValid(cudaBase.filePath(ver + "/bin/x64"), "cublas64_13.dll");
            }
        }

        // 2. cuDNN 9 bin (system NVIDIA CUDNN install or Python torch/nvidia-cudnn packages)
        QDir cudnnSys("C:/Program Files/NVIDIA/CUDNN");
        if (cudnnSys.exists()) {
            for (const QString& ver : cudnnSys.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed)) {
                addIfValid(cudnnSys.filePath(ver + "/bin"), "cudnn64_9.dll");
                QDir binDir(cudnnSys.filePath(ver + "/bin"));
                if (binDir.exists()) {
                    for (const QString& sub : binDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed)) {
                        addIfValid(binDir.filePath(sub), "cudnn64_9.dll");
                    }
                }
            }
        }

        QString localAppData = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
        if (!localAppData.isEmpty()) {
            QDir pyBase(localAppData + "/Programs/Python");
            if (pyBase.exists()) {
                for (const QString& pyVer : pyBase.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed)) {
                    addIfValid(pyBase.filePath(pyVer + "/Lib/site-packages/nvidia/cudnn/bin"), "cudnn64_9.dll");
                    addIfValid(pyBase.filePath(pyVer + "/Lib/site-packages/torch/lib"), "cudnn64_9.dll");
                }
            }
        }

        if (!candidateDirs.isEmpty()) {
            QByteArray curPath = qgetenv("PATH");
            QString curPathStr = QString::fromLocal8Bit(curPath);
            QStringList toPrepend;
            for (const QString& dir : candidateDirs) {
                if (!curPathStr.contains(dir, Qt::CaseInsensitive)) {
                    toPrepend.append(dir);
                }
            }
            if (!toPrepend.isEmpty()) {
                QString newPath = toPrepend.join(';') + ";" + curPathStr;
                qputenv("PATH", newPath.toLocal8Bit());
                SetEnvironmentVariableW(L"PATH", reinterpret_cast<LPCWSTR>(newPath.utf16()));
                qDebug() << "ORT CUDA DLL search paths prepended:" << toPrepend;
            }
        }
#endif
    });
}

inline QString queryNvidiaDeviceLabel() {
#ifdef Q_OS_WIN
    HMODULE hNvCuda = LoadLibraryA("nvcuda.dll");
    if (hNvCuda) {
        using cuInit_fn = int (*)(unsigned int);
        using cuDeviceGet_fn = int (*)(int*, int);
        using cuDeviceGetName_fn = int (*)(char*, int, int);
        auto pInit = reinterpret_cast<cuInit_fn>(GetProcAddress(hNvCuda, "cuInit"));
        auto pGet = reinterpret_cast<cuDeviceGet_fn>(GetProcAddress(hNvCuda, "cuDeviceGet"));
        auto pName = reinterpret_cast<cuDeviceGetName_fn>(GetProcAddress(hNvCuda, "cuDeviceGetName"));
        if (pInit && pGet && pName && pInit(0) == 0) {
            int dev = 0;
            if (pGet(&dev, 0) == 0) {
                char buf[128] = {};
                if (pName(buf, sizeof(buf), dev) == 0 && buf[0] != '\0') {
                    QString raw = QString::fromLatin1(buf).trimmed();
                    raw.replace("NVIDIA GeForce ", "");
                    raw.replace(" Laptop GPU", "");
                    raw.replace("NVIDIA ", "");
                    return QString("%1 CUDA").arg(raw.trimmed());
                }
            }
        }
    }
#endif
    return QStringLiteral("CUDA GPU");
}

struct SessionBundle {
    std::shared_ptr<Ort::Env> env;
    Ort::SessionOptions sessionOptions;
    std::unique_ptr<Ort::Session> session;
    std::vector<std::string> inputNameStrings;
    std::vector<std::string> outputNameStrings;
    std::vector<const char*> inputNames;
    std::vector<const char*> outputNames;
    bool usingGpu = false;
    QString backendLabel = QStringLiteral("CPU");
};

// Create an ORT session with CUDA EP when available (memPattern + kSameAsRequested + HEURISTIC)
// and transparently fall back to low-memory multi-thread CPU execution when GPU init fails.
inline std::unique_ptr<SessionBundle> createSession(
    const char* logId,
    const std::wstring& modelPath,
    bool preferGpu
) {
    ensureCudaDllPaths();

    auto loadWithOptions = [&](bool tryCuda) -> std::unique_ptr<SessionBundle> {
        auto bundle = std::make_unique<SessionBundle>();
        bundle->env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_ERROR, logId);

        int numThreads = static_cast<int>(std::max(4u, std::thread::hardware_concurrency()));
        bundle->sessionOptions.SetIntraOpNumThreads(numThreads);
        bundle->sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        if (tryCuda) {
            // Optimal config verified on RTX 5070 8GB:
            // EnableMemPattern + kSameAsRequested + HEURISTIC + Shrinkage per Run prevents WDDM spill (1.84s vs 59s)
            bundle->sessionOptions.EnableMemPattern();
            bundle->sessionOptions.EnableCpuMemArena();

            OrtCUDAProviderOptionsV2* cudaOpts = nullptr;
            Ort::ThrowOnError(Ort::GetApi().CreateCUDAProviderOptions(&cudaOpts));
            struct CudaOptsGuard {
                OrtCUDAProviderOptionsV2* ptr;
                ~CudaOptsGuard() {
                    if (ptr) Ort::GetApi().ReleaseCUDAProviderOptions(ptr);
                }
            } guard{cudaOpts};

            const char* keys[] = {
                "device_id",
                "arena_extend_strategy",
                "cudnn_conv_algo_search",
                "cudnn_conv_use_max_workspace",
                "do_copy_in_default_stream"
            };
            const char* vals[] = {
                "0",
                "kSameAsRequested",
                "HEURISTIC",
                "0",
                "1"
            };
            Ort::ThrowOnError(Ort::GetApi().UpdateCUDAProviderOptions(cudaOpts, keys, vals, 5));
            bundle->sessionOptions.AppendExecutionProvider_CUDA_V2(*cudaOpts);
            bundle->usingGpu = true;
            bundle->backendLabel = queryNvidiaDeviceLabel();
        } else {
            bundle->sessionOptions.DisableCpuMemArena();
            bundle->sessionOptions.DisableMemPattern();
            bundle->usingGpu = false;
            bundle->backendLabel = QString("CPU (%1T)").arg(numThreads);
        }

        bundle->session = std::make_unique<Ort::Session>(*bundle->env, modelPath.c_str(), bundle->sessionOptions);

        Ort::AllocatorWithDefaultOptions allocator;
        if (bundle->session->GetInputCount() > 0) {
            auto inName = bundle->session->GetInputNameAllocated(0, allocator);
            bundle->inputNameStrings.emplace_back(inName.get());
            bundle->inputNames.push_back(bundle->inputNameStrings.back().c_str());
        }
        if (bundle->session->GetOutputCount() > 0) {
            auto outName = bundle->session->GetOutputNameAllocated(0, allocator);
            bundle->outputNameStrings.emplace_back(outName.get());
            bundle->outputNames.push_back(bundle->outputNameStrings.back().c_str());
        }
        return bundle;
    };

    if (preferGpu) {
        try {
            return loadWithOptions(true);
        } catch (const std::exception& e) {
            qWarning() << logId << "CUDA EP unavailable, falling back to CPU:" << e.what();
        }
    }
    return loadWithOptions(false);
}

// Registers an active Ort::RunOptions for the duration of Session::Run so Cancel can SetTerminate() immediately.
class ScopedRunOptions {
public:
    ScopedRunOptions(
        std::mutex& mutex,
        void (*&fnSlot)(void*),
        void*& ctxSlot,
        const std::atomic<bool>& cancelled,
        bool usingGpu
    )
        : m_mutex(mutex)
        , m_fnSlot(fnSlot)
        , m_ctxSlot(ctxSlot)
    {
        if (usingGpu) {
            m_runOpts.AddConfigEntry(kOrtRunOptionsConfigEnableMemoryArenaShrinkage, "gpu:0");
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (cancelled.load()) {
            m_runOpts.SetTerminate();
        }
        m_fnSlot = &ScopedRunOptions::invokeTerminate;
        m_ctxSlot = &m_runOpts;
    }

    ~ScopedRunOptions() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fnSlot = nullptr;
        m_ctxSlot = nullptr;
    }

    Ort::RunOptions& get() { return m_runOpts; }

    static void invokeTerminate(void* ctx) {
        if (ctx) {
            static_cast<Ort::RunOptions*>(ctx)->SetTerminate();
        }
    }

private:
    std::mutex& m_mutex;
    void (*&m_fnSlot)(void*);
    void*& m_ctxSlot;
    Ort::RunOptions m_runOpts;
};

} // namespace OnnxRuntimeHelper

#endif // ONNXRUNTIMEHELPER_H
