#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QLabel>
#include <QProgressBar>
#include <QTextBrowser>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QElapsedTimer>
#include "GlassWidgets.h"
#include "UpdateChecker.h"

class UpdateDialog : public GlassModalDialog {
    Q_OBJECT

public:
    explicit UpdateDialog(const UpdateInfo& info, const QString& currentVersion, QWidget* parent = nullptr);
    ~UpdateDialog() override;

    bool launchedInstaller() const { return m_launchedInstaller; }

protected:
    void reject() override;

private slots:
    void startDownload();
    void cancelDownload();
    void onDownloadReadyRead();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    enum DownloadMode { ModeLivePatch = 0, ModeInstaller = 1, ModePortable = 2 };

    void setupUI();
    void updateActionLabel();
    DownloadMode selectedMode() const;
    QString selectedUrl() const;
    QString selectedFileName() const;
    qint64 selectedExpectedSize() const;
    bool applyLivePatchAndRestart(const QString& patchZipPath);

    static QString formatBytes(qint64 bytes);
    static QString formatSpeed(double bytesPerSec);
    static QString formatEta(double seconds);

    UpdateInfo m_info;
    QString m_currentVersion;

    LiquidGlassTabBar* m_formatTabs = nullptr;
    QLabel* m_modeHintLabel = nullptr;
    QTextBrowser* m_notesBrowser = nullptr;
    GlassPanel* m_progressCard = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_pctLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_speedMetaLabel = nullptr;

    GlassButton* m_githubBtn = nullptr;
    GlassButton* m_laterBtn = nullptr;
    GlassButton* m_downloadBtn = nullptr;

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_activeReply = nullptr;
    QFile m_outFile;
    QString m_savedFilePath;
    QElapsedTimer m_dlTimer;
    qint64 m_lastBytes = 0;
    qint64 m_lastTickMs = 0;
    double m_emaBytesPerSec = 0.0;
    bool m_isDownloading = false;
    bool m_launchedInstaller = false;
};

#endif // UPDATEDIALOG_H
