#ifndef REMOVERDIALOG_H
#define REMOVERDIALOG_H

#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include "BackgroundRemover.h"
#include "GlassWidgets.h"

class RemoverDialog : public GlassModalDialog {
    Q_OBJECT

public:
    explicit RemoverDialog(int imageWidth, int imageHeight, BackgroundRemover* remover, QWidget* parent = nullptr);

    BackgroundRemover::Settings getSettings() const;

private slots:
    void onPresetChanged(int index);
    void onManualSettingChanged();
    void onDownloadClicked();
    void onRunClicked();

private:
    void setupUI();
    void updateSchedulePreview();
    void updateModelStatus();

    int m_imgWidth;
    int m_imgHeight;
    bool m_updatingControls = false;
    BackgroundRemover* m_remover;

    LiquidGlassTabBar* m_presetTabBar = nullptr;
    GlassButton* m_tuneBtn = nullptr;
    GlassPanel* m_tuneDrawer = nullptr;

    LiquidGlassTabBar* m_tileSizeTabBar = nullptr; // Auto (0) | 1024 | 1536 | 2048
    QComboBox* m_modeCombo = nullptr;
    GlassSliderControl* m_overlapSlider = nullptr;  // 10 .. 75 (%)
    GlassSliderControl* m_cleanEpsSlider = nullptr; // 0 .. 50 (x0.001)
    GlassToggleSwitch* m_globalAnchorSwitch = nullptr;
    GlassToggleSwitch* m_ttaSwitch = nullptr;

    QLabel* m_scheduleLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_downloadProgress = nullptr;
    GlassButton* m_downloadBtn = nullptr;
    GlassButton* m_runBtn = nullptr;
    GlassButton* m_cancelBtn = nullptr;
};

#endif // REMOVERDIALOG_H
