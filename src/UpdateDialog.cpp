#include "UpdateDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QApplication>
#include <algorithm>
#include <cmath>

UpdateDialog::UpdateDialog(const UpdateInfo& info, const QString& currentVersion, QWidget* parent)
    : GlassModalDialog("Software Update", parent)
    , m_info(info)
    , m_currentVersion(currentVersion)
    , m_nam(new QNetworkAccessManager(this))
{
    setMinimumSize(560, 520);
    setupUI();
}

UpdateDialog::~UpdateDialog() {
    cancelDownload();
}

void UpdateDialog::setupUI() {
    const GlassTheme& th = ThemeManager::instance().current();
    QVBoxLayout* root = contentLayout();
    root->setSpacing(12);

    // 1) HERO VERSION CARD
    GlassPanel* heroCard = new GlassPanel();
    heroCard->setCornerRadius(18.0);
    heroCard->setAccentGlow(true);
    QVBoxLayout* hLay = new QVBoxLayout(heroCard);
    hLay->setContentsMargins(18, 15, 18, 15);
    hLay->setSpacing(6);

    QHBoxLayout* topRow = new QHBoxLayout();
    QLabel* titleLbl = new QLabel(m_info.releaseTitle.isEmpty()
        ? QString("PixelEraser Pro v%1").arg(m_info.version)
        : m_info.releaseTitle);
    titleLbl->setStyleSheet("color: #ffffff; font-size: 16px; font-weight: 700; background: transparent;");

    QLabel* verPill = new QLabel(QString("v%1  →  v%2").arg(m_currentVersion, m_info.version));
    verPill->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: 700; "
        "background-color: rgba(%2,%3,%4,%5); border: 1px solid rgba(%6,%7,%8,%9); "
        "border-radius: 10px; padding: 3px 10px;"
    ).arg(th.badgeText.name())
     .arg(th.badgeBg.red()).arg(th.badgeBg.green()).arg(th.badgeBg.blue()).arg(th.badgeBg.alpha())
     .arg(th.badgeBorder.red()).arg(th.badgeBorder.green()).arg(th.badgeBorder.blue()).arg(th.badgeBorder.alpha()));

    topRow->addWidget(titleLbl, 1);
    topRow->addWidget(verPill);
    hLay->addLayout(topRow);

    QLabel* subLbl = new QLabel("A new release of PixelEraser Pro is ready to download and install directly.");
    subLbl->setStyleSheet("color: #aab2c2; font-size: 12px; background: transparent;");
    subLbl->setWordWrap(true);
    hLay->addWidget(subLbl);
    root->addWidget(heroCard);

    // 2) WHAT'S NEW RELEASE NOTES CARD
    GlassPanel* notesCard = new GlassPanel();
    notesCard->setCornerRadius(18.0);
    QVBoxLayout* nLay = new QVBoxLayout(notesCard);
    nLay->setContentsMargins(16, 13, 16, 14);
    nLay->setSpacing(8);

    QLabel* notesHdr = new QLabel("WHAT'S NEW");
    notesHdr->setStyleSheet(
        "color: rgba(210, 214, 222, 165); font-size: 10px; font-weight: 700; "
        "letter-spacing: 0.8px; background: transparent;"
    );
    nLay->addWidget(notesHdr);

    m_notesBrowser = new QTextBrowser();
    m_notesBrowser->setOpenExternalLinks(true);
    m_notesBrowser->setMinimumHeight(185);
    m_notesBrowser->setStyleSheet(
        "QTextBrowser { "
        "  background-color: rgba(14, 16, 20, 175); border: 1px solid rgba(255, 255, 255, 26); "
        "  border-radius: 12px; padding: 10px 12px; color: #e4e8f0; font-size: 12px; "
        "}"
    );
    const QString notesMd = m_info.releaseNotes.trimmed().isEmpty()
        ? QString("Performance improvements, bug fixes, and new features in v%1.").arg(m_info.version)
        : m_info.releaseNotes;
    m_notesBrowser->setMarkdown(notesMd);
    nLay->addWidget(m_notesBrowser, 1);
    root->addWidget(notesCard, 1);

    // 3) PACKAGE SELECTOR (Live Patch ~350KB vs Full Installer vs Portable ZIP)
    const int availableCount = (m_info.hasPatch() ? 1 : 0)
                             + (m_info.hasInstaller() ? 1 : 0)
                             + (m_info.hasPortable() ? 1 : 0);
    if (availableCount > 1) {
        GlassPanel* fmtWrap = new GlassPanel();
        fmtWrap->setCornerRadius(17.0);
        fmtWrap->setSubSurface(true);
        fmtWrap->setElevated(false);
        fmtWrap->setFixedHeight(38);
        QHBoxLayout* fLay = new QHBoxLayout(fmtWrap);
        fLay->setContentsMargins(3, 2, 3, 2);

        m_formatTabs = new LiquidGlassTabBar();
        m_formatTabs->setPillHeight(34);
        int defaultMode = -1;
        if (m_info.hasPatch()) {
            const QString patchTxt = m_info.patchSize > 0
                ? QString("Live Patch (%1)").arg(formatBytes(m_info.patchSize))
                : QString("Live Patch");
            m_formatTabs->addTab(ModeLivePatch, patchTxt, "", "", "Downloads only updated files and restarts automatically");
            defaultMode = ModeLivePatch;
        }
        if (m_info.hasInstaller()) {
            const QString exeTxt = m_info.installerSize > 0
                ? QString("Installer (%1)").arg(formatBytes(m_info.installerSize))
                : QString("Installer (.exe)");
            m_formatTabs->addTab(ModeInstaller, exeTxt, "", "", "Full Windows installer (.exe)");
            if (defaultMode < 0) defaultMode = ModeInstaller;
        }
        if (m_info.hasPortable()) {
            const QString zipTxt = m_info.portableSize > 0
                ? QString("Portable (%1)").arg(formatBytes(m_info.portableSize))
                : QString("Portable (.zip)");
            m_formatTabs->addTab(ModePortable, zipTxt, "", "", "Full standalone portable ZIP");
            if (defaultMode < 0) defaultMode = ModePortable;
        }
        if (defaultMode >= 0) {
            m_formatTabs->setCurrentId(defaultMode, false);
        }
        fLay->addWidget(m_formatTabs);
        root->addWidget(fmtWrap);

        connect(m_formatTabs, &LiquidGlassTabBar::tabSelected, this, [this](int) {
            updateActionLabel();
        });
    }

    m_modeHintLabel = new QLabel();
    m_modeHintLabel->setStyleSheet("color: #9ba3b4; font-size: 11px; background: transparent;");
    m_modeHintLabel->setWordWrap(true);
    root->addWidget(m_modeHintLabel);

    // 4) LIVE DOWNLOAD PROGRESS CARD (Hidden until download starts)
    m_progressCard = new GlassPanel();
    m_progressCard->setCornerRadius(16.0);
    m_progressCard->setVisible(false);
    QVBoxLayout* pLay = new QVBoxLayout(m_progressCard);
    pLay->setContentsMargins(16, 12, 16, 12);
    pLay->setSpacing(7);

    QHBoxLayout* pTop = new QHBoxLayout();
    m_statusLabel = new QLabel("Preparing download...");
    m_statusLabel->setStyleSheet("color: #f2f4f8; font-size: 12px; font-weight: 600; background: transparent;");
    m_pctLabel = new QLabel("0%");
    m_pctLabel->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: 700; "
        "background-color: rgba(%2,%3,%4,%5); border: 1px solid rgba(%6,%7,%8,%9); "
        "border-radius: 9px; padding: 2px 9px;"
    ).arg(th.badgeText.name())
     .arg(th.badgeBg.red()).arg(th.badgeBg.green()).arg(th.badgeBg.blue()).arg(th.badgeBg.alpha())
     .arg(th.badgeBorder.red()).arg(th.badgeBorder.green()).arg(th.badgeBorder.blue()).arg(th.badgeBorder.alpha()));
    pTop->addWidget(m_statusLabel, 1);
    pTop->addWidget(m_pctLabel);
    pLay->addLayout(pTop);

    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(8);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(QString(
        "QProgressBar { background-color: rgba(255,255,255,24); border: none; border-radius: 4px; }"
        "QProgressBar::chunk { background-color: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 %1, stop:1 %2); border-radius: 4px; }"
    ).arg(th.accentStart.name(), th.accentEnd.name()));
    pLay->addWidget(m_progressBar);

    m_speedMetaLabel = new QLabel("Connecting to GitHub...");
    m_speedMetaLabel->setStyleSheet("color: #9ba3b4; font-size: 11px; font-weight: 600; background: transparent;");
    pLay->addWidget(m_speedMetaLabel);
    root->addWidget(m_progressCard);

    // 5) ACTION BUTTONS
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    m_githubBtn = new GlassButton("View on GitHub", GlassButton::Ghost);
    m_githubBtn->setMinimumHeight(36);
    connect(m_githubBtn, &QPushButton::clicked, this, [this]() {
        if (!m_info.releasePageUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(m_info.releasePageUrl));
        }
    });

    m_laterBtn = new GlassButton("Later", GlassButton::Ghost);
    m_laterBtn->setMinimumSize(92, 36);
    connect(m_laterBtn, &QPushButton::clicked, this, [this]() {
        if (m_isDownloading) {
            cancelDownload();
        } else {
            reject();
        }
    });

    m_downloadBtn = new GlassButton("Update Now", GlassButton::White);
    m_downloadBtn->setMinimumSize(148, 36);
    m_downloadBtn->setDefault(true);
    connect(m_downloadBtn, &QPushButton::clicked, this, &UpdateDialog::startDownload);

    btnRow->addWidget(m_githubBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_laterBtn);
    btnRow->addWidget(m_downloadBtn);
    root->addLayout(btnRow);

    updateActionLabel();
}

UpdateDialog::DownloadMode UpdateDialog::selectedMode() const {
    if (m_formatTabs) {
        return static_cast<DownloadMode>(m_formatTabs->currentId());
    }
    if (m_info.hasPatch()) return ModeLivePatch;
    if (m_info.hasInstaller()) return ModeInstaller;
    return ModePortable;
}

QString UpdateDialog::selectedUrl() const {
    const DownloadMode mode = selectedMode();
    if (mode == ModeLivePatch && m_info.hasPatch()) return m_info.patchUrl;
    if (mode == ModeInstaller && m_info.hasInstaller()) return m_info.installerUrl;
    if (m_info.hasPortable()) return m_info.portableUrl;
    return m_info.releasePageUrl;
}

QString UpdateDialog::selectedFileName() const {
    const DownloadMode mode = selectedMode();
    if (mode == ModeLivePatch && !m_info.patchName.isEmpty()) return m_info.patchName;
    if (mode == ModeInstaller && !m_info.installerName.isEmpty()) return m_info.installerName;
    if (!m_info.portableName.isEmpty()) return m_info.portableName;
    return QString("PixelEraserPro-%1.exe").arg(m_info.version);
}

qint64 UpdateDialog::selectedExpectedSize() const {
    const DownloadMode mode = selectedMode();
    if (mode == ModeLivePatch && m_info.patchSize > 0) return m_info.patchSize;
    if (mode == ModeInstaller && m_info.installerSize > 0) return m_info.installerSize;
    if (m_info.portableSize > 0) return m_info.portableSize;
    return 0;
}

void UpdateDialog::updateActionLabel() {
    if (!m_info.hasPatch() && !m_info.hasInstaller() && !m_info.hasPortable()) {
        m_downloadBtn->setText("Open Release Page");
        if (m_modeHintLabel) m_modeHintLabel->setText("Opens the GitHub release page in your browser.");
        return;
    }
    switch (selectedMode()) {
        case ModeLivePatch:
            m_downloadBtn->setText("Update & Restart");
            if (m_modeHintLabel) {
                m_modeHintLabel->setText("Fast delta update: downloads only changed app files and restarts automatically — no reinstall needed.");
            }
            break;
        case ModeInstaller:
            m_downloadBtn->setText("Download & Install");
            if (m_modeHintLabel) {
                m_modeHintLabel->setText("Full Windows installer package (includes all runtime DLLs).");
            }
            break;
        case ModePortable:
            m_downloadBtn->setText("Download Portable");
            if (m_modeHintLabel) {
                m_modeHintLabel->setText("Standalone portable ZIP saved to your Downloads folder.");
            }
            break;
    }
}

void UpdateDialog::startDownload() {
    if (m_isDownloading) return;

    const QString url = selectedUrl();
    if (!m_info.hasPatch() && !m_info.hasInstaller() && !m_info.hasPortable()) {
        QDesktopServices::openUrl(QUrl(url));
        accept();
        return;
    }

    const DownloadMode mode = selectedMode();
    QString targetDir;
    if (mode == ModeLivePatch) {
        targetDir = QDir(QDir::tempPath()).filePath("PixelEraserPro_patch");
        QDir().mkpath(targetDir);
    } else if (mode == ModeInstaller) {
        targetDir = QDir::tempPath();
    } else {
        targetDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (targetDir.isEmpty()) targetDir = QDir::tempPath();
    }

    m_savedFilePath = QDir(targetDir).filePath(selectedFileName());
    m_outFile.setFileName(m_savedFilePath);
    if (!m_outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        GlassDialog::error(this, "Download Error", "Could not create local update file.", m_savedFilePath);
        return;
    }

    m_isDownloading = true;
    m_lastBytes = 0;
    m_lastTickMs = 0;
    m_emaBytesPerSec = 0.0;
    m_dlTimer.restart();

    if (m_formatTabs) m_formatTabs->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    m_downloadBtn->setText("Downloading...");
    m_laterBtn->setText("Cancel");
    m_laterBtn->setVariant(GlassButton::Danger);
    m_progressCard->setVisible(true);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    m_pctLabel->setText("0%");
    m_statusLabel->setText(QString("Downloading %1").arg(selectedFileName()));
    m_speedMetaLabel->setText("Connecting to GitHub...");

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "PixelEraserPro-Updater");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_activeReply = m_nam->get(req);
    connect(m_activeReply, &QNetworkReply::readyRead, this, &UpdateDialog::onDownloadReadyRead);
    connect(m_activeReply, &QNetworkReply::downloadProgress, this, &UpdateDialog::onDownloadProgress);
    connect(m_activeReply, &QNetworkReply::finished, this, &UpdateDialog::onDownloadFinished);
}

void UpdateDialog::cancelDownload() {
    if (!m_isDownloading) return;
    m_isDownloading = false;

    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
    if (m_outFile.isOpen()) {
        m_outFile.close();
    }
    if (!m_savedFilePath.isEmpty() && QFile::exists(m_savedFilePath)) {
        QFile::remove(m_savedFilePath);
    }

    if (m_formatTabs) m_formatTabs->setEnabled(true);
    m_downloadBtn->setEnabled(true);
    updateActionLabel();
    m_laterBtn->setText("Later");
    m_laterBtn->setVariant(GlassButton::Ghost);
    m_statusLabel->setText("Download cancelled.");
    m_speedMetaLabel->setText("You can retry anytime.");
}

void UpdateDialog::reject() {
    if (m_isDownloading) {
        cancelDownload();
    }
    GlassModalDialog::reject();
}

void UpdateDialog::onDownloadReadyRead() {
    if (m_activeReply && m_outFile.isOpen()) {
        m_outFile.write(m_activeReply->readAll());
    }
}

void UpdateDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (!m_isDownloading) return;

    const qint64 effectiveTotal = (bytesTotal > 0) ? bytesTotal : selectedExpectedSize();
    const qint64 nowMs = m_dlTimer.elapsed();

    if (nowMs - m_lastTickMs >= 120 || m_lastTickMs == 0) {
        const double dtSec = std::max(0.05, (nowMs - m_lastTickMs) / 1000.0);
        const double instSpeed = static_cast<double>(bytesReceived - m_lastBytes) / dtSec;
        m_emaBytesPerSec = (m_emaBytesPerSec <= 0.0) ? instSpeed : (m_emaBytesPerSec * 0.72 + instSpeed * 0.28);
        m_lastBytes = bytesReceived;
        m_lastTickMs = nowMs;
    }

    if (effectiveTotal > 0) {
        const double frac = std::clamp(static_cast<double>(bytesReceived) / static_cast<double>(effectiveTotal), 0.0, 1.0);
        m_progressBar->setRange(0, 1000);
        m_progressBar->setValue(static_cast<int>(std::lround(frac * 1000.0)));
        m_pctLabel->setText(QString("%1%").arg(static_cast<int>(std::floor(frac * 100.0))));

        QString etaStr;
        if (m_emaBytesPerSec > 1024.0 && effectiveTotal > bytesReceived) {
            etaStr = QString("  ·  ETA %1").arg(formatEta((effectiveTotal - bytesReceived) / m_emaBytesPerSec));
        }
        m_speedMetaLabel->setText(QString("%1 / %2  ·  %3%4")
            .arg(formatBytes(bytesReceived), formatBytes(effectiveTotal), formatSpeed(m_emaBytesPerSec), etaStr));
    } else {
        m_progressBar->setRange(0, 0);
        m_pctLabel->setText("...");
        m_speedMetaLabel->setText(QString("%1 downloaded  ·  %2")
            .arg(formatBytes(bytesReceived), formatSpeed(m_emaBytesPerSec)));
    }
}

void UpdateDialog::onDownloadFinished() {
    if (!m_activeReply) return;

    QNetworkReply* reply = m_activeReply;
    m_activeReply = nullptr;
    m_isDownloading = false;

    if (m_outFile.isOpen()) {
        if (reply->bytesAvailable() > 0) {
            m_outFile.write(reply->readAll());
        }
        m_outFile.flush();
        m_outFile.close();
    }

    const QNetworkReply::NetworkError err = reply->error();
    const QString errStr = reply->errorString();
    reply->deleteLater();

    if (err != QNetworkReply::NoError) {
        QFile::remove(m_savedFilePath);
        if (m_formatTabs) m_formatTabs->setEnabled(true);
        m_downloadBtn->setEnabled(true);
        updateActionLabel();
        m_laterBtn->setText("Later");
        m_laterBtn->setVariant(GlassButton::Ghost);
        m_statusLabel->setText("Download failed");
        m_speedMetaLabel->setText(errStr);
        return;
    }

    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(1000);
    m_pctLabel->setText("100%");

    const DownloadMode mode = selectedMode();
    if (mode == ModeLivePatch) {
        m_statusLabel->setText("Applying live patch & restarting...");
        m_speedMetaLabel->setText("Updating only changed files in-place — PixelEraser Pro will reopen in a second.");
        if (applyLivePatchAndRestart(m_savedFilePath)) {
            m_launchedInstaller = true;
            accept();
            QApplication::quit();
        } else {
            GlassDialog::error(this, "Live Patch Failed",
                "Could not launch the in-place updater script. Try the Full Installer tab instead.",
                m_savedFilePath);
            if (m_formatTabs) m_formatTabs->setEnabled(true);
            m_downloadBtn->setEnabled(true);
            updateActionLabel();
        }
    } else if (mode == ModeInstaller && m_savedFilePath.endsWith(".exe", Qt::CaseInsensitive)) {
        m_statusLabel->setText("Launching installer...");
        m_speedMetaLabel->setText("PixelEraser Pro will close automatically so the update can install.");
        if (QProcess::startDetached(m_savedFilePath, QStringList())) {
            m_launchedInstaller = true;
            accept();
            QApplication::quit();
        } else {
            GlassDialog::warning(this, "Installer Ready",
                "Downloaded the installer, but Windows blocked automatic launch.",
                m_savedFilePath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_savedFilePath).absolutePath()));
            accept();
        }
    } else {
        m_statusLabel->setText("Portable ZIP downloaded!");
        m_speedMetaLabel->setText(m_savedFilePath);
        QProcess::startDetached("explorer.exe", QStringList() << "/select," << QDir::toNativeSeparators(m_savedFilePath));
        accept();
    }
}

bool UpdateDialog::applyLivePatchAndRestart(const QString& patchZipPath) {
    const QString appDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString appExe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString patchDir = QDir::toNativeSeparators(QFileInfo(patchZipPath).absolutePath());
    const QString stageDir = QDir::toNativeSeparators(QDir(patchDir).filePath("stage"));
    const QString scriptPath = QDir::toNativeSeparators(QDir(patchDir).filePath("apply_patch.ps1"));
    const qint64 pid = QCoreApplication::applicationPid();

    // Test write permission in appDir (e.g. Program Files requires elevation; %LOCALAPPDATA% / Portable does not)
    bool needsElevation = false;
    {
        QFile probe(QDir(appDir).filePath(".update_write_probe"));
        if (probe.open(QIODevice::WriteOnly)) {
            probe.close();
            probe.remove();
        } else {
            needsElevation = true;
        }
    }

    auto psEsc = [](QString s) { return s.replace("'", "''"); };

    const QString psScript = QString(
        "$ErrorActionPreference = 'SilentlyContinue'\n"
        "$appPid = %1\n"
        "for ($i = 0; $i -lt 40; $i++) {\n"
        "    if (-not (Get-Process -Id $appPid -ErrorAction SilentlyContinue)) { break }\n"
        "    Start-Sleep -Milliseconds 250\n"
        "}\n"
        "$stage = '%2'\n"
        "if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }\n"
        "New-Item -ItemType Directory -Path $stage -Force | Out-Null\n"
        "tar.exe -xf '%3' -C $stage\n"
        "Copy-Item -Path (Join-Path $stage '*') -Destination '%4' -Recurse -Force\n"
        "Start-Process -FilePath '%5'\n"
        "Start-Sleep -Milliseconds 400\n"
        "Remove-Item -Path '%6' -Recurse -Force\n"
    ).arg(pid)
     .arg(psEsc(stageDir))
     .arg(psEsc(QDir::toNativeSeparators(patchZipPath)))
     .arg(psEsc(appDir))
     .arg(psEsc(appExe))
     .arg(psEsc(patchDir));

    QFile sf(scriptPath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    sf.write(psScript.toUtf8());
    sf.close();

    if (needsElevation) {
        const QString cmd = QString("Start-Process powershell.exe -Verb RunAs -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','%1'")
            .arg(psEsc(scriptPath));
        return QProcess::startDetached("powershell.exe", QStringList()
            << "-NoProfile" << "-WindowStyle" << "Hidden" << "-Command" << cmd);
    }

    return QProcess::startDetached("powershell.exe", QStringList()
        << "-NoProfile" << "-ExecutionPolicy" << "Bypass" << "-WindowStyle" << "Hidden" << "-File" << scriptPath);
}

QString UpdateDialog::formatBytes(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

QString UpdateDialog::formatSpeed(double bytesPerSec) {
    if (bytesPerSec < 1024.0) return QString("%1 B/s").arg(static_cast<int>(bytesPerSec));
    if (bytesPerSec < 1024.0 * 1024.0) return QString("%1 KB/s").arg(bytesPerSec / 1024.0, 0, 'f', 1);
    return QString("%1 MB/s").arg(bytesPerSec / (1024.0 * 1024.0), 0, 'f', 2);
}

QString UpdateDialog::formatEta(double seconds) {
    const int s = std::max(1, static_cast<int>(std::ceil(seconds)));
    if (s < 60) return QString("%1s").arg(s);
    const int m = s / 60;
    const int rem = s % 60;
    return QString("%1m %2s").arg(m).arg(rem);
}
