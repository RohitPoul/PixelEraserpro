#ifndef RESIZEDIALOG_H
#define RESIZEDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>

class ResizeDialog : public QDialog {
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
    
    QSpinBox* m_widthSpin;
    QSpinBox* m_heightSpin;
    QCheckBox* m_lockCheck;
    QLabel* m_previewLabel;
};

#endif // RESIZEDIALOG_H
