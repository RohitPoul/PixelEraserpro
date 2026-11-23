#ifndef UPSCALEDIALOG_H
#define UPSCALEDIALOG_H

#include <QLabel>
#include <QProgressBar>
#include "Upscaler.h"
#include "GlassWidgets.h"

class UpscaleDialog : public GlassModalDialog {
    Q_OBJECT

public:
    explicit UpscaleDialog(QWidget* parent = nullptr);

    Upscaler::Model getSelectedModel() const;
    int getScale() const;

private slots:
    void onModelChanged(int id);
    void onDownloadClicked();
    void onUpscaleClicked();

private:
    void setupUI();
    void updateModelStatus();

    Upscaler* m_upscaler;
    LiquidGlassTabBar* m_modelTabBar = nullptr;
    QLabel* m_descLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_downloadProgress = nullptr;
    GlassButton* m_downloadBtn = nullptr;
    GlassButton* m_upscaleBtn = nullptr;
    GlassButton* m_cancelBtn = nullptr;
};

#endif // UPSCALEDIALOG_H
