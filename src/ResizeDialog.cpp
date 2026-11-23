#include "ResizeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <algorithm>

ResizeDialog::ResizeDialog(int currentWidth, int currentHeight, QWidget* parent)
    : GlassModalDialog("Resize Canvas", parent)
    , m_originalWidth(currentWidth)
    , m_originalHeight(currentHeight)
    , m_newWidth(currentWidth)
    , m_newHeight(currentHeight)
    , m_aspectRatio(static_cast<double>(currentWidth) / std::max(1, currentHeight))
{
    setMinimumWidth(480);
    setupUI();
}

void ResizeDialog::setupUI() {
    const GlassTheme& th = ThemeManager::instance().current();
    QVBoxLayout* root = contentLayout();

    // Header Liquid Glass Card
    GlassPanel* heroCard = new GlassPanel();
    heroCard->setCornerRadius(20.0);
    heroCard->setAccentGlow(true);
    QVBoxLayout* heroLay = new QVBoxLayout(heroCard);
    heroLay->setContentsMargins(18, 15, 18, 15);
    heroLay->setSpacing(6);

    QHBoxLayout* hdrRow = new QHBoxLayout();
    QLabel* titleLbl = new QLabel("Resize Canvas");
    titleLbl->setStyleSheet("color: #ffffff; font-size: 17px; font-weight: 700; background: transparent;");

    QLabel* badgeLbl = new QLabel("Lanczos4 · High Precision");
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

    QLabel* curLbl = new QLabel(QString("Original Canvas: %1 × %2 px").arg(m_originalWidth).arg(m_originalHeight));
    curLbl->setStyleSheet("color: #b4c0d2; font-size: 12px; background: transparent;");
    heroLay->addWidget(curLbl);
    root->addWidget(heroCard);

    // Dimensions Liquid Glass Card
    GlassPanel* dimCard = new GlassPanel();
    dimCard->setCornerRadius(20.0);
    QVBoxLayout* dimLay = new QVBoxLayout(dimCard);
    dimLay->setContentsMargins(18, 16, 18, 16);
    dimLay->setSpacing(14);

    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);

    QLabel* wLbl = new QLabel("Width");
    wLbl->setStyleSheet("color: #cfd8e8; font-weight: 600; background: transparent;");
    m_widthSpin = new QSpinBox();
    m_widthSpin->setRange(1, 16384);
    m_widthSpin->setValue(m_originalWidth);
    m_widthSpin->setSuffix(" px");
    m_widthSpin->setMinimumHeight(34);

    QLabel* hLbl = new QLabel("Height");
    hLbl->setStyleSheet("color: #cfd8e8; font-weight: 600; background: transparent;");
    m_heightSpin = new QSpinBox();
    m_heightSpin->setRange(1, 16384);
    m_heightSpin->setValue(m_originalHeight);
    m_heightSpin->setSuffix(" px");
    m_heightSpin->setMinimumHeight(34);

    grid->addWidget(wLbl, 0, 0);
    grid->addWidget(m_widthSpin, 0, 1);
    grid->addWidget(hLbl, 1, 0);
    grid->addWidget(m_heightSpin, 1, 1);
    grid->setColumnStretch(1, 1);
    dimLay->addLayout(grid);

    QHBoxLayout* lockRow = new QHBoxLayout();
    QLabel* lockLbl = new QLabel("Lock Aspect Ratio");
    lockLbl->setStyleSheet("color: #dfe6f2; font-size: 12px; font-weight: 600; background: transparent;");
    m_lockSwitch = new GlassToggleSwitch();
    m_lockSwitch->setChecked(true);
    lockRow->addWidget(lockLbl);
    lockRow->addStretch();
    lockRow->addWidget(m_lockSwitch);
    dimLay->addLayout(lockRow);

    // Quick Scale Liquid Glass Sliding Tab Bar
    QLabel* quickLbl = new QLabel("Quick Scale");
    quickLbl->setStyleSheet("color: #b4c0d2; font-size: 11px; font-weight: 700; background: transparent;");
    dimLay->addWidget(quickLbl);

    GlassPanel* tabWrap = new GlassPanel();
    tabWrap->setCornerRadius(19.0);
    tabWrap->setSubSurface(true);
    tabWrap->setElevated(false);
    tabWrap->setFixedHeight(38);
    QHBoxLayout* twLay = new QHBoxLayout(tabWrap);
    twLay->setContentsMargins(3, 2, 3, 2);

    m_presetTabBar = new LiquidGlassTabBar();
    m_presetTabBar->setPillHeight(34);
    m_presetTabBar->addTab(25, "25%");
    m_presetTabBar->addTab(50, "50%");
    m_presetTabBar->addTab(75, "75%");
    m_presetTabBar->addTab(100, "100%");
    m_presetTabBar->addTab(150, "150%");
    m_presetTabBar->addTab(200, "200%");
    m_presetTabBar->setCurrentId(100, false);
    twLay->addWidget(m_presetTabBar);
    dimLay->addWidget(tabWrap);

    m_previewLabel = new QLabel(QString("Output Resolution: %1 × %2 px").arg(m_newWidth).arg(m_newHeight));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(QString(
        "color: %1; font-size: 12px; font-weight: 700; "
        "background-color: rgba(%2,%3,%4,%5); border: 1px solid rgba(%6,%7,%8,%9); "
        "border-radius: 12px; padding: 9px 14px;"
    ).arg(th.badgeText.name())
     .arg(th.badgeBg.red()).arg(th.badgeBg.green()).arg(th.badgeBg.blue()).arg(th.badgeBg.alpha())
     .arg(th.badgeBorder.red()).arg(th.badgeBorder.green()).arg(th.badgeBorder.blue()).arg(th.badgeBorder.alpha()));
    dimLay->addWidget(m_previewLabel);
    root->addWidget(dimCard);

    // Bottom Actions
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addStretch();

    GlassButton* cancelBtn = new GlassButton("Cancel", GlassButton::Ghost);
    cancelBtn->setMinimumSize(100, 38);

    GlassButton* resizeBtn = new GlassButton("Apply Resize", GlassButton::Primary);
    resizeBtn->setIconName("resize");
    resizeBtn->setDefault(true);
    resizeBtn->setMinimumSize(148, 38);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(resizeBtn);
    root->addLayout(btnRow);

    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResizeDialog::onWidthChanged);
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResizeDialog::onHeightChanged);
    connect(m_lockSwitch, &QAbstractButton::toggled, this, [this](bool checked) { m_lockAspect = checked; });
    connect(m_presetTabBar, &LiquidGlassTabBar::tabSelected, this, &ResizeDialog::onPresetClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(resizeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ResizeDialog::onWidthChanged(int value) {
    if (m_updating) return;
    m_updating = true;

    m_newWidth = value;
    if (m_lockAspect) {
        m_newHeight = std::max(1, static_cast<int>(value / m_aspectRatio));
        m_heightSpin->setValue(m_newHeight);
    } else {
        m_newHeight = m_heightSpin->value();
    }

    m_previewLabel->setText(QString("Output Resolution: %1 × %2 px").arg(m_newWidth).arg(m_newHeight));
    m_updating = false;
}

void ResizeDialog::onHeightChanged(int value) {
    if (m_updating) return;
    m_updating = true;

    m_newHeight = value;
    if (m_lockAspect) {
        m_newWidth = std::max(1, static_cast<int>(value * m_aspectRatio));
        m_widthSpin->setValue(m_newWidth);
    } else {
        m_newWidth = m_widthSpin->value();
    }

    m_previewLabel->setText(QString("Output Resolution: %1 × %2 px").arg(m_newWidth).arg(m_newHeight));
    m_updating = false;
}

void ResizeDialog::onPresetClicked(int percent) {
    m_updating = true;
    m_newWidth = std::max(1, m_originalWidth * percent / 100);
    m_newHeight = std::max(1, m_originalHeight * percent / 100);
    m_widthSpin->setValue(m_newWidth);
    m_heightSpin->setValue(m_newHeight);
    m_previewLabel->setText(QString("Output Resolution: %1 × %2 px").arg(m_newWidth).arg(m_newHeight));
    m_updating = false;
}
