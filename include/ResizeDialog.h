#ifndef RESIZEDIALOG_H
#define RESIZEDIALOG_H

#include <QSpinBox>
#include <QLabel>
#include "GlassWidgets.h"

class ResizeDialog : public GlassModalDialog {
    Q_OBJECT

public:
    explicit ResizeDialog(int currentWidth, int currentHeight, QWidget* parent = nullptr);

    int getNewWidth() const { return m_newWidth; }
    int getNewHeight() const { return m_newHeight; }

private slots:
    void onWidthChanged(int value);
    void onHeightChanged(int value);
    void onPresetClicked(int percent);

private:
    void setupUI();

    int m_originalWidth;
    int m_originalHeight;
    int m_newWidth;
    int m_newHeight;
    double m_aspectRatio;
    bool m_lockAspect = true;
    bool m_updating = false;

    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    GlassToggleSwitch* m_lockSwitch = nullptr;
    LiquidGlassTabBar* m_presetTabBar = nullptr;
    QLabel* m_previewLabel = nullptr;
};

#endif // RESIZEDIALOG_H
