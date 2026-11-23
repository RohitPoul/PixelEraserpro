#include "UpdateChecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_silentCheck(false)
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onReplyFinished);
}

UpdateChecker::~UpdateChecker() = default;

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

    QString url = QString("https://api.github.com/repos/%1/releases/latest").arg(m_repository);

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "PixelEraserPro-UpdateChecker");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

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

    UpdateInfo info;
    QString tagName = obj["tag_name"].toString();
    info.version = tagName.startsWith('v', Qt::CaseInsensitive) ? tagName.mid(1) : tagName;
    info.releaseTitle = obj["name"].toString();
    if (info.releaseTitle.isEmpty()) {
        info.releaseTitle = QString("PixelEraser Pro v%1").arg(info.version);
    }
    info.releaseNotes = obj["body"].toString();
    info.releasePageUrl = obj["html_url"].toString();

    QJsonArray assets = obj["assets"].toArray();
    for (const QJsonValue &asset : assets) {
        QJsonObject assetObj = asset.toObject();
        const QString name = assetObj["name"].toString();
        const QString url = assetObj["browser_download_url"].toString();
        const qint64 size = static_cast<qint64>(assetObj["size"].toDouble(0.0));

        if (name.endsWith(".exe", Qt::CaseInsensitive)) {
            if (info.installerUrl.isEmpty() || name.contains("setup", Qt::CaseInsensitive)) {
                info.installerUrl = url;
                info.installerName = name;
                info.installerSize = size;
            }
        } else if (name.endsWith(".zip", Qt::CaseInsensitive)) {
            if (info.portableUrl.isEmpty() || name.contains("portable", Qt::CaseInsensitive)) {
                info.portableUrl = url;
                info.portableName = name;
                info.portableSize = size;
            }
        }
    }

    m_latestVersion = info.version;
    m_downloadUrl = info.hasInstaller() ? info.installerUrl
                  : (info.hasPortable() ? info.portableUrl : info.releasePageUrl);
    m_latestInfo = info;

    if (isNewerVersion(info.version, m_currentVersion)) {
        emit updateAvailable(info);
    } else {
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
