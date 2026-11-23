#include "UpscaleDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>

UpscaleDialog::UpscaleDialog(QWidget* parent)
    : GlassModalDialog("AI Upscale Studio", parent)
    , m_upscaler(new Upscaler(this))
{
    setMinimumWidth(500);
    setupUI();
    updateModelStatus();
}

void UpscaleDialog::setupUI() {
    const GlassTheme& th = ThemeManager::instance().current();
    QVBoxLayout* root = contentLayout();

    // Hero Header Liquid Glass Card
    GlassPanel* heroCard = new GlassPanel();
    heroCard->setCornerRadius(20.0);
    heroCard->setAccentGlow(true);
    QVBoxLayout* heroLay = new QVBoxLayout(heroCard);
    heroLay->setContentsMargins(18, 16, 18, 16);
    heroLay->setSpacing(6);

    QHBoxLayout* hdrRow = new QHBoxLayout();
    QLabel* titleLbl = new QLabel("AI Super-Resolution");
    titleLbl->setStyleSheet("color: #ffffff; font-size: 17px; font-weight: 700; background: transparent;");

    QLabel* badgeLbl = new QLabel("Real-ESRGAN · ONNX");
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
    heroLay->addLayout(hdrRow);

    QLabel* subLbl = new QLabel("Enhance fine lines, textures, and crisp transparent edges using neural upscaling.");
    subLbl->setWordWrap(true);
    subLbl->setStyleSheet("color: #b4c0d2; font-size: 12px; background: transparent;");
    heroLay->addWidget(subLbl);
    root->addWidget(heroCard);

    // Model Selection Card with Smooth Sliding LiquidGlassTabBar
    GlassPanel* modelCard = new GlassPanel();
    modelCard->setCornerRadius(20.0);
    QVBoxLayout* modelLay = new QVBoxLayout(modelCard);
    modelLay->setContentsMargins(18, 16, 18, 16);
    modelLay->setSpacing(12);

    QLabel* pickLbl = new QLabel("Model Architecture");
    pickLbl->setStyleSheet("color: #c8d2e2; font-size: 11px; font-weight: 700; background: transparent;");
    modelLay->addWidget(pickLbl);

    GlassPanel* tabWrap = new GlassPanel();
    tabWrap->setCornerRadius(19.0);
    tabWrap->setSubSurface(true);
    tabWrap->setElevated(false);
    tabWrap->setFixedHeight(38);
    QHBoxLayout* twLay = new QHBoxLayout(tabWrap);
    twLay->setContentsMargins(3, 2, 3, 2);

    m_modelTabBar = new LiquidGlassTabBar();
    m_modelTabBar->setPillHeight(34);
    m_modelTabBar->addTab(static_cast<int>(Upscaler::RealESRGAN_x4), "4× General", "", "",
                          Upscaler::getModelName(Upscaler::RealESRGAN_x4));
    m_modelTabBar->addTab(static_cast<int>(Upscaler::RealESRGAN_x2), "2× General", "", "",
                          Upscaler::getModelName(Upscaler::RealESRGAN_x2));
    m_modelTabBar->addTab(static_cast<int>(Upscaler::RealESRGAN_x4_anime), "4× Anime", "", "",
                          Upscaler::getModelName(Upscaler::RealESRGAN_x4_anime));
    twLay->addWidget(m_modelTabBar);
    modelLay->addWidget(tabWrap);

    m_descLabel = new QLabel();
    m_descLabel->setWordWrap(true);
    m_descLabel->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: 600; "
        "background-color: rgba(%2,%3,%4,%5); border: 1px solid rgba(%6,%7,%8,%9); "
        "border-radius: 12px; padding: 9px 13px;"
    ).arg(th.badgeText.name())
     .arg(th.badgeBg.red()).arg(th.badgeBg.green()).arg(th.badgeBg.blue()).arg(th.badgeBg.alpha())
     .arg(th.badgeBorder.red()).arg(th.badgeBorder.green()).arg(th.badgeBorder.blue()).arg(th.badgeBorder.alpha()));
    modelLay->addWidget(m_descLabel);
    root->addWidget(modelCard);

    // Model Cache Status Card
    GlassPanel* stCard = new GlassPanel();
    stCard->setCornerRadius(16.0);
    stCard->setAccentGlow(false);
    QVBoxLayout* stLay = new QVBoxLayout(stCard);
    stLay->setContentsMargins(16, 12, 16, 12);
    stLay->setSpacing(8);

    QHBoxLayout* stRow = new QHBoxLayout();
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("background: transparent;");
    m_downloadBtn = new GlassButton("Download Model", GlassButton::Ghost);
    stRow->addWidget(m_statusLabel, 1);
    stRow->addWidget(m_downloadBtn);
    stLay->addLayout(stRow);

    m_downloadProgress = new QProgressBar();
    m_downloadProgress->setFixedHeight(8);
    m_downloadProgress->setTextVisible(false);
    m_downloadProgress->setRange(0, 100);
    m_downloadProgress->setValue(0);
    m_downloadProgress->setVisible(false);
    stLay->addWidget(m_downloadProgress);
    root->addWidget(stCard);

    // Bottom Actions
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addStretch();

    m_cancelBtn = new GlassButton("Cancel", GlassButton::Ghost);
    m_cancelBtn->setMinimumSize(100, 38);

    m_upscaleBtn = new GlassButton("Upscale Image", GlassButton::Primary);
    m_upscaleBtn->setIconName("upscale");
    m_upscaleBtn->setDefault(true);
    m_upscaleBtn->setMinimumSize(156, 38);

    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_upscaleBtn);
    root->addLayout(btnRow);

    connect(m_modelTabBar, &LiquidGlassTabBar::tabSelected, this, &UpscaleDialog::onModelChanged);
    connect(m_downloadBtn, &QPushButton::clicked, this, &UpscaleDialog::onDownloadClicked);
    connect(m_upscaleBtn, &QPushButton::clicked, this, &UpscaleDialog::onUpscaleClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

Upscaler::Model UpscaleDialog::getSelectedModel() const {
    return static_cast<Upscaler::Model>(m_modelTabBar->currentId());
}

int UpscaleDialog::getScale() const {
    return Upscaler::getModelScale(getSelectedModel());
}

void UpscaleDialog::onModelChanged(int /*id*/) {
    updateModelStatus();
}

void UpscaleDialog::updateModelStatus() {
    Upscaler::Model model = getSelectedModel();
    m_descLabel->setText(QString("%1  ·  %2")
        .arg(Upscaler::getModelName(model))
        .arg(Upscaler::getModelDescription(model)));

    if (m_upscaler->isModelAvailable(model)) {
        m_statusLabel->setText("●  Model Cached Offline & Ready");
        m_statusLabel->setStyleSheet("color: #30d158; font-size: 12px; font-weight: 600; background: transparent;");
        m_downloadBtn->setVisible(false);
        m_upscaleBtn->setEnabled(true);
    } else {
        m_statusLabel->setText("●  Model Not Downloaded Yet");
        m_statusLabel->setStyleSheet("color: #ffd60a; font-size: 12px; font-weight: 600; background: transparent;");
        m_downloadBtn->setVisible(true);
        m_downloadBtn->setEnabled(true);
    }
}

void UpscaleDialog::onDownloadClicked() {
    Upscaler::Model model = getSelectedModel();

    m_downloadBtn->setEnabled(false);
    m_modelTabBar->setEnabled(false);
    m_upscaleBtn->setEnabled(false);
    m_downloadProgress->setVisible(true);
    m_downloadProgress->setValue(0);
    m_statusLabel->setText("Downloading model...");
    m_statusLabel->setStyleSheet("color: #64d2ff; font-size: 12px; font-weight: 600; background: transparent;");

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
    m_modelTabBar->setEnabled(true);

    if (ok) {
        updateModelStatus();
    } else {
        updateModelStatus();
        GlassDialog::warning(this, "Download Failed",
            "Failed to download the AI upscaling model.",
            "Please check your internet connection and try again.");
    }
}

void UpscaleDialog::onUpscaleClicked() {
    Upscaler::Model model = getSelectedModel();
    if (!m_upscaler->isModelAvailable(model)) {
        bool yes = GlassDialog::confirm(this, "Model Required",
            "The selected Real-ESRGAN AI model is not downloaded yet.",
            "Would you like to download it now?", "Download Now", "Cancel");
        if (yes) {
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
