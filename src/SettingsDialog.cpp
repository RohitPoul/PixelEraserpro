#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>
#include <QFileInfo>

namespace {
constexpr const char* kSettingsOrg = "PixelEraserPro";
constexpr const char* kSettingsApp = "PixelEraserPro";

QString sanitizeFilenamePart(QString text) {
    // Strip characters disallowed in Windows/macOS/Linux filenames
    static const QString forbidden = "\\/:*?\"<>|";
    for (QChar ch : forbidden) {
        text.remove(ch);
    }
    return text;
}
} // namespace

ExportSettings SettingsDialog::loadSettings() {
    QSettings qs(kSettingsOrg, kSettingsApp);
    ExportSettings s;
    const int nm = qs.value("export/namingMode", static_cast<int>(ExportSettings::Suffix)).toInt();
    if (nm >= ExportSettings::KeepOriginal && nm <= ExportSettings::Both) {
        s.namingMode = static_cast<ExportSettings::NamingMode>(nm);
    }
    s.prefix = qs.value("export/prefix", "cutout_").toString();
    s.suffix = qs.value("export/suffix", "_cutout").toString();

    const int fm = qs.value("export/formatMode", static_cast<int>(ExportSettings::ForcePng)).toInt();
    if (fm >= ExportSettings::KeepOriginalType && fm <= ExportSettings::ForceJpg) {
        s.formatMode = static_cast<ExportSettings::FormatMode>(fm);
    }
    return s;
}

void SettingsDialog::saveSettings(const ExportSettings& settings) {
    QSettings qs(kSettingsOrg, kSettingsApp);
    qs.setValue("export/namingMode", static_cast<int>(settings.namingMode));
    qs.setValue("export/prefix", sanitizeFilenamePart(settings.prefix));
    qs.setValue("export/suffix", sanitizeFilenamePart(settings.suffix));
    qs.setValue("export/formatMode", static_cast<int>(settings.formatMode));
    qs.sync();
}

QString SettingsDialog::resolveExtension(const QString& currentFilePath, ExportSettings::FormatMode formatMode) {
    switch (formatMode) {
        case ExportSettings::ForcePng:  return "png";
        case ExportSettings::ForceWebp: return "webp";
        case ExportSettings::ForceJpg:  return "jpg";
        case ExportSettings::KeepOriginalType: {
            if (!currentFilePath.isEmpty()) {
                QString origExt = QFileInfo(currentFilePath).suffix().toLower();
                if (origExt == "png" || origExt == "webp" || origExt == "jpg" ||
                    origExt == "jpeg" || origExt == "bmp" || origExt == "tiff" || origExt == "tif") {
                    return origExt;
                }
            }
            return "png";
        }
    }
    return "png";
}

QString SettingsDialog::buildExportPath(const QString& currentFilePath, const ExportSettings& settings) {
    const QString ext = resolveExtension(currentFilePath, settings.formatMode);
    const QString prefix = sanitizeFilenamePart(settings.prefix);
    const QString suffix = sanitizeFilenamePart(settings.suffix);

    QString dir;
    QString baseName = "exported";
    if (!currentFilePath.isEmpty()) {
        QFileInfo fi(currentFilePath);
        dir = fi.absolutePath();
        baseName = fi.completeBaseName();
    }

    QString finalBase = baseName;
    switch (settings.namingMode) {
        case ExportSettings::KeepOriginal:
            finalBase = baseName;
            break;
        case ExportSettings::Prefix:
            finalBase = prefix + baseName;
            break;
        case ExportSettings::Suffix:
            finalBase = baseName + suffix;
            break;
        case ExportSettings::Both:
            finalBase = prefix + baseName + suffix;
            break;
    }

    if (finalBase.isEmpty()) {
        finalBase = "exported";
    }

    const QString fileName = finalBase + "." + ext;
    return dir.isEmpty() ? fileName : (dir + "/" + fileName);
}

QString SettingsDialog::buildExportPath(const QString& currentFilePath) {
    return buildExportPath(currentFilePath, loadSettings());
}

QString SettingsDialog::buildFileDialogFilter(const QString& ext) {
    const QString e = ext.toLower();
    if (e == "webp") {
        return "WebP (*.webp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;TIFF (*.tiff *.tif);;BMP (*.bmp)";
    }
    if (e == "jpg" || e == "jpeg") {
        return "JPEG (*.jpg *.jpeg);;PNG (*.png);;WebP (*.webp);;TIFF (*.tiff *.tif);;BMP (*.bmp)";
    }
    if (e == "tiff" || e == "tif") {
        return "TIFF (*.tiff *.tif);;PNG (*.png);;WebP (*.webp);;JPEG (*.jpg *.jpeg);;BMP (*.bmp)";
    }
    if (e == "bmp") {
        return "BMP (*.bmp);;PNG (*.png);;WebP (*.webp);;JPEG (*.jpg *.jpeg);;TIFF (*.tiff *.tif)";
    }
    return "PNG (*.png);;WebP (*.webp);;JPEG (*.jpg *.jpeg);;TIFF (*.tiff *.tif);;BMP (*.bmp)";
}

SettingsDialog::SettingsDialog(const QString& currentFilePath, QWidget* parent)
    : GlassModalDialog("Settings", parent)
    , m_sampleFilePath(currentFilePath)
{
    setMinimumSize(540, 490);
    setupUI();
    applyStateToControls(loadSettings());
}

void SettingsDialog::setupUI() {
    const GlassTheme& th = ThemeManager::instance().current();
    QVBoxLayout* root = contentLayout();
    root->setSpacing(14);

    auto makeFieldStyle = []() {
        return QString(
            "QLineEdit { "
            "  background-color: rgba(16, 18, 22, 210); color: #f2f4f8; "
            "  border: 1px solid rgba(255, 255, 255, 42); border-radius: 10px; "
            "  padding: 6px 12px; font-size: 12px; font-weight: 600; "
            "}"
            "QLineEdit:focus { border: 1px solid rgba(255, 255, 255, 130); }"
            "QLineEdit:disabled { "
            "  background-color: rgba(16, 18, 22, 95); color: rgba(160, 166, 178, 110); "
            "  border: 1px solid rgba(255, 255, 255, 18); "
            "}"
        );
    };

    // 1) EXPORT FILENAME NAMING CARD
    GlassPanel* namingCard = new GlassPanel();
    namingCard->setCornerRadius(18.0);
    namingCard->setAccentGlow(true);
    QVBoxLayout* nLay = new QVBoxLayout(namingCard);
    nLay->setContentsMargins(18, 15, 18, 16);
    nLay->setSpacing(10);

    QLabel* nTitle = new QLabel("Export Filename");
    nTitle->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: 700; background: transparent;");
    QLabel* nSub = new QLabel("Choose whether to keep the original filename or add a custom prefix / suffix.");
    nSub->setStyleSheet("color: #9faac0; font-size: 11px; background: transparent;");
    nSub->setWordWrap(true);
    nLay->addWidget(nTitle);
    nLay->addWidget(nSub);

    GlassPanel* nTabWrap = new GlassPanel();
    nTabWrap->setCornerRadius(17.0);
    nTabWrap->setSubSurface(true);
    nTabWrap->setElevated(false);
    nTabWrap->setFixedHeight(38);
    QHBoxLayout* nTabLay = new QHBoxLayout(nTabWrap);
    nTabLay->setContentsMargins(3, 2, 3, 2);

    m_namingTabs = new LiquidGlassTabBar();
    m_namingTabs->setPillHeight(34);
    m_namingTabs->addTab(ExportSettings::KeepOriginal, "Original", "", "", "Keep exact original filename");
    m_namingTabs->addTab(ExportSettings::Prefix, "Prefix", "", "", "Add custom text before filename");
    m_namingTabs->addTab(ExportSettings::Suffix, "Suffix", "", "", "Add custom text after filename");
    m_namingTabs->addTab(ExportSettings::Both, "Both", "", "", "Add both prefix and suffix");
    nTabLay->addWidget(m_namingTabs);
    nLay->addWidget(nTabWrap);

    QHBoxLayout* fieldsRow = new QHBoxLayout();
    fieldsRow->setSpacing(10);

    QVBoxLayout* preCol = new QVBoxLayout();
    preCol->setSpacing(4);
    QLabel* preLbl = new QLabel("PREFIX");
    preLbl->setStyleSheet("color: rgba(210, 214, 222, 165); font-size: 10px; font-weight: 700; letter-spacing: 0.7px; background: transparent;");
    m_prefixEdit = new QLineEdit();
    m_prefixEdit->setPlaceholderText("e.g. cutout_");
    m_prefixEdit->setMinimumHeight(34);
    m_prefixEdit->setStyleSheet(makeFieldStyle());
    preCol->addWidget(preLbl);
    preCol->addWidget(m_prefixEdit);

    QVBoxLayout* sufCol = new QVBoxLayout();
    sufCol->setSpacing(4);
    QLabel* sufLbl = new QLabel("SUFFIX");
    sufLbl->setStyleSheet("color: rgba(210, 214, 222, 165); font-size: 10px; font-weight: 700; letter-spacing: 0.7px; background: transparent;");
    m_suffixEdit = new QLineEdit();
    m_suffixEdit->setPlaceholderText("e.g. _cutout");
    m_suffixEdit->setMinimumHeight(34);
    m_suffixEdit->setStyleSheet(makeFieldStyle());
    sufCol->addWidget(sufLbl);
    sufCol->addWidget(m_suffixEdit);

    fieldsRow->addLayout(preCol, 1);
    fieldsRow->addLayout(sufCol, 1);
    nLay->addLayout(fieldsRow);
    root->addWidget(namingCard);

    // 2) EXPORT FILE TYPE CARD
    GlassPanel* fmtCard = new GlassPanel();
    fmtCard->setCornerRadius(18.0);
    QVBoxLayout* fLay = new QVBoxLayout(fmtCard);
    fLay->setContentsMargins(18, 15, 18, 16);
    fLay->setSpacing(10);

    QLabel* fTitle = new QLabel("Output File Type");
    fTitle->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: 700; background: transparent;");
    fLay->addWidget(fTitle);

    GlassPanel* fTabWrap = new GlassPanel();
    fTabWrap->setCornerRadius(17.0);
    fTabWrap->setSubSurface(true);
    fTabWrap->setElevated(false);
    fTabWrap->setFixedHeight(38);
    QHBoxLayout* fTabLay = new QHBoxLayout(fTabWrap);
    fTabLay->setContentsMargins(3, 2, 3, 2);

    m_formatTabs = new LiquidGlassTabBar();
    m_formatTabs->setPillHeight(34);
    m_formatTabs->addTab(ExportSettings::KeepOriginalType, "Keep Original", "", "", "Export using the input image's original file extension");
    m_formatTabs->addTab(ExportSettings::ForcePng, "PNG", "", "", "Always export lossless transparent PNG");
    m_formatTabs->addTab(ExportSettings::ForceWebp, "WebP", "", "", "Always export lossless transparent WebP");
    m_formatTabs->addTab(ExportSettings::ForceJpg, "JPG", "", "", "Always export high-quality JPEG (flattened)");
    fTabLay->addWidget(m_formatTabs);
    fLay->addWidget(fTabWrap);

    m_formatHintLabel = new QLabel();
    m_formatHintLabel->setStyleSheet("color: #9faac0; font-size: 11px; background: transparent;");
    m_formatHintLabel->setWordWrap(true);
    fLay->addWidget(m_formatHintLabel);
    root->addWidget(fmtCard);

    // 3) LIVE PREVIEW CARD
    GlassPanel* prevCard = new GlassPanel();
    prevCard->setCornerRadius(16.0);
    QHBoxLayout* pLay = new QHBoxLayout(prevCard);
    pLay->setContentsMargins(16, 12, 16, 12);
    pLay->setSpacing(10);

    QLabel* prevTag = new QLabel("PREVIEW");
    prevTag->setStyleSheet(QString(
        "color: %1; font-size: 10px; font-weight: 700; letter-spacing: 0.7px; "
        "background-color: rgba(%2,%3,%4,%5); border: 1px solid rgba(%6,%7,%8,%9); "
        "border-radius: 8px; padding: 3px 8px;"
    ).arg(th.badgeText.name())
     .arg(th.badgeBg.red()).arg(th.badgeBg.green()).arg(th.badgeBg.blue()).arg(th.badgeBg.alpha())
     .arg(th.badgeBorder.red()).arg(th.badgeBorder.green()).arg(th.badgeBorder.blue()).arg(th.badgeBorder.alpha()));

    m_previewOutputLabel = new QLabel();
    m_previewOutputLabel->setStyleSheet("color: #f0f3f8; font-size: 12px; font-weight: 600; background: transparent;");
    pLay->addWidget(prevTag);
    pLay->addWidget(m_previewOutputLabel, 1);
    root->addWidget(prevCard);

    root->addStretch(1);

    // 4) BOTTOM ACTION BUTTONS
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    GlassButton* resetBtn = new GlassButton("Reset Defaults", GlassButton::Ghost);
    resetBtn->setMinimumHeight(36);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        applyStateToControls(ExportSettings{});
    });

    GlassButton* cancelBtn = new GlassButton("Cancel", GlassButton::Ghost);
    cancelBtn->setMinimumSize(96, 36);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    GlassButton* saveBtn = new GlassButton("Save Settings", GlassButton::White);
    saveBtn->setMinimumSize(136, 36);
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        saveSettings(collectSettings());
        accept();
    });

    btnRow->addWidget(resetBtn);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    connect(m_namingTabs, &LiquidGlassTabBar::tabSelected, this, [this](int) {
        updateInputStates();
        updatePreview();
    });
    connect(m_formatTabs, &LiquidGlassTabBar::tabSelected, this, [this](int) {
        updatePreview();
    });
    connect(m_prefixEdit, &QLineEdit::textChanged, this, &SettingsDialog::updatePreview);
    connect(m_suffixEdit, &QLineEdit::textChanged, this, &SettingsDialog::updatePreview);
}

void SettingsDialog::applyStateToControls(const ExportSettings& s) {
    m_namingTabs->setCurrentId(static_cast<int>(s.namingMode), false);
    m_prefixEdit->setText(s.prefix);
    m_suffixEdit->setText(s.suffix);
    m_formatTabs->setCurrentId(static_cast<int>(s.formatMode), false);
    updateInputStates();
    updatePreview();
}

ExportSettings SettingsDialog::collectSettings() const {
    ExportSettings s;
    s.namingMode = static_cast<ExportSettings::NamingMode>(m_namingTabs->currentId());
    s.prefix = m_prefixEdit->text();
    s.suffix = m_suffixEdit->text();
    s.formatMode = static_cast<ExportSettings::FormatMode>(m_formatTabs->currentId());
    return s;
}

void SettingsDialog::updateInputStates() {
    const auto mode = static_cast<ExportSettings::NamingMode>(m_namingTabs->currentId());
    m_prefixEdit->setEnabled(mode == ExportSettings::Prefix || mode == ExportSettings::Both);
    m_suffixEdit->setEnabled(mode == ExportSettings::Suffix || mode == ExportSettings::Both);
}

void SettingsDialog::updatePreview() {
    const ExportSettings s = collectSettings();
    const QString sampleIn = m_sampleFilePath.isEmpty() ? QString("character.jpg") : m_sampleFilePath;
    const QString inName = QFileInfo(sampleIn).fileName();
    const QString outName = QFileInfo(buildExportPath(sampleIn, s)).fileName();

    m_previewOutputLabel->setText(QString("%1   →   %2").arg(inName, outName));

    switch (s.formatMode) {
        case ExportSettings::KeepOriginalType:
            m_formatHintLabel->setText("Matches the input file's extension (.png, .webp, .jpg, .tiff, .bmp). PNG keeps Self-Remembering RGBA.");
            break;
        case ExportSettings::ForcePng:
            m_formatHintLabel->setText("Lossless transparent RGBA PNG with Self-Remembering original pixels (recommended for cutouts).");
            break;
        case ExportSettings::ForceWebp:
            m_formatHintLabel->setText("Lossless transparent RGBA WebP for smaller file sizes while preserving clean alpha edges.");
            break;
        case ExportSettings::ForceJpg:
            m_formatHintLabel->setText("High-quality JPEG (95%). Transparent areas are cleanly flattened onto a white background.");
            break;
    }
}
