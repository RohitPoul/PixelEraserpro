#include "RemoverDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <cmath>

RemoverDialog::RemoverDialog(int imageWidth, int imageHeight, BackgroundRemover* remover, QWidget* parent)
    : GlassModalDialog("ToonOut G4 AI Remover Studio", parent)
    , m_imgWidth(imageWidth)
    , m_imgHeight(imageHeight)
    , m_remover(remover)
{
    setMinimumWidth(560);
    setupUI();
    onPresetChanged(0);
    updateModelStatus();
}

void RemoverDialog::setupUI() {
    const GlassTheme& th = ThemeManager::instance().current();
    QVBoxLayout* root = contentLayout();

    // Hero Glass Card
    GlassPanel* heroCard = new GlassPanel();
    heroCard->setCornerRadius(20.0);
    heroCard->setAccentGlow(true);
    QVBoxLayout* heroLay = new QVBoxLayout(heroCard);
    heroLay->setContentsMargins(18, 16, 18, 16);
    heroLay->setSpacing(6);

    QHBoxLayout* hdrRow = new QHBoxLayout();
    QLabel* titleLbl = new QLabel("ToonOut G4 AI Matting");
    titleLbl->setStyleSheet("color: #ffffff; font-size: 17px; font-weight: 700; background: transparent;");

    QLabel* badgeLbl = new QLabel("G4 · w18 · SmartEdge AA");
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

    QLabel* subLbl = new QLabel(QString("Input Canvas: %1 × %2 px  ·  OOM-Safe Global + Hann Tile Engine")
        .arg(m_imgWidth).arg(m_imgHeight));
    subLbl->setStyleSheet("color: #b4b8c2; font-size: 12px; background: transparent;");
    heroLay->addWidget(subLbl);
    root->addWidget(heroCard);

    // Profile & Schedule Glass Card with Smooth Liquid Glass Sliding Tab Bar
    GlassPanel* profCard = new GlassPanel();
    profCard->setCornerRadius(20.0);
    QVBoxLayout* profLay = new QVBoxLayout(profCard);
    profLay->setContentsMargins(18, 16, 18, 16);
    profLay->setSpacing(12);

    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    GlassPanel* tabPillWrap = new GlassPanel();
    tabPillWrap->setCornerRadius(19.0);
    tabPillWrap->setSubSurface(true);
    tabPillWrap->setElevated(false);
    tabPillWrap->setFixedHeight(38);
    QHBoxLayout* twLay = new QHBoxLayout(tabPillWrap);
    twLay->setContentsMargins(3, 2, 3, 2);

    m_presetTabBar = new LiquidGlassTabBar();
    m_presetTabBar->setPillHeight(34);
    m_presetTabBar->addTab(0, "Auto Best", "", "", "Auto Best (G4) · Global 1536px + 35% Overlap Tiles");
    m_presetTabBar->addTab(1, "Fast", "", "", "Single 1536px Global Pass · No TTA · Lowest Memory");
    m_presetTabBar->addTab(2, "Precision", "", "", "Global 1536px + 50% Overlap Hann Tiles");
    m_presetTabBar->addTab(3, "Ultra", "", "", "Global + 62% Overlap + H-Flip TTA");
    twLay->addWidget(m_presetTabBar);

    topRow->addWidget(tabPillWrap, 1);

    m_tuneBtn = new GlassButton("Tune", GlassButton::Ghost);
    m_tuneBtn->setIconName("tune");
    m_tuneBtn->setCheckable(true);
    m_tuneBtn->setToolTip("Expand manual Crop Size (1024/1536/2048), Overlap, Global Anchor, H-Flip TTA & Edge Clean");
    topRow->addWidget(m_tuneBtn);
    profLay->addLayout(topRow);

    m_scheduleLabel = new QLabel();
    m_scheduleLabel->setWordWrap(true);
    m_scheduleLabel->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: 600; "
        "background-color: rgba(%2,%3,%4,%5); border: 1px solid rgba(%6,%7,%8,%9); "
        "border-radius: 12px; padding: 9px 13px;"
    ).arg(th.badgeText.name())
     .arg(th.badgeBg.red()).arg(th.badgeBg.green()).arg(th.badgeBg.blue()).arg(th.badgeBg.alpha())
     .arg(th.badgeBorder.red()).arg(th.badgeBorder.green()).arg(th.badgeBorder.blue()).arg(th.badgeBorder.alpha()));
    profLay->addWidget(m_scheduleLabel);

    // Collapsible Full infer_g4.py Tune Drawer
    m_tuneDrawer = new GlassPanel();
    m_tuneDrawer->setCornerRadius(16.0);
    m_tuneDrawer->setSubSurface(true);
    m_tuneDrawer->setAccentGlow(false);
    m_tuneDrawer->setElevated(false);

    QVBoxLayout* tuneLay = new QVBoxLayout(m_tuneDrawer);
    tuneLay->setContentsMargins(14, 14, 14, 14);
    tuneLay->setSpacing(12);

    // 1. Crop / Tile Size Selector (Auto | 1024 | 1536 | 2048 — matching infer_g4.py!)
    QHBoxLayout* tileRow = new QHBoxLayout();
    tileRow->setSpacing(10);
    QLabel* tileLbl = new QLabel("Crop / Tile Size");
    tileLbl->setStyleSheet("color: #d4d8e0; font-weight: 600; background: transparent;");

    GlassPanel* tilePillWrap = new GlassPanel();
    tilePillWrap->setCornerRadius(17.0);
    tilePillWrap->setSubSurface(true);
    tilePillWrap->setElevated(false);
    tilePillWrap->setFixedHeight(34);
    QHBoxLayout* tileWrapLay = new QHBoxLayout(tilePillWrap);
    tileWrapLay->setContentsMargins(3, 2, 3, 2);

    m_tileSizeTabBar = new LiquidGlassTabBar();
    m_tileSizeTabBar->setPillHeight(30);
    m_tileSizeTabBar->addTab(0, "Auto", "", "", "Auto Native 1536px crop");
    m_tileSizeTabBar->addTab(1024, "1024", "", "", "1024px high-detail crop (1.5× zoomed into 1536px G4)");
    m_tileSizeTabBar->addTab(1536, "1536", "", "", "1536px native G4 Swin-w18 crop");
    m_tileSizeTabBar->addTab(2048, "2048", "", "", "2048px wide-context crop (fewer tiles on 4K–8K)");
    tileWrapLay->addWidget(m_tileSizeTabBar);

    tileRow->addWidget(tileLbl);
    tileRow->addWidget(tilePillWrap, 1);
    tuneLay->addLayout(tileRow);

    // 2. Pass Strategy
    QHBoxLayout* modeRow = new QHBoxLayout();
    QLabel* modeLbl = new QLabel("Pass Strategy");
    modeLbl->setStyleSheet("color: #d4d8e0; font-weight: 600; background: transparent;");
    m_modeCombo = new QComboBox();
    m_modeCombo->setMinimumHeight(32);
    m_modeCombo->addItems({
        "Global Anchor + Hann Tiles",
        "Single 1536px Global Pass (Lowest RAM/GPU)"
    });
    modeRow->addWidget(modeLbl);
    modeRow->addWidget(m_modeCombo, 1);
    tuneLay->addLayout(modeRow);

    // 3. Tile Overlap (10% .. 75%, exact infer_g4.py 0.10 .. 0.75 range) & Edge Deep Clean
    m_overlapSlider = new GlassSliderControl("Tile Overlap", 10, 75, 35, "%");
    m_cleanEpsSlider = new GlassSliderControl("Edge Deep-Clean (×0.001)", 0, 50, 20);
    m_cleanEpsSlider->setZeroLabel("Off");
    tuneLay->addWidget(m_overlapSlider);
    tuneLay->addWidget(m_cleanEpsSlider);

    // 4. Global Anchor & H-Flip TTA toggles
    QHBoxLayout* anchorRow = new QHBoxLayout();
    QLabel* anchorLbl = new QLabel("Global 1536px Anchor (Prevents interior tile holes)");
    anchorLbl->setStyleSheet("color: #dadde4; font-size: 12px; font-weight: 600; background: transparent;");
    m_globalAnchorSwitch = new GlassToggleSwitch();
    m_globalAnchorSwitch->setChecked(true);
    anchorRow->addWidget(anchorLbl);
    anchorRow->addStretch();
    anchorRow->addWidget(m_globalAnchorSwitch);
    tuneLay->addLayout(anchorRow);

    QHBoxLayout* ttaRow = new QHBoxLayout();
    QLabel* ttaLbl = new QLabel("H-Flip Test-Time Augmentation (2× Passes)");
    ttaLbl->setStyleSheet("color: #dadde4; font-size: 12px; font-weight: 600; background: transparent;");
    m_ttaSwitch = new GlassToggleSwitch();
    m_ttaSwitch->setChecked(false);
    ttaRow->addWidget(ttaLbl);
    ttaRow->addStretch();
    ttaRow->addWidget(m_ttaSwitch);
    tuneLay->addLayout(ttaRow);

    // 5. AAHead status + Reset Auto button (matching infer_g4.py)
    QHBoxLayout* tuneFoot = new QHBoxLayout();
    QLabel* aaLbl = new QLabel("AAHead: G4 Color-Affinity (ONNX)");
    aaLbl->setStyleSheet("color: #9ca2ae; font-size: 11px; font-weight: 600; background: transparent;");
    GlassButton* resetBtn = new GlassButton("Reset Auto", GlassButton::Ghost);
    resetBtn->setFixedHeight(28);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        m_presetTabBar->setCurrentId(0, true);
        onPresetChanged(0);
    });
    tuneFoot->addWidget(aaLbl);
    tuneFoot->addStretch();
    tuneFoot->addWidget(resetBtn);
    tuneLay->addLayout(tuneFoot);

    m_tuneDrawer->setVisible(false);
    profLay->addWidget(m_tuneDrawer);
    root->addWidget(profCard);

    // Model Status Glass Card
    GlassPanel* stCard = new GlassPanel();
    stCard->setCornerRadius(16.0);
    stCard->setAccentGlow(false);
    QVBoxLayout* stLay = new QVBoxLayout(stCard);
    stLay->setContentsMargins(16, 12, 16, 12);
    stLay->setSpacing(8);

    QHBoxLayout* stRow = new QHBoxLayout();
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("background: transparent;");
    m_downloadBtn = new GlassButton("Download (425 MB)", GlassButton::Ghost);
    stRow->addWidget(m_statusLabel, 1);
    stRow->addWidget(m_downloadBtn);
    stLay->addLayout(stRow);

    m_downloadProgress = new QProgressBar();
    m_downloadProgress->setFixedHeight(8);
    m_downloadProgress->setTextVisible(false);
    m_downloadProgress->setVisible(false);
    stLay->addWidget(m_downloadProgress);
    root->addWidget(stCard);

    // Bottom Actions
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addStretch();

    m_cancelBtn = new GlassButton("Cancel", GlassButton::Ghost);
    m_cancelBtn->setMinimumSize(104, 38);

    m_runBtn = new GlassButton("Remove Background", GlassButton::White);
    m_runBtn->setIconName("sparkle");
    m_runBtn->setDefault(true);
    m_runBtn->setMinimumSize(188, 38);

    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_runBtn);
    root->addLayout(btnRow);

    connect(m_presetTabBar, &LiquidGlassTabBar::tabSelected, this, &RemoverDialog::onPresetChanged);
    connect(m_tileSizeTabBar, &LiquidGlassTabBar::tabSelected, this, [this](int) { onManualSettingChanged(); });
    connect(m_tuneBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_tuneDrawer->setVisible(checked);
        adjustSize();
    });
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoverDialog::onManualSettingChanged);
    connect(m_overlapSlider, &GlassSliderControl::valueChanged, this, &RemoverDialog::onManualSettingChanged);
    connect(m_cleanEpsSlider, &GlassSliderControl::valueChanged, this, &RemoverDialog::onManualSettingChanged);
    connect(m_globalAnchorSwitch, &QAbstractButton::toggled, this, &RemoverDialog::onManualSettingChanged);
    connect(m_ttaSwitch, &QAbstractButton::toggled, this, &RemoverDialog::onManualSettingChanged);
    connect(m_downloadBtn, &QPushButton::clicked, this, &RemoverDialog::onDownloadClicked);
    connect(m_runBtn, &QPushButton::clicked, this, &RemoverDialog::onRunClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void RemoverDialog::onPresetChanged(int index) {
    if (m_updatingControls) return;
    m_updatingControls = true;

    switch (index) {
        case 0: // Auto Best (G4)
            m_tileSizeTabBar->setCurrentId(0, true);
            m_modeCombo->setCurrentIndex(0);
            m_overlapSlider->setValue(35);
            m_globalAnchorSwitch->setChecked(true);
            m_ttaSwitch->setChecked(false);
            m_cleanEpsSlider->setValue(20);
            break;
        case 1: // Fast / Low Memory
            m_tileSizeTabBar->setCurrentId(0, true);
            m_modeCombo->setCurrentIndex(1);
            m_overlapSlider->setValue(35);
            m_globalAnchorSwitch->setChecked(true);
            m_ttaSwitch->setChecked(false);
            m_cleanEpsSlider->setValue(20);
            break;
        case 2: // Precision (50% Ov)
            m_tileSizeTabBar->setCurrentId(1536, true);
            m_modeCombo->setCurrentIndex(0);
            m_overlapSlider->setValue(50);
            m_globalAnchorSwitch->setChecked(true);
            m_ttaSwitch->setChecked(false);
            m_cleanEpsSlider->setValue(20);
            break;
        case 3: // Ultra (62% Ov + TTA)
            m_tileSizeTabBar->setCurrentId(1536, true);
            m_modeCombo->setCurrentIndex(0);
            m_overlapSlider->setValue(62);
            m_globalAnchorSwitch->setChecked(true);
            m_ttaSwitch->setChecked(true);
            m_cleanEpsSlider->setValue(15);
            break;
        default:
            if (!m_tuneBtn->isChecked()) {
                m_tuneBtn->setChecked(true);
            }
            break;
    }

    bool tiled = (m_modeCombo->currentIndex() == 0);
    m_overlapSlider->setEnabled(tiled);
    m_tileSizeTabBar->setEnabled(tiled);
    m_updatingControls = false;
    updateSchedulePreview();
}

void RemoverDialog::onManualSettingChanged() {
    if (m_updatingControls) return;
    m_updatingControls = true;
    bool tiled = (m_modeCombo->currentIndex() == 0);
    m_overlapSlider->setEnabled(tiled);
    m_tileSizeTabBar->setEnabled(tiled);
    m_updatingControls = false;
    updateSchedulePreview();
}

BackgroundRemover::Settings RemoverDialog::getSettings() const {
    BackgroundRemover::Settings s;
    s.tileSize = m_tileSizeTabBar ? m_tileSizeTabBar->currentId() : 0;
    s.enableTiling = (m_modeCombo->currentIndex() == 0);
    s.useGlobalAnchor = m_globalAnchorSwitch->isChecked() || !s.enableTiling;
    s.overlap = m_overlapSlider->value() / 100.0f;
    s.ttaHFlip = m_ttaSwitch->isChecked();
    s.deepCleanEps = m_cleanEpsSlider->value() / 1000.0f;
    return s;
}

void RemoverDialog::updateSchedulePreview() {
    BackgroundRemover::Settings s = getSettings();
    const int effTile = (s.tileSize > 0) ? s.tileSize : 1536;
    int tiles = BackgroundRemover::estimateTileCount(m_imgWidth, m_imgHeight, s.tileSize, s.overlap, s.enableTiling);
    int passesPerDir = (tiles == 0) ? 1 : ((s.useGlobalAnchor ? 1 : 0) + tiles);
    int totalPasses = passesPerDir * (s.ttaHFlip ? 2 : 1);

    QString sched;
    if (tiles == 0) {
        sched = QString("Schedule: Single 1536px Global Pass%1  ·  %2 total pass%3  ·  Minimal Memory")
            .arg(s.ttaHFlip ? " + H-Flip TTA" : "")
            .arg(totalPasses)
            .arg(totalPasses > 1 ? "es" : "");
    } else {
        sched = QString("Schedule: %1%2 × %3px Hann Tiles (%4% Ov)%5  ·  %6 passes  ·  Pause/Resume Ready")
            .arg(s.useGlobalAnchor ? "Global 1536px Anchor + " : "")
            .arg(tiles)
            .arg(effTile)
            .arg(static_cast<int>(std::lround(s.overlap * 100)))
            .arg(s.ttaHFlip ? " + H-Flip TTA" : "")
            .arg(totalPasses);
    }
    m_scheduleLabel->setText(sched);
}

void RemoverDialog::updateModelStatus() {
    if (m_remover && m_remover->isModelAvailable()) {
        m_statusLabel->setText("●  G4 FP16 Model Cached Offline (425 MB)");
        m_statusLabel->setStyleSheet("color: #30d158; font-size: 12px; font-weight: 600; background: transparent;");
        m_downloadBtn->setVisible(false);
        m_runBtn->setEnabled(true);
    } else {
        m_statusLabel->setText("●  G4 FP16 Model Not Downloaded (425 MB)");
        m_statusLabel->setStyleSheet("color: #ffd60a; font-size: 12px; font-weight: 600; background: transparent;");
        m_downloadBtn->setVisible(true);
        m_downloadBtn->setEnabled(true);
    }
}

void RemoverDialog::onDownloadClicked() {
    if (!m_remover) return;
    m_downloadBtn->setEnabled(false);
    m_runBtn->setEnabled(false);
    m_downloadProgress->setVisible(true);
    m_downloadProgress->setRange(0, 100);
    m_downloadProgress->setValue(0);
    m_statusLabel->setText("Downloading G4 FP16 model...");
    m_statusLabel->setStyleSheet("color: #c8d2e2; font-size: 12px; font-weight: 600; background: transparent;");

    bool ok = m_remover->downloadModel([this](qint64 received, qint64 total) {
        if (total > 0) {
            int pct = static_cast<int>((received * 100) / total);
            m_downloadProgress->setValue(pct);
            double mbRecv = received / (1024.0 * 1024.0);
            double mbTot = total / (1024.0 * 1024.0);
            m_statusLabel->setText(QString("Downloading G4 Model: %1 / %2 MB (%3%)")
                .arg(mbRecv, 0, 'f', 1)
                .arg(mbTot, 0, 'f', 1)
                .arg(pct));
        }
        QApplication::processEvents();
    });

    m_downloadProgress->setVisible(false);
    updateModelStatus();
    if (!ok) {
        GlassDialog::warning(this, "Download Failed",
            "Could not download g4.onnx from HuggingFace.",
            "Please check your internet connection and try again.");
    }
}

void RemoverDialog::onRunClicked() {
    if (m_remover && !m_remover->isModelAvailable()) {
        bool yes = GlassDialog::confirm(this, "Download G4 Model",
            "The G4 AI Background Remover model (425 MB) is not cached locally yet.",
            "Download it once now for offline use?", "Download Now", "Cancel");
        if (yes) {
            onDownloadClicked();
            if (!m_remover->isModelAvailable()) return;
        } else {
            return;
        }
    }
    accept();
}
