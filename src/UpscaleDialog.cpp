#include "UpscaleDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QApplication>

UpscaleDialog::UpscaleDialog(QWidget* parent)
    : QDialog(parent)
    , m_upscaler(new Upscaler(this))
{
    setWindowTitle("AI Upscale Image");
    setMinimumSize(440, 340);
    setModal(true);
    setupUI();
    updateModelStatus();
}

void UpscaleDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(18);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // Model selection group
    QGroupBox* modelGroup = new QGroupBox("Select AI Model");
    QVBoxLayout* modelLayout = new QVBoxLayout(modelGroup);
    modelLayout->setSpacing(12);
    modelLayout->setContentsMargins(16, 20, 16, 16);

    m_modelCombo = new QComboBox();
    m_modelCombo->setMinimumHeight(32);
    m_modelCombo->addItem(Upscaler::getModelName(Upscaler::RealESRGAN_x4), static_cast<int>(Upscaler::RealESRGAN_x4));
    m_modelCombo->addItem(Upscaler::getModelName(Upscaler::RealESRGAN_x2), static_cast<int>(Upscaler::RealESRGAN_x2));
    m_modelCombo->addItem(Upscaler::getModelName(Upscaler::RealESRGAN_x4_anime), static_cast<int>(Upscaler::RealESRGAN_x4_anime));
    modelLayout->addWidget(m_modelCombo);

    m_descLabel = new QLabel();
    m_descLabel->setWordWrap(true);
    m_descLabel->setStyleSheet("color: #aaa; font-size: 12px; padding-top: 4px;");
    modelLayout->addWidget(m_descLabel);

    mainLayout->addWidget(modelGroup);

    // Status & Download group
    QGroupBox* statusGroup = new QGroupBox("Model Status");
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->setSpacing(10);
    statusLayout->setContentsMargins(16, 20, 16, 16);

    QHBoxLayout* statusRow = new QHBoxLayout();
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 13px; font-weight: bold;");
    statusRow->addWidget(m_statusLabel, 1);

    m_downloadBtn = new QPushButton("Download Model");
    m_downloadBtn->setMinimumWidth(130);
    m_downloadBtn->setMinimumHeight(32);
    statusRow->addWidget(m_downloadBtn);
    statusLayout->addLayout(statusRow);

    m_downloadProgress = new QProgressBar();
    m_downloadProgress->setRange(0, 100);
    m_downloadProgress->setValue(0);
    m_downloadProgress->setVisible(false);
    statusLayout->addWidget(m_downloadProgress);

    mainLayout->addWidget(statusGroup);

    mainLayout->addStretch();

    // Bottom buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    m_cancelBtn = new QPushButton("Cancel");
    m_cancelBtn->setMinimumWidth(100);
    m_cancelBtn->setMinimumHeight(36);

    m_upscaleBtn = new QPushButton("Upscale");
    m_upscaleBtn->setDefault(true);
    m_upscaleBtn->setMinimumWidth(100);
    m_upscaleBtn->setMinimumHeight(36);
    m_upscaleBtn->setStyleSheet("background-color: #506090; font-weight: bold;");

    buttonLayout->addWidget(m_cancelBtn);
    buttonLayout->addWidget(m_upscaleBtn);
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UpscaleDialog::onModelChanged);
    connect(m_downloadBtn, &QPushButton::clicked, this, &UpscaleDialog::onDownloadClicked);
    connect(m_upscaleBtn, &QPushButton::clicked, this, &UpscaleDialog::onUpscaleClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

Upscaler::Model UpscaleDialog::getSelectedModel() const {
    return static_cast<Upscaler::Model>(m_modelCombo->currentData().toInt());
}

int UpscaleDialog::getScale() const {
    return Upscaler::getModelScale(getSelectedModel());
}

void UpscaleDialog::onModelChanged(int /*index*/) {
    updateModelStatus();
}

void UpscaleDialog::updateModelStatus() {
    Upscaler::Model model = getSelectedModel();
    m_descLabel->setText(Upscaler::getModelDescription(model));

    if (m_upscaler->isModelAvailable(model)) {
        m_statusLabel->setText("Model Ready (Downloaded)");
        m_statusLabel->setStyleSheet("color: #60c080; font-size: 13px; font-weight: bold;");
        m_downloadBtn->setVisible(false);
        m_upscaleBtn->setEnabled(true);
    } else {
        m_statusLabel->setText("Model Not Downloaded");
        m_statusLabel->setStyleSheet("color: #e0a050; font-size: 13px; font-weight: bold;");
        m_downloadBtn->setVisible(true);
        m_downloadBtn->setEnabled(true);
    }
}

void UpscaleDialog::onDownloadClicked() {
    Upscaler::Model model = getSelectedModel();

    m_downloadBtn->setEnabled(false);
    m_modelCombo->setEnabled(false);
    m_upscaleBtn->setEnabled(false);
    m_downloadProgress->setVisible(true);
    m_downloadProgress->setValue(0);
    m_statusLabel->setText("Downloading model...");
    m_statusLabel->setStyleSheet("color: #70a0d0; font-size: 13px; font-weight: bold;");

    bool ok = m_upscaler->downloadModel(model, [this](qint64 received, qint64 total) {
        if (total > 0) {
            int pct = static_cast<int>((received * 100) / total);
            m_downloadProgress->setValue(pct);
            double mbRecv = received / (1024.0 * 1024.0);
            double mbTot = total / (1024.0 * 1024.0);
            m_statusLabel->setText(QString("Downloading: %1 / %2 MB (%3%)")
                .arg(mbRecv, 0, 'f', 1)
                .arg(mbTot, 0, 'f', 1)
                .arg(pct));
        }
        QApplication::processEvents();
    });

    m_downloadProgress->setVisible(false);
    m_modelCombo->setEnabled(true);

    if (ok) {
        updateModelStatus();
    } else {
        updateModelStatus();
        QMessageBox::warning(this, "Download Failed",
            "Failed to download the AI upscaling model.\nPlease check your internet connection and try again.");
    }
}

void UpscaleDialog::onUpscaleClicked() {
    Upscaler::Model model = getSelectedModel();
    if (!m_upscaler->isModelAvailable(model)) {
        auto reply = QMessageBox::question(this, "Model Required",
            "The selected AI model is not downloaded yet.\nWould you like to download it now?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply == QMessageBox::Yes) {
            onDownloadClicked();
            if (!m_upscaler->isModelAvailable(model)) {
                return;
            }
        } else {
            return;
        }
    }
    accept();
}
