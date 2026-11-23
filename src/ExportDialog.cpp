#include "ExportDialog.h"
#include "ImageProcessor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>

ExportDialog::ExportDialog(ImageProcessor* processor, QWidget* parent)
    : GlassModalDialog("Export Transparent PNG", parent)
    , m_processor(processor)
{
    setMinimumSize(520, 640);
    setupUI();
    updatePreview();
}

void ExportDialog::setupUI() {
    const GlassTheme& th = ThemeManager::instance().current();
    QVBoxLayout* root = contentLayout();

    // Preview Liquid Glass Card
    GlassPanel* previewCard = new GlassPanel();
    previewCard->setCornerRadius(20.0);
    previewCard->setAccentGlow(true);
    QVBoxLayout* prevLay = new QVBoxLayout(previewCard);
    prevLay->setContentsMargins(16, 14, 16, 16);
    prevLay->setSpacing(10);

    QHBoxLayout* hdrRow = new QHBoxLayout();
    QLabel* titleLbl = new QLabel("Export Preview");
    titleLbl->setStyleSheet("color: #ffffff; font-size: 16px; font-weight: 700; background: transparent;");
    QLabel* badgeLbl = new QLabel("Lossless RGBA PNG");
    badgeLbl->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: 700; "
        "background-color: rgba(%2,%3,%4,%5); border: 1px solid rgba(%6,%7,%8,%9); "
        "border-radius: 10px; padding: 3px 10px;"
    ).arg(th.badgeText.name())
     .arg(th.badgeBg.red()).arg(th.badgeBg.green()).arg(th.badgeBg.blue()).arg(th.badgeBg.alpha())
     .arg(th.badgeBorder.red()).arg(th.badgeBorder.green()).arg(th.badgeBorder.blue()).arg(th.badgeBorder.alpha()));
    hdrRow->addWidget(titleLbl);
    hdrRow->addStretch();
    hdrRow->addWidget(badgeLbl);
    prevLay->addLayout(hdrRow);

    m_previewLabel = new QLabel();
    m_previewLabel->setMinimumSize(320, 290);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(
        "background-color: rgba(12, 14, 20, 200); "
        "border: 1px solid rgba(255, 255, 255, 30); "
        "border-radius: 14px;");
    prevLay->addWidget(m_previewLabel, 1);
    root->addWidget(previewCard, 1);

    // Edge Softening Liquid Glass Card
    GlassPanel* softCard = new GlassPanel();
    softCard->setCornerRadius(18.0);
    QVBoxLayout* softLay = new QVBoxLayout(softCard);
    softLay->setContentsMargins(18, 14, 18, 14);
    softLay->setSpacing(6);

    m_softeningSlider = new GlassSliderControl("Edge Softening (Anti-Fringe)", 0, 5, 0);
    m_softeningSlider->setZeroLabel("Off");
    softLay->addWidget(m_softeningSlider);

    QLabel* hintLbl = new QLabel("Higher levels remove light edge halos while preserving smooth alpha gradients.");
    hintLbl->setStyleSheet("color: #9faac0; font-size: 11px; background: transparent;");
    hintLbl->setWordWrap(true);
    softLay->addWidget(hintLbl);
    root->addWidget(softCard);

    // Save Destination Liquid Glass Card
    GlassPanel* pathCard = new GlassPanel();
    pathCard->setCornerRadius(18.0);
    QVBoxLayout* pathWrap = new QVBoxLayout(pathCard);
    pathWrap->setContentsMargins(18, 13, 18, 14);
    pathWrap->setSpacing(8);

    QLabel* pathTitle = new QLabel("Save Location");
    pathTitle->setStyleSheet("color: #c8d2e2; font-size: 11px; font-weight: 700; background: transparent;");
    pathWrap->addWidget(pathTitle);

    QHBoxLayout* pathRow = new QHBoxLayout();
    pathRow->setSpacing(8);
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText("Choose export destination (*.png)...");
    m_pathEdit->setMinimumHeight(34);
    m_browseBtn = new GlassButton("Browse...", GlassButton::Ghost);
    m_browseBtn->setMinimumHeight(34);
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(m_browseBtn);
    pathWrap->addLayout(pathRow);
    root->addWidget(pathCard);

    // Bottom Actions
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addStretch();

    m_cancelBtn = new GlassButton("Cancel", GlassButton::Ghost);
    m_cancelBtn->setMinimumSize(100, 38);

    m_exportBtn = new GlassButton("Export PNG", GlassButton::White);
    m_exportBtn->setIconName("export");
    m_exportBtn->setDefault(true);
    m_exportBtn->setMinimumSize(144, 38);

    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_exportBtn);
    root->addLayout(btnRow);

    connect(m_softeningSlider, &GlassSliderControl::valueChanged, this, &ExportDialog::onSofteningChanged);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ExportDialog::onSofteningChanged(int level) {
    m_softeningLevel = level;
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
        GlassDialog::warning(this, "Save Location Required", "Please choose an export location before saving.");
        return;
    }

    if (!m_exportPath.endsWith(".png", Qt::CaseInsensitive)) {
        m_exportPath += ".png";
    }

    accept();
}

void ExportDialog::updatePreview() {
    if (!m_processor || !m_processor->hasImage()) return;

    cv::Mat processed;
    if (m_softeningLevel > 0) {
        processed = m_processor->applySoftening(m_processor->getCurrentImage(), m_softeningLevel);
    } else {
        processed = m_processor->getCurrentImage();
    }

    cv::Mat rgba;
    cv::cvtColor(processed, rgba, cv::COLOR_BGRA2RGBA);
    QImage img(rgba.data, rgba.cols, rgba.rows,
               static_cast<int>(rgba.step), QImage::Format_RGBA8888);

    QSize targetSz = m_previewLabel->size().expandedTo(QSize(320, 290)) - QSize(20, 20);
    QPixmap pixmap = QPixmap::fromImage(img.copy());
    pixmap = pixmap.scaled(targetSz, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_previewLabel->setPixmap(pixmap);
}
