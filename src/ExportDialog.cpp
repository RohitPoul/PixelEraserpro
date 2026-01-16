#include "ExportDialog.h"
#include "ImageProcessor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>

ExportDialog::ExportDialog(ImageProcessor* processor, QWidget* parent)
    : QDialog(parent)
    , m_processor(processor)
{
    setWindowTitle("Export Image");
    setMinimumSize(500, 600);
    setModal(true);
    setupUI();
    updatePreview();
}

void ExportDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Preview section
    QGroupBox* previewGroup = new QGroupBox("Preview");
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewLabel = new QLabel();
    m_previewLabel->setMinimumSize(300, 300);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: #2a2a2e; border-radius: 8px;");
    previewLayout->addWidget(m_previewLabel);
    mainLayout->addWidget(previewGroup);

    // Edge Softening section
    QGroupBox* softenGroup = new QGroupBox("Edge Softening (Removes white fringing)");
    QVBoxLayout* softenLayout = new QVBoxLayout(softenGroup);
    
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    QLabel* offLabel = new QLabel("Off");
    m_softeningSlider = new QSlider(Qt::Horizontal);
    m_softeningSlider->setRange(0, 5);
    m_softeningSlider->setValue(0);
    m_softeningSlider->setTickPosition(QSlider::TicksBelow);
    m_softeningSlider->setTickInterval(1);
    QLabel* maxLabel = new QLabel("Max");
    m_softeningValueLabel = new QLabel("0");
    m_softeningValueLabel->setMinimumWidth(30);
    m_softeningValueLabel->setAlignment(Qt::AlignCenter);
    
    sliderLayout->addWidget(offLabel);
    sliderLayout->addWidget(m_softeningSlider);
    sliderLayout->addWidget(maxLabel);
    sliderLayout->addWidget(m_softeningValueLabel);
    softenLayout->addLayout(sliderLayout);
    
    QLabel* hintLabel = new QLabel("Higher values remove more edge artifacts but may blur fine details.");
    hintLabel->setStyleSheet("color: #888; font-size: 11px;");
    hintLabel->setWordWrap(true);
    softenLayout->addWidget(hintLabel);
    
    mainLayout->addWidget(softenGroup);

    // File path section
    QGroupBox* pathGroup = new QGroupBox("Save Location");
    QHBoxLayout* pathLayout = new QHBoxLayout(pathGroup);
    
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText("Select export location...");
    m_browseBtn = new QPushButton("Browse...");
    
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(m_browseBtn);
    mainLayout->addWidget(pathGroup);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_cancelBtn = new QPushButton("Cancel");
    m_exportBtn = new QPushButton("Export");
    m_exportBtn->setDefault(true);
    m_exportBtn->setStyleSheet("background-color: #506090; font-weight: bold;");
    
    buttonLayout->addWidget(m_cancelBtn);
    buttonLayout->addWidget(m_exportBtn);
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_softeningSlider, &QSlider::valueChanged, this, &ExportDialog::onSofteningChanged);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ExportDialog::onSofteningChanged(int level) {
    m_softeningLevel = level;
    m_softeningValueLabel->setText(QString::number(level));
    updatePreview();
}

void ExportDialog::onBrowseClicked() {
    QString filename = QFileDialog::getSaveFileName(
        this, "Export Image", QString(), "PNG Image (*.png)"
    );
    
    if (!filename.isEmpty()) {
        if (!filename.endsWith(".png", Qt::CaseInsensitive)) {
            filename += ".png";
        }
        m_pathEdit->setText(filename);
        m_exportPath = filename;
    }
}

void ExportDialog::onExportClicked() {
    m_exportPath = m_pathEdit->text();
    
    if (m_exportPath.isEmpty()) {
        QMessageBox::warning(this, "Export", "Please select a save location.");
        return;
    }
    
    if (!m_exportPath.endsWith(".png", Qt::CaseInsensitive)) {
        m_exportPath += ".png";
    }
    
    accept();
}

void ExportDialog::updatePreview() {
    if (!m_processor || !m_processor->hasImage()) return;
    
    // Get image with softening applied
    cv::Mat processed;
    if (m_softeningLevel > 0) {
        processed = m_processor->applySoftening(m_processor->getCurrentImage(), m_softeningLevel);
    } else {
        processed = m_processor->getCurrentImage();
    }
    
    // Convert to QImage
    cv::Mat rgba;
    cv::cvtColor(processed, rgba, cv::COLOR_BGRA2RGBA);
    QImage img(rgba.data, rgba.cols, rgba.rows, 
               static_cast<int>(rgba.step), QImage::Format_RGBA8888);
    
    // Scale to fit preview
    QPixmap pixmap = QPixmap::fromImage(img.copy());
    pixmap = pixmap.scaled(m_previewLabel->size() - QSize(20, 20), 
                          Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    m_previewLabel->setPixmap(pixmap);
}
