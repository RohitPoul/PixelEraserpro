#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QVersionNumber>

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker();

    // Set the GitHub repository (format: "owner/repo")
    void setRepository(const QString &repo);
    
    // Set current app version
    void setCurrentVersion(const QString &version);
    
    // Check for updates (async)
    void checkForUpdates(bool silent = false);
    
    // Get download URL for latest release
    QString getDownloadUrl() const { return m_downloadUrl; }
    
    // Get latest version string
    QString getLatestVersion() const { return m_latestVersion; }

signals:
    // Emitted when update check completes
    void updateAvailable(const QString &newVersion, const QString &downloadUrl, const QString &releaseNotes);
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
    bool m_silentCheck;
    
    bool isNewerVersion(const QString &latest, const QString &current);
};

#endif // UPDATECHECKER_H
