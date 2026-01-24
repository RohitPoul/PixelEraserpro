#include "UpdateChecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QApplication>

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_silentCheck(false)
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onReplyFinished);
}

UpdateChecker::~UpdateChecker()
{
}

void UpdateChecker::setRepository(const QString &repo)
{
    m_repository = repo;
}

void UpdateChecker::setCurrentVersion(const QString &version)
{
    m_currentVersion = version;
}

void UpdateChecker::checkForUpdates(bool silent)
{
    m_silentCheck = silent;
    
    if (m_repository.isEmpty()) {
        emit checkFailed("Repository not configured");
        return;
    }
    
    // GitHub API endpoint for latest release
    QString url = QString("https://api.github.com/repos/%1/releases/latest").arg(m_repository);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "PixelEraserPro-UpdateChecker");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    
    m_networkManager->get(request);
}

void UpdateChecker::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        if (!m_silentCheck) {
            emit checkFailed(reply->errorString());
        }
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        if (!m_silentCheck) {
            emit checkFailed("Invalid response from server");
        }
        return;
    }
    
    QJsonObject obj = doc.object();
    
    // Get version tag (remove 'v' prefix if present)
    QString tagName = obj["tag_name"].toString();
    m_latestVersion = tagName.startsWith('v') ? tagName.mid(1) : tagName;
    
    // Get release notes
    QString releaseNotes = obj["body"].toString();
    
    // Get download URL - look for .exe asset first, fallback to html_url
    m_downloadUrl = obj["html_url"].toString();
    
    QJsonArray assets = obj["assets"].toArray();
    for (const QJsonValue &asset : assets) {
        QJsonObject assetObj = asset.toObject();
        QString name = assetObj["name"].toString();
        if (name.endsWith(".exe") || name.endsWith(".zip")) {
            m_downloadUrl = assetObj["browser_download_url"].toString();
            break;
        }
    }
    
    // Compare versions
    if (isNewerVersion(m_latestVersion, m_currentVersion)) {
        emit updateAvailable(m_latestVersion, m_downloadUrl, releaseNotes);
    } else {
        // Only emit noUpdateAvailable if it's NOT a silent check
        if (!m_silentCheck) {
            emit noUpdateAvailable();
        }
    }
}

bool UpdateChecker::isNewerVersion(const QString &latest, const QString &current)
{
    QVersionNumber latestVer = QVersionNumber::fromString(latest);
    QVersionNumber currentVer = QVersionNumber::fromString(current);
    
    return latestVer > currentVer;
}
