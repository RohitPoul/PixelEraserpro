#include "ResizeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>

ResizeDialog::ResizeDialog(int currentWidth, int currentHeight, QWidget* parent)
    : QDialog(parent)
    , m_originalWidth(currentWidth)
    , m_originalHeight(currentHeight)
    , m_newWidth(currentWidth)
    , m_newHeight(currentHeight)
    , m_aspectRatio(static_cast<double>(currentWidth) / currentHeight)
{
    setWindowTitle("Resize Image");
    setMinimumSize(400, 380);
    setModal(true);
    setupUI();
}

void ResizeDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // Current size info
    QLabel* currentLabel = new QLabel(QString("Current Size: %1 × %2 pixels")
        .arg(m_originalWidth).arg(m_originalHeight));
    currentLabel->setStyleSheet("color: #aaa; font-size: 13px;");
    mainLayout->addWidget(currentLabel);

    // Size inputs
    QGroupBox* sizeGroup = new QGroupBox("New Size");
    QGridLayout* sizeLayout = new QGridLayout(sizeGroup);
    sizeLayout->setSpacing(12);
    sizeLayout->setContentsMargins(16, 20, 16, 16);
    
    QLabel* widthLabel = new QLabel("Width:");
    widthLabel->setMinimumWidth(60);
    sizeLayout->addWidget(widthLabel, 0, 0);
    
    m_widthSpin = new QSpinBox();
    m_widthSpin->setRange(1, 16384);
    m_widthSpin->setValue(m_originalWidth);
    m_widthSpin->setSuffix(" px");
    m_widthSpin->setMinimumWidth(120);
    m_widthSpin->setMaximumWidth(150);
    sizeLayout->addWidget(m_widthSpin, 0, 1);
    
    QLabel* heightLabel = new QLabel("Height:");
    sizeLayout->addWidget(heightLabel, 1, 0);
    
    m_heightSpin = new QSpinBox();
    m_heightSpin->setRange(1, 16384);
    m_heightSpin->setValue(m_originalHeight);
    m_heightSpin->setSuffix(" px");
    m_heightSpin->setMinimumWidth(120);
    m_heightSpin->setMaximumWidth(150);
    sizeLayout->addWidget(m_heightSpin, 1, 1);
    
    // Add stretch to push spinboxes left
    sizeLayout->setColumnStretch(2, 1);
    
    m_lockCheck = new QCheckBox("Lock aspect ratio");
    m_lockCheck->setChecked(true);
    sizeLayout->addWidget(m_lockCheck, 2, 0, 1, 3);
    
    mainLayout->addWidget(sizeGroup);

    // Quick presets
    QGroupBox* presetGroup = new QGroupBox("Quick Resize");
    QHBoxLayout* presetLayout = new QHBoxLayout(presetGroup);
    presetLayout->setSpacing(8);
    presetLayout->setContentsMargins(16, 20, 16, 16);
    
    QPushButton* btn25 = new QPushButton("25%");
    QPushButton* btn50 = new QPushButton("50%");
    QPushButton* btn75 = new QPushButton("75%");
    QPushButton* btn150 = new QPushButton("150%");
    QPushButton* btn200 = new QPushButton("200%");
    
    QString presetStyle = "QPushButton { padding: 8px 12px; min-width: 50px; }";
    btn25->setStyleSheet(presetStyle);
    btn50->setStyleSheet(presetStyle);
    btn75->setStyleSheet(presetStyle);
    btn150->setStyleSheet(presetStyle);
    btn200->setStyleSheet(presetStyle);
    
    presetLayout->addWidget(btn25);
    presetLayout->addWidget(btn50);
    presetLayout->addWidget(btn75);
    presetLayout->addWidget(btn150);
    presetLayout->addWidget(btn200);
    
    mainLayout->addWidget(presetGroup);

    // Preview
    m_previewLabel = new QLabel(QString("Output: %1 × %2 pixels").arg(m_newWidth).arg(m_newHeight));
    m_previewLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #70a0d0; padding: 12px;");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_previewLabel);

    mainLayout->addStretch();

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();
    
    QPushButton* cancelBtn = new QPushButton("Cancel");
    cancelBtn->setMinimumWidth(100);
    cancelBtn->setMinimumHeight(36);
    
    QPushButton* resizeBtn = new QPushButton("Resize");
    resizeBtn->setDefault(true);
    resizeBtn->setMinimumWidth(100);
    resizeBtn->setMinimumHeight(36);
    resizeBtn->setStyleSheet("background-color: #506090; font-weight: bold;");
    
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(resizeBtn);
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResizeDialog::onWidthChanged);
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResizeDialog::onHeightChanged);
    connect(m_lockCheck, &QCheckBox::toggled, [this](bool checked) { m_lockAspect = checked; });
    
    connect(btn25, &QPushButton::clicked, [this]() { onPresetClicked(25); });
    connect(btn50, &QPushButton::clicked, [this]() { onPresetClicked(50); });
    connect(btn75, &QPushButton::clicked, [this]() { onPresetClicked(75); });
    connect(btn150, &QPushButton::clicked, [this]() { onPresetClicked(150); });
    connect(btn200, &QPushButton::clicked, [this]() { onPresetClicked(200); });
    
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(resizeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ResizeDialog::onWidthChanged(int value) {
    if (m_updating) return;
    m_updating = true;
    
    m_newWidth = value;
    if (m_lockAspect) {
        m_newHeight = static_cast<int>(value / m_aspectRatio);
        m_heightSpin->setValue(m_newHeight);
    } else {
        m_newHeight = m_heightSpin->value();
    }
    
    m_previewLabel->setText(QString("Output: %1 × %2 pixels").arg(m_newWidth).arg(m_newHeight));
    m_updating = false;
}

void ResizeDialog::onHeightChanged(int value) {
    if (m_updating) return;
    m_updating = true;
    
    m_newHeight = value;
    if (m_lockAspect) {
        m_newWidth = static_cast<int>(value * m_aspectRatio);
        m_widthSpin->setValue(m_newWidth);
    } else {
        m_newWidth = m_widthSpin->value();
    }
    
    m_previewLabel->setText(QString("Output: %1 × %2 pixels").arg(m_newWidth).arg(m_newHeight));
    m_updating = false;
}

void ResizeDialog::onPresetClicked(int percent) {
    m_updating = true;
    m_newWidth = m_originalWidth * percent / 100;
    m_newHeight = m_originalHeight * percent / 100;
    m_widthSpin->setValue(m_newWidth);
    m_heightSpin->setValue(m_newHeight);
    m_previewLabel->setText(QString("Output: %1 × %2 pixels").arg(m_newWidth).arg(m_newHeight));
    m_updating = false;
}
