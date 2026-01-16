#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QImage>
#include <QLineEdit>
#include <opencv2/opencv.hpp>

class ImageProcessor;

class ExportDialog : public QDialog {
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
    
    QLabel* m_previewLabel;
    QSlider* m_softeningSlider;
    QLabel* m_softeningValueLabel;
    QLineEdit* m_pathEdit;
    QPushButton* m_browseBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_cancelBtn;
    
    int m_softeningLevel = 0;
    QString m_exportPath;
    QImage m_previewImage;
};

#endif // EXPORTDIALOG_H
