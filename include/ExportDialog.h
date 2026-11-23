#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QLabel>
#include <QImage>
#include <QLineEdit>
#include <opencv2/opencv.hpp>
#include "GlassWidgets.h"

class ImageProcessor;

class ExportDialog : public GlassModalDialog {
    Q_OBJECT

public:
    explicit ExportDialog(ImageProcessor* processor, QWidget* parent = nullptr);

    int getSofteningLevel() const { return m_softeningLevel; }
    QString getExportPath() const { return m_exportPath; }

private slots:
    void onSofteningChanged(int level);
    void onBrowseClicked();
    void onExportClicked();
    void updatePreview();

private:
    void setupUI();

    ImageProcessor* m_processor;

    QLabel* m_previewLabel = nullptr;
    GlassSliderControl* m_softeningSlider = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    GlassButton* m_browseBtn = nullptr;
    GlassButton* m_exportBtn = nullptr;
    GlassButton* m_cancelBtn = nullptr;

    int m_softeningLevel = 0;
    QString m_exportPath;
    QImage m_previewImage;
};

#endif // EXPORTDIALOG_H
