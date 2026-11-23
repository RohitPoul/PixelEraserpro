#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QLabel>
#include <QLineEdit>
#include <QString>
#include "GlassWidgets.h"

struct ExportSettings {
    enum NamingMode {
        KeepOriginal = 0,
        Prefix = 1,
        Suffix = 2,
        Both = 3
    };

    enum FormatMode {
        KeepOriginalType = 0,
        ForcePng = 1,
        ForceWebp = 2,
        ForceJpg = 3
    };

    NamingMode namingMode = Suffix;
    QString prefix = "cutout_";
    QString suffix = "_cutout";
    FormatMode formatMode = ForcePng;
};

class SettingsDialog : public GlassModalDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const QString& currentFilePath = QString(), QWidget* parent = nullptr);

    static ExportSettings loadSettings();
    static void saveSettings(const ExportSettings& settings);
    static QString resolveExtension(const QString& currentFilePath, ExportSettings::FormatMode formatMode);
    static QString buildExportPath(const QString& currentFilePath, const ExportSettings& settings);
    static QString buildExportPath(const QString& currentFilePath);
    static QString buildFileDialogFilter(const QString& ext);

private:
    void setupUI();
    void applyStateToControls(const ExportSettings& s);
    ExportSettings collectSettings() const;
    void updateInputStates();
    void updatePreview();

    QString m_sampleFilePath;

    LiquidGlassTabBar* m_namingTabs = nullptr;
    QLineEdit* m_prefixEdit = nullptr;
    QLineEdit* m_suffixEdit = nullptr;
    LiquidGlassTabBar* m_formatTabs = nullptr;
    QLabel* m_formatHintLabel = nullptr;
    QLabel* m_previewOutputLabel = nullptr;
};

#endif // SETTINGSDIALOG_H
