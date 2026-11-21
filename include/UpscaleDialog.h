#ifndef UPSCALEDIALOG_H
#define UPSCALEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include "Upscaler.h"

class UpscaleDialog : public QDialog {
    Q_OBJECT

public:
    explicit UpscaleDialog(QWidget* parent = nullptr);

    Upscaler::Model getSelectedModel() const;
    int getScale() const;

private slots:
    void onModelChanged(int index);
    void onDownloadClicked();
    void onUpscaleClicked();

private:
    void setupUI();
    void updateModelStatus();

    Upscaler* m_upscaler;
    QComboBox* m_modelCombo;
    QLabel* m_descLabel;
    QLabel* m_statusLabel;
    QProgressBar* m_downloadProgress;
    QPushButton* m_downloadBtn;
    QPushButton* m_upscaleBtn;
    QPushButton* m_cancelBtn;
};

#endif // UPSCALEDIALOG_H
