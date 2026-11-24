#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QVersionNumber>

struct UpdateInfo {
    QString version;
    QString releaseTitle;
    QString releaseNotes;
    QString releasePageUrl;
    QString patchUrl;
    QString patchName;
    qint64 patchSize = 0;
    QString installerUrl;
    QString installerName;
    qint64 installerSize = 0;
    QString portableUrl;
    QString portableName;
    qint64 portableSize = 0;

    bool hasPatch() const { return !patchUrl.isEmpty(); }
    bool hasInstaller() const { return !installerUrl.isEmpty(); }
    bool hasPortable() const { return !portableUrl.isEmpty(); }
};

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker();

    void setRepository(const QString &repo);
    void setCurrentVersion(const QString &version);
    void checkForUpdates(bool silent = false);

    QString getDownloadUrl() const { return m_downloadUrl; }
    QString getLatestVersion() const { return m_latestVersion; }
    const UpdateInfo& latestUpdateInfo() const { return m_latestInfo; }

signals:
    void updateAvailable(const UpdateInfo &info);
    void noUpdateAvailable();
    void checkFailed(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_repository;
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_downloadUrl;
    UpdateInfo m_latestInfo;
    bool m_silentCheck;

    bool isNewerVersion(const QString &latest, const QString &current);
};

#endif // UPDATECHECKER_H
