#ifndef GLASSWIDGETS_H
#define GLASSWIDGETS_H

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QAbstractButton>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QLabel>
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QTimer>
#include <QString>
#include <QRect>
#include <QVector>
#include <vector>

class CanvasWidget;

struct GlassTheme {
    QString id;
    QString name;
    QColor swatchColor;      // Theme picker dot color
    QColor stageBgTop;       // Dark background top
    QColor stageBgMid;       // Dark background center
    QColor stageBgBottom;    // Dark background bottom
    QColor auroraPrimary;    // Soft atmospheric bloom
    QColor auroraSecondary;  // Secondary subtle bloom
    QColor glassBaseTop;     // Grayish translucent liquid glass top
    QColor glassBaseBottom;  // Grayish translucent liquid glass bottom
    QColor glassRimTop;      // Bright specular top-left refraction edge
    QColor glassRimBottom;   // Caustic bottom-right reflection edge
    QColor glassGlow;        // Subtle inner theme tint inside liquid glass
    QColor accentStart;      // Primary accent gradient start
    QColor accentEnd;        // Primary accent gradient end
    QColor toggleActive;     // Toggle switch ON color
    QColor badgeBg;          // Pill badge background
    QColor badgeBorder;      // Pill badge border
    QColor badgeText;        // Pill badge text
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    const QVector<GlassTheme>& themes() const { return m_themes; }
    const GlassTheme& current() const { return m_themes[m_currentIndex]; }
    int currentIndex() const { return m_currentIndex; }
    void setThemeIndex(int index);

signals:
    void themeChanged(const GlassTheme& theme);

private:
    ThemeManager();
    QVector<GlassTheme> m_themes;
    int m_currentIndex = 0;
};

namespace GlassIcons {
    void paintIcon(QPainter& p, const QString& name, const QRectF& rect, const QColor& color, qreal strokeWidth = 1.75);
    QIcon createIcon(const QString& name, const QColor& color = QColor(240, 245, 255), int size = 24);
}

// Translucent Grayish Liquid Glass container with cached HiDPI optical surface for 120 FPS performance
class GlassPanel : public QWidget {
    Q_OBJECT
public:
    explicit GlassPanel(QWidget* parent = nullptr);

    void setCanvasBackdrop(CanvasWidget* canvas) { m_canvas = canvas; }
    void setCornerRadius(qreal radius) { m_radius = radius; invalidateCache(); }
    void setAccentGlow(bool enabled) { m_accentGlow = enabled; invalidateCache(); }
    void setElevated(bool elevated) { m_elevated = elevated; invalidateCache(); }
    void setSubSurface(bool sub) { m_subSurface = sub; invalidateCache(); }
    void invalidateCache();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void rebuildShellCache();

    CanvasWidget* m_canvas = nullptr;
    qreal m_radius = 22.0;
    bool m_accentGlow = true;
    bool m_elevated = true;
    bool m_subSurface = false;
    QPixmap m_cachedShell;
    qreal m_cachedDpr = 0.0;
    bool m_cacheDirty = true;
};

// Authentic Apple Liquid Glass Tab Navigation Bar with smooth spring-gliding refractive lens bubble
class LiquidGlassTabBar : public QWidget {
    Q_OBJECT
public:
    struct TabItem {
        int id;
        QString label;
        QString iconName;
        QString shortcut;
        QString tooltip;
    };

    explicit LiquidGlassTabBar(QWidget* parent = nullptr);

    void addTab(int id, const QString& label, const QString& iconName = QString(),
                const QString& shortcut = QString(), const QString& tooltip = QString());
    void setCurrentId(int id, bool animate = true);
    int currentId() const;
    void setPillHeight(int h);

    QSize sizeHint() const override;

signals:
    void tabSelected(int id);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QRectF tabRectAt(int index) const;
    int hitTestTab(const QPointF& pos) const;
    void syncGeometryInstant();

    std::vector<TabItem> m_tabs;
    int m_currentIndex = 0;
    int m_hoveredIndex = -1;
    int m_pillHeight = 40;

    QRectF m_indicatorRect;
    QVariantAnimation* m_slideAnim = nullptr;
    qreal m_stretchAmount = 0.0; // Subtle liquid stretch mid-transition
    QPixmap m_cachePixmap;
    quint64 m_cacheKey = 0;
};

// High-craft animated Liquid Glass capsule & circular medallion button
class GlassButton : public QPushButton {
    Q_OBJECT
public:
    enum Variant {
        Ghost,       // Grayish translucent liquid glass pill
        Primary,     // Theme-colored high-gloss liquid pill
        White,       // Pure crisp white high-contrast CTA pill
        IconCircle,  // Circular liquid-glass medallion with bright specular rim
        Danger       // Subtle dark crimson liquid glass pill (Discard / Close)
    };

    explicit GlassButton(const QString& text = QString(), Variant variant = Ghost, QWidget* parent = nullptr);

    void setIconName(const QString& iconName) { m_iconName = iconName; update(); }
    void setShortcutBadge(const QString& badge) { m_shortcut = badge; update(); }
    void setVariant(Variant v) { m_variant = v; update(); }
    void setCornerRadius(qreal r) { m_radius = r; update(); }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    Variant m_variant;
    QString m_iconName;
    QString m_shortcut;
    qreal m_radius = -1.0;
    qreal m_hoverFactor = 0.0;
    QVariantAnimation* m_hoverAnim = nullptr;
    QPixmap m_cachePixmap;
    quint64 m_cacheKey = 0;
};

// Smoothly animated iOS / visionOS Liquid Glass Toggle Switch
class GlassToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit GlassToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(46, 26); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void checkStateSet() override;
    void nextCheckState() override;

private:
    void animateToState(bool checked);
    qreal m_knobPos = 0.0;
    QVariantAnimation* m_anim = nullptr;
};

// Custom-painted Liquid Glass slider control with Up/Down precision stepper buttons & smooth hover
class GlassSliderControl : public QWidget {
    Q_OBJECT
public:
    explicit GlassSliderControl(const QString& title, int minVal, int maxVal, int initVal,
                                const QString& suffix = QString(), QWidget* parent = nullptr);

    int value() const { return m_value; }
    void setValue(int val, bool emitSignal = false);
    void stepBy(int delta);
    void setZeroLabel(const QString& zeroLbl) { m_zeroLabel = zeroLbl; update(); }
    void setUseThemeToggleColor(bool useToggle) { m_useToggleAccent = useToggle; update(); }

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum HoverPart { PartNone, PartTrack, PartStepUp, PartStepDown };

    void updateValueFromX(qreal x);
    QRectF trackRect() const;
    QRectF valueBoxRect() const;
    QRectF stepUpRect() const;
    QRectF stepDownRect() const;

    QString m_title;
    QString m_suffix;
    QString m_zeroLabel;
    int m_min;
    int m_max;
    int m_value;
    bool m_dragging = false;
    bool m_useToggleAccent = false;
    HoverPart m_hoverPart = PartNone;
    HoverPart m_pressedPart = PartNone;
    qreal m_hoverFactor = 0.0;
    QVariantAnimation* m_hoverAnim = nullptr;
    QTimer* m_repeatDelayTimer = nullptr;
    QTimer* m_repeatTickTimer = nullptr;
    int m_repeatDir = 0;
    QPixmap m_cachePixmap;
    quint64 m_cacheKey = 0;
};

// Nayo-Bank / Liquid Glass nested action row with circular icon medallion & animated hover
class GlassActionRow : public QPushButton {
    Q_OBJECT
public:
    explicit GlassActionRow(const QString& iconName, const QString& title,
                            const QString& subtitle, bool useThemeAccent = true,
                            const QColor& customAccent = QColor(240, 245, 255),
                            QWidget* parent = nullptr);

    QSize sizeHint() const override { return QSize(230, 46); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString m_iconName;
    QString m_title;
    QString m_subtitle;
    bool m_useThemeAccent;
    QColor m_customAccent;
    qreal m_hoverFactor = 0.0;
    QVariantAnimation* m_hoverAnim = nullptr;
    QPixmap m_cachePixmap;
    quint64 m_cacheKey = 0;
};

// Tactile glass backdrop surface swatch
class GlassSwatchButton : public QPushButton {
    Q_OBJECT
public:
    explicit GlassSwatchButton(int bgType, const QString& label, const QString& tip, QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(54, 36); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_bgType;
    QString m_label;
    QPixmap m_cachePixmap;
    quint64 m_cacheKey = 0;
};

// Sleek row of glowing theme selector dots with smooth active ring
class GlassThemePicker : public QWidget {
    Q_OBJECT
public:
    explicit GlassThemePicker(QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(230, 34); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF dotRect(int idx) const;
    int m_hovered = -1;
    QPixmap m_cachePixmap;
    quint64 m_cacheKey = 0;
};

// Frameless Liquid Glass Modal Window Base (eliminates ugly default Windows titlebar on ALL dialogs!)
class GlassModalDialog : public QDialog {
    Q_OBJECT
public:
    explicit GlassModalDialog(const QString& title, QWidget* parent = nullptr);

    void setModalTitle(const QString& title);
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QLabel* m_headerTitle = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    bool m_draggingWindow = false;
    QPoint m_dragOffset;
};

// Centralized Liquid Glass Alert, Warning, Error & Confirmation System (replaces ALL default QMessageBox!)
class GlassDialog : public GlassModalDialog {
    Q_OBJECT
public:
    enum Kind { Info, Warning, Error, Success, Question };

    static void info(QWidget* parent, const QString& title, const QString& text, const QString& detail = QString());
    static void warning(QWidget* parent, const QString& title, const QString& text, const QString& detail = QString());
    static void error(QWidget* parent, const QString& title, const QString& text, const QString& detail = QString());
    static void success(QWidget* parent, const QString& title, const QString& text, const QString& detail = QString());
    static bool confirm(QWidget* parent, const QString& title, const QString& text,
                        const QString& detail = QString(),
                        const QString& confirmText = "Confirm",
                        const QString& cancelText = "Cancel",
                        bool destructive = false);
    // Returns: 1 = primaryYes, 2 = secondaryNo, 0 = cancel
    static int confirmThreeWay(QWidget* parent, const QString& title, const QString& text,
                               const QString& detail,
                               const QString& yesText,
                               const QString& noText,
                               const QString& cancelText = "Cancel");
    static void showShortcuts(QWidget* parent, const QString& title, const QString& bodyText);

private:
    explicit GlassDialog(Kind kind, const QString& title, const QString& text,
                         const QString& detail, QWidget* parent = nullptr);
    int m_choice = 0;
};

#endif // GLASSWIDGETS_H
