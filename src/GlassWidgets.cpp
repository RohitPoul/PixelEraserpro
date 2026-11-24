#include "GlassWidgets.h"
#include <QGuiApplication>
#include <QScreen>
#include <QShowEvent>
#include "CanvasWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QGuiApplication>
#include <QToolTip>
#include <QSettings>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QEasingCurve>
#include <algorithm>
#include <cmath>

namespace {
QColor lerpColor(const QColor& a, const QColor& b, qreal t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(
        static_cast<int>(a.red()   + (b.red()   - a.red())   * t),
        static_cast<int>(a.green() + (b.green() - a.green()) * t),
        static_cast<int>(a.blue()  + (b.blue()  - a.blue())  * t),
        static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t)
    );
}

inline quint64 mixHash(quint64 h, quint64 v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

QPixmap makeHiDpiPixmap(int w, int h, qreal dpr) {
    QPixmap pm(std::max(1, static_cast<int>(std::ceil(w * dpr))),
               std::max(1, static_cast<int>(std::ceil(h * dpr))));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    return pm;
}
} // namespace

// ============================================================================
// ThemeManager — 6 Curated Dark-Backdrop + Grayish Liquid Glass Color Combos
// ============================================================================
ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager() {
    m_themes = {
        // 0. Obsidian Smoke (Default: Pure Neutral Dark Grayish Liquid Glass + Crisp Titanium Silver)
        GlassTheme{
            "obsidian_smoke", "Obsidian Smoke", QColor(122, 125, 132),
            QColor(18, 18, 20), QColor(24, 24, 27), QColor(14, 14, 16),
            QColor(160, 162, 168, 22), QColor(110, 112, 118, 14),
            QColor(72, 74, 80, 174), QColor(38, 40, 44, 198),
            QColor(255, 255, 255, 152), QColor(200, 204, 212, 86),
            QColor(210, 214, 222, 18),
            QColor(76, 80, 88), QColor(116, 122, 134),
            QColor(118, 126, 138),
            QColor(255, 255, 255, 28), QColor(255, 255, 255, 76), QColor(238, 240, 244)
        },
        // 1. Matte Onyx (Stealth Pitch-Carbon Dark Glass + Frosted Pearl Rim)
        GlassTheme{
            "matte_onyx", "Matte Onyx", QColor(68, 70, 74),
            QColor(10, 10, 12), QColor(16, 16, 18), QColor(8, 8, 10),
            QColor(140, 140, 145, 16), QColor(90, 90, 96, 10),
            QColor(46, 48, 52, 192), QColor(24, 25, 28, 214),
            QColor(255, 255, 255, 145), QColor(175, 178, 185, 74),
            QColor(180, 184, 192, 14),
            QColor(52, 54, 60), QColor(88, 92, 100),
            QColor(108, 112, 120),
            QColor(255, 255, 255, 22), QColor(255, 255, 255, 64), QColor(228, 230, 235)
        },
        // 2. Carbon Ash (Warm Smoked Ash-Gray Glass + Brushed Pewter)
        GlassTheme{
            "carbon_ash", "Carbon Ash", QColor(112, 106, 98),
            QColor(20, 19, 18), QColor(26, 25, 23), QColor(15, 14, 13),
            QColor(165, 155, 142, 20), QColor(115, 108, 98, 12),
            QColor(78, 74, 70, 174), QColor(42, 40, 38, 198),
            QColor(255, 252, 246, 148), QColor(212, 202, 188, 82),
            QColor(208, 198, 184, 18),
            QColor(86, 80, 74), QColor(128, 120, 110),
            QColor(138, 128, 114),
            QColor(240, 230, 216, 26), QColor(240, 230, 216, 72), QColor(242, 236, 228)
        },
        // 3. Graphite Steel (Dark Smoked Gunmetal Glass + Muted Steel)
        GlassTheme{
            "graphite_steel", "Graphite Steel", QColor(90, 98, 108),
            QColor(16, 18, 20), QColor(22, 24, 28), QColor(12, 14, 16),
            QColor(130, 142, 158, 20), QColor(85, 96, 112, 12),
            QColor(64, 68, 76, 176), QColor(34, 37, 42, 200),
            QColor(248, 252, 255, 148), QColor(178, 192, 208, 82),
            QColor(160, 176, 196, 18),
            QColor(66, 76, 90), QColor(102, 116, 134),
            QColor(96, 114, 136),
            QColor(190, 208, 228, 28), QColor(190, 208, 228, 72), QColor(224, 234, 246)
        },
        // 4. Dark Tungsten (Smoked Espresso-Charcoal Glass + Muted Dark Bronze)
        GlassTheme{
            "dark_tungsten", "Dark Tungsten", QColor(118, 96, 68),
            QColor(18, 16, 14), QColor(24, 21, 18), QColor(13, 12, 10),
            QColor(168, 132, 86, 22), QColor(118, 92, 58, 12),
            QColor(74, 68, 62, 176), QColor(40, 36, 33, 200),
            QColor(255, 248, 238, 146), QColor(214, 182, 138, 82),
            QColor(196, 156, 102, 18),
            QColor(98, 76, 48), QColor(142, 112, 72),
            QColor(148, 116, 72),
            QColor(224, 184, 130, 28), QColor(224, 184, 130, 72), QColor(244, 226, 198)
        },
        // 5. Shadow Garnet (Dark Volcanic-Ash Glass + Muted Smoked Crimson)
        GlassTheme{
            "shadow_garnet", "Shadow Garnet", QColor(116, 66, 76),
            QColor(18, 14, 15), QColor(24, 18, 20), QColor(13, 10, 11),
            QColor(168, 78, 94, 22), QColor(118, 54, 66, 12),
            QColor(72, 62, 65, 176), QColor(38, 33, 35, 200),
            QColor(255, 246, 248, 146), QColor(210, 150, 162, 82),
            QColor(192, 92, 110, 18),
            QColor(102, 52, 62), QColor(144, 76, 88),
            QColor(152, 74, 88),
            QColor(226, 136, 152, 28), QColor(226, 136, 152, 72), QColor(244, 212, 218)
        }
    };

    QSettings settings("PixelEraser", "PixelEraser Pro");
    int savedIdx = settings.value("ui/themeIndex", 0).toInt();
    m_currentIndex = std::clamp(savedIdx, 0, static_cast<int>(m_themes.size()) - 1);
}

void ThemeManager::setThemeIndex(int index) {
    index = std::clamp(index, 0, static_cast<int>(m_themes.size()) - 1);
    if (index != m_currentIndex) {
        m_currentIndex = index;
        QSettings settings("PixelEraser", "PixelEraser Pro");
        settings.setValue("ui/themeIndex", m_currentIndex);
        emit themeChanged(m_themes[m_currentIndex]);
    }
}

// ============================================================================
// GlassIcons — Crisp SF-Symbols Style Thin Vector Iconography
// ============================================================================
namespace GlassIcons {

void paintIcon(QPainter& p, const QString& name, const QRectF& rect, const QColor& color, qreal strokeWidth) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal s = std::min(rect.width(), rect.height());
    const qreal u = s / 24.0;

    auto pt = [&](qreal x, qreal y) -> QPointF {
        return QPointF(cx + (x - 12.0) * u, cy + (y - 12.0) * u);
    };

    QPen pen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (name == "menu") {
        p.drawLine(pt(4.5, 7.5), pt(19.5, 7.5));
        p.drawLine(pt(4.5, 12.0), pt(19.5, 12.0));
        p.drawLine(pt(4.5, 16.5), pt(15.5, 16.5));
    } else if (name == "open") {
        QPainterPath path;
        path.moveTo(pt(3.5, 7.5));
        path.lineTo(pt(9.0, 7.5));
        path.lineTo(pt(11.0, 9.5));
        path.lineTo(pt(20.5, 9.5));
        path.lineTo(pt(20.5, 18.5));
        path.lineTo(pt(3.5, 18.5));
        path.closeSubpath();
        p.drawPath(path);
    } else if (name == "export") {
        p.drawLine(pt(12.0, 4.5), pt(12.0, 15.0));
        p.drawLine(pt(8.0, 8.5), pt(12.0, 4.5));
        p.drawLine(pt(16.0, 8.5), pt(12.0, 4.5));
        QPainterPath tray;
        tray.moveTo(pt(5.0, 14.5));
        tray.lineTo(pt(5.0, 19.0));
        tray.lineTo(pt(19.0, 19.0));
        tray.lineTo(pt(19.0, 14.5));
        p.drawPath(tray);
    } else if (name == "discard") {
        p.drawLine(pt(5.5, 7.0), pt(18.5, 7.0));
        p.drawLine(pt(9.5, 4.5), pt(14.5, 4.5));
        p.drawRoundedRect(QRectF(pt(7.0, 7.0), pt(17.0, 19.5)), 2.0 * u, 2.0 * u);
        p.drawLine(pt(10.2, 10.5), pt(10.2, 16.0));
        p.drawLine(pt(13.8, 10.5), pt(13.8, 16.0));
    } else if (name == "undo") {
        p.drawLine(pt(9.5, 7.0), pt(5.5, 11.0));
        p.drawLine(pt(9.5, 15.0), pt(5.5, 11.0));
        QPainterPath arc;
        arc.moveTo(pt(6.0, 11.0));
        arc.lineTo(pt(14.5, 11.0));
        arc.cubicTo(pt(18.5, 11.0), pt(19.5, 14.0), pt(18.5, 17.5));
        p.drawPath(arc);
    } else if (name == "redo") {
        p.drawLine(pt(14.5, 7.0), pt(18.5, 11.0));
        p.drawLine(pt(14.5, 15.0), pt(18.5, 11.0));
        QPainterPath arc;
        arc.moveTo(pt(18.0, 11.0));
        arc.lineTo(pt(9.5, 11.0));
        arc.cubicTo(pt(5.5, 11.0), pt(4.5, 14.0), pt(5.5, 17.5));
        p.drawPath(arc);
    } else if (name == "zoom_in") {
        p.drawLine(pt(12.0, 6.0), pt(12.0, 18.0));
        p.drawLine(pt(6.0, 12.0), pt(18.0, 12.0));
    } else if (name == "zoom_out") {
        p.drawLine(pt(6.0, 12.0), pt(18.0, 12.0));
    } else if (name == "fit") {
        p.drawLine(pt(5.0, 9.0), pt(5.0, 5.0)); p.drawLine(pt(5.0, 5.0), pt(9.0, 5.0));
        p.drawLine(pt(19.0, 9.0), pt(19.0, 5.0)); p.drawLine(pt(19.0, 5.0), pt(15.0, 5.0));
        p.drawLine(pt(5.0, 15.0), pt(5.0, 19.0)); p.drawLine(pt(5.0, 19.0), pt(9.0, 19.0));
        p.drawLine(pt(19.0, 15.0), pt(19.0, 19.0)); p.drawLine(pt(19.0, 19.0), pt(15.0, 19.0));
    } else if (name == "sparkle") {
        QPainterPath star;
        star.moveTo(pt(11.0, 3.5));
        star.quadTo(pt(11.0, 11.5), pt(18.5, 11.5));
        star.quadTo(pt(11.0, 11.5), pt(11.0, 19.5));
        star.quadTo(pt(11.0, 11.5), pt(3.5, 11.5));
        star.quadTo(pt(11.0, 11.5), pt(11.0, 3.5));
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawPath(star);

        QPainterPath mini;
        mini.moveTo(pt(18.5, 4.0));
        mini.quadTo(pt(18.5, 6.8), pt(21.2, 6.8));
        mini.quadTo(pt(18.5, 6.8), pt(18.5, 9.5));
        mini.quadTo(pt(18.5, 6.8), pt(15.8, 6.8));
        mini.quadTo(pt(18.5, 6.8), pt(18.5, 4.0));
        p.drawPath(mini);
    } else if (name == "wand") {
        p.drawLine(pt(5.0, 19.0), pt(15.5, 8.5));
        p.drawLine(pt(15.5, 4.0), pt(15.5, 6.5));
        p.drawLine(pt(19.5, 8.5), pt(17.5, 8.5));
        p.drawLine(pt(18.8, 5.2), pt(17.2, 6.8));
        p.drawLine(pt(12.2, 5.2), pt(13.8, 6.8));
    } else if (name == "eraser") {
        QPainterPath body;
        body.moveTo(pt(8.5, 18.5));
        body.lineTo(pt(4.5, 14.5));
        body.lineTo(pt(13.0, 6.0));
        body.lineTo(pt(19.5, 12.5));
        body.lineTo(pt(13.5, 18.5));
        body.closeSubpath();
        p.drawPath(body);
        p.drawLine(pt(9.0, 10.0), pt(15.5, 16.5));
        p.drawLine(pt(8.5, 18.5), pt(20.0, 18.5));
    } else if (name == "repair") {
        p.drawEllipse(QRectF(pt(4.5, 4.5), pt(19.5, 19.5)));
        p.drawLine(pt(12.0, 8.2), pt(12.0, 15.8));
        p.drawLine(pt(8.2, 12.0), pt(15.8, 12.0));
    } else if (name == "upscale") {
        p.drawRoundedRect(QRectF(pt(4.5, 11.5), pt(12.5, 19.5)), 2.0 * u, 2.0 * u);
        p.drawLine(pt(11.0, 13.0), pt(19.0, 5.0));
        p.drawLine(pt(14.0, 5.0), pt(19.0, 5.0));
        p.drawLine(pt(19.0, 5.0), pt(19.0, 10.0));
    } else if (name == "resize") {
        p.drawRoundedRect(QRectF(pt(4.5, 4.5), pt(19.5, 19.5)), 2.5 * u, 2.5 * u);
        p.drawLine(pt(8.5, 15.5), pt(15.5, 8.5));
        p.drawLine(pt(12.0, 8.5), pt(15.5, 8.5));
        p.drawLine(pt(15.5, 8.5), pt(15.5, 12.0));
    } else if (name == "compare") {
        p.drawRoundedRect(QRectF(pt(4.5, 5.5), pt(19.5, 18.5)), 2.5 * u, 2.5 * u);
        p.drawLine(pt(12.0, 3.5), pt(12.0, 20.5));
    } else if (name == "sidebar") {
        p.drawRoundedRect(QRectF(pt(4.5, 5.5), pt(19.5, 18.5)), 2.5 * u, 2.5 * u);
        p.drawLine(pt(10.0, 5.5), pt(10.0, 18.5));
    } else if (name == "chevron_left") {
        p.drawLine(pt(14.5, 6.5), pt(9.0, 12.0));
        p.drawLine(pt(9.0, 12.0), pt(14.5, 17.5));
    } else if (name == "chevron_right") {
        p.drawLine(pt(9.5, 6.5), pt(15.0, 12.0));
        p.drawLine(pt(15.0, 12.0), pt(9.5, 17.5));
    } else if (name == "image_drop") {
        p.drawRoundedRect(QRectF(pt(3.5, 4.5), pt(20.5, 19.5)), 3.2 * u, 3.2 * u);
        p.drawEllipse(pt(9.0, 9.5), 1.6 * u, 1.6 * u);
        QPainterPath hills;
        hills.moveTo(pt(5.0, 17.5));
        hills.lineTo(pt(10.0, 12.8));
        hills.lineTo(pt(13.2, 15.6));
        hills.lineTo(pt(16.2, 11.8));
        hills.lineTo(pt(19.2, 17.5));
        p.drawPath(hills);
    } else if (name == "sync") {
        p.drawArc(QRectF(pt(5.5, 5.5), pt(18.5, 18.5)), 35 * 16, 130 * 16);
        p.drawLine(pt(16.8, 7.6), pt(18.6, 8.2));
        p.drawLine(pt(18.6, 8.2), pt(17.9, 6.2));
        p.drawArc(QRectF(pt(5.5, 5.5), pt(18.5, 18.5)), 215 * 16, 130 * 16);
        p.drawLine(pt(7.2, 16.4), pt(5.4, 15.8));
        p.drawLine(pt(5.4, 15.8), pt(6.1, 17.8));
    } else if (name == "settings") {
        p.drawEllipse(pt(12.0, 12.0), 3.1 * u, 3.1 * u);
        p.drawEllipse(pt(12.0, 12.0), 6.2 * u, 6.2 * u);
        for (int i = 0; i < 8; ++i) {
            const double ang = i * (3.14159265358979323846 / 4.0);
            const qreal c = std::cos(ang);
            const qreal sn = std::sin(ang);
            p.drawLine(pt(12.0 + 6.4 * c, 12.0 + 6.4 * sn),
                       pt(12.0 + 8.6 * c, 12.0 + 8.6 * sn));
        }
    } else if (name == "tune") {
        p.drawLine(pt(4.5, 8.0), pt(19.5, 8.0));
        p.drawLine(pt(4.5, 16.0), pt(19.5, 16.0));
        p.setBrush(color);
        p.drawEllipse(pt(14.5, 8.0), 2.1 * u, 2.1 * u);
        p.drawEllipse(pt(9.5, 16.0), 2.1 * u, 2.1 * u);
    } else if (name == "pause") {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(pt(6.5, 5.5), pt(10.2, 18.5)), 1.4 * u, 1.4 * u);
        p.drawRoundedRect(QRectF(pt(13.8, 5.5), pt(17.5, 18.5)), 1.4 * u, 1.4 * u);
    } else if (name == "play") {
        QPainterPath tri;
        tri.moveTo(pt(7.5, 5.5));
        tri.lineTo(pt(18.5, 12.0));
        tri.lineTo(pt(7.5, 18.5));
        tri.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(tri);
    } else if (name == "stop") {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(pt(6.5, 6.5), pt(17.5, 17.5)), 2.2 * u, 2.2 * u);
    } else if (name == "infrared") {
        p.drawEllipse(pt(12.0, 12.0), 7.5 * u, 7.5 * u);
        p.drawEllipse(pt(12.0, 12.0), 3.6 * u, 3.6 * u);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(pt(12.0, 12.0), 1.6 * u, 1.6 * u);
        p.drawEllipse(pt(17.5, 6.5), 1.3 * u, 1.3 * u);
        p.drawEllipse(pt(6.2, 16.8), 1.1 * u, 1.1 * u);
    } else if (name == "mask") {
        p.drawRoundedRect(QRectF(pt(4.2, 4.2), pt(19.8, 19.8)), 3.0 * u, 3.0 * u);
        p.drawEllipse(pt(12.0, 12.0), 4.6 * u, 4.6 * u);
        p.drawLine(pt(12.0, 4.2), pt(12.0, 19.8));
    } else if (name == "win_min") {
        p.drawLine(pt(6.0, 12.5), pt(18.0, 12.5));
    } else if (name == "win_max") {
        p.drawRoundedRect(QRectF(pt(6.0, 6.0), pt(18.0, 18.0)), 2.2 * u, 2.2 * u);
    } else if (name == "win_restore") {
        p.drawRoundedRect(QRectF(pt(5.5, 8.5), pt(15.5, 18.5)), 1.8 * u, 1.8 * u);
        QPainterPath back;
        back.moveTo(pt(8.5, 8.5));
        back.lineTo(pt(8.5, 5.5));
        back.lineTo(pt(18.5, 5.5));
        back.lineTo(pt(18.5, 15.5));
        back.lineTo(pt(15.5, 15.5));
        p.drawPath(back);
    } else if (name == "win_close") {
        p.drawLine(pt(6.5, 6.5), pt(17.5, 17.5));
        p.drawLine(pt(17.5, 6.5), pt(6.5, 17.5));
    } else if (name == "alert_warn") {
        QPainterPath tri;
        tri.moveTo(pt(12.0, 3.8));
        tri.lineTo(pt(20.4, 19.2));
        tri.lineTo(pt(3.6, 19.2));
        tri.closeSubpath();
        p.drawPath(tri);
        p.drawLine(pt(12.0, 9.2), pt(12.0, 13.8));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(pt(12.0, 16.5), 1.15 * u, 1.15 * u);
    } else if (name == "alert_error") {
        p.drawEllipse(QRectF(pt(3.8, 3.8), pt(20.2, 20.2)));
        p.drawLine(pt(8.5, 8.5), pt(15.5, 15.5));
        p.drawLine(pt(15.5, 8.5), pt(8.5, 15.5));
    } else if (name == "alert_check") {
        p.drawEllipse(QRectF(pt(3.8, 3.8), pt(20.2, 20.2)));
        p.drawLine(pt(8.0, 12.2), pt(10.8, 15.0));
        p.drawLine(pt(10.8, 15.0), pt(16.4, 9.2));
    } else if (name == "alert_info") {
        p.drawEllipse(QRectF(pt(3.8, 3.8), pt(20.2, 20.2)));
        p.drawLine(pt(12.0, 11.2), pt(12.0, 16.6));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(pt(12.0, 8.0), 1.15 * u, 1.15 * u);
    } else if (name == "alert_question") {
        p.drawEllipse(QRectF(pt(3.8, 3.8), pt(20.2, 20.2)));
        QPainterPath q;
        q.moveTo(pt(9.4, 9.4));
        q.cubicTo(pt(9.4, 7.0), pt(14.6, 7.0), pt(14.6, 9.6));
        q.cubicTo(pt(14.6, 11.4), pt(12.0, 11.8), pt(12.0, 13.6));
        p.drawPath(q);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(pt(12.0, 16.4), 1.1 * u, 1.1 * u);
    }

    p.restore();
}

QIcon createIcon(const QString& name, const QColor& color, int size) {
    QPixmap pix(size * 2, size * 2);
    pix.setDevicePixelRatio(2.0);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    paintIcon(p, name, QRectF(0, 0, size, size), color, 1.65);
    return QIcon(pix);
}

} // namespace GlassIcons

// ============================================================================
// GlassPanel — Cached HiDPI Dark Smoked Liquid Glass (Ultra-Fast 1-Blit Paint)
// ============================================================================
GlassPanel::GlassPanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { invalidateCache(); });
}

void GlassPanel::invalidateCache() {
    m_cacheDirty = true;
    update();
}

void GlassPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_cacheDirty = true;
}

void GlassPanel::rebuildShellCache() {
    const qreal dpr = devicePixelRatioF();
    const QSize physSize(std::max(1, static_cast<int>(std::ceil(width() * dpr))),
                         std::max(1, static_cast<int>(std::ceil(height() * dpr))));
    m_cachedShell = QPixmap(physSize);
    m_cachedShell.setDevicePixelRatio(dpr);
    m_cachedShell.fill(Qt::transparent);
    m_cachedDpr = dpr;
    m_cacheDirty = false;

    QPainter p(&m_cachedShell);
    p.setRenderHint(QPainter::Antialiasing, true);

    const GlassTheme& th = ThemeManager::instance().current();
    const bool docked = (m_radius <= 0.5);
    QRectF r = docked ? QRectF(rect()) : QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    QPainterPath clipPath;
    if (docked) {
        clipPath.addRect(r);
    } else {
        clipPath.addRoundedRect(r, m_radius, m_radius);
    }

    // Soft outer contact shadow for floating separation
    if (m_elevated && !docked) {
        p.save();
        p.setPen(QPen(QColor(0, 0, 0, 128), 2.6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(-0.5, 1.0, 0.5, 1.8), m_radius, m_radius);
        p.restore();
    }

    p.save();
    p.setClipPath(clipPath);

    // 1. Signature Smoked Translucent Liquid Glass body (naturally composites over live canvas in 1 blit!)
    QLinearGradient glassBody(r.topLeft(), r.bottomLeft());
    if (m_subSurface) {
        glassBody.setColorAt(0.0, QColor(32, 34, 38, 198));
        glassBody.setColorAt(1.0, QColor(22, 23, 27, 218));
    } else {
        QColor topC = th.glassBaseTop;
        QColor botC = th.glassBaseBottom;
        topC.setAlpha(std::min(255, topC.alpha() + 34));
        botC.setAlpha(std::min(255, botC.alpha() + 30));
        glassBody.setColorAt(0.0, topC);
        glassBody.setColorAt(0.52, lerpColor(topC, botC, 0.58));
        glassBody.setColorAt(1.0, botC);
    }
    p.fillRect(r, glassBody);

    // 2. Subtle frosted milky-gray refractive wash
    if (!m_subSurface) {
        QLinearGradient milkyWash(r.topLeft(), r.bottomRight());
        milkyWash.setColorAt(0.0, QColor(238, 240, 244, 24));
        milkyWash.setColorAt(0.45, QColor(180, 184, 190, 10));
        milkyWash.setColorAt(1.0, QColor(214, 218, 224, 18));
        p.fillRect(r, milkyWash);
    }

    // 3. Delicate theme-matched atmospheric luminescence inside the glass
    if (m_accentGlow && !m_subSurface) {
        QRadialGradient glow(QPointF(r.left() + r.width() * 0.25, r.top() + r.height() * 0.14),
                             std::max(r.width(), r.height()) * 0.85);
        glow.setColorAt(0.0, th.glassGlow);
        glow.setColorAt(0.6, QColor(th.glassGlow.red(), th.glassGlow.green(), th.glassGlow.blue(), th.glassGlow.alpha() / 4));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(r, glow);
    }

    // 4. Top liquid-glass meniscus highlight + bottom caustic bounce
    qreal sheenH = std::min(44.0, r.height() * 0.44);
    QLinearGradient topMeniscus(r.topLeft(), QPointF(r.left(), r.top() + sheenH));
    topMeniscus.setColorAt(0.0, QColor(255, 255, 255, m_subSurface ? 18 : 44));
    topMeniscus.setColorAt(0.45, QColor(255, 255, 255, m_subSurface ? 5 : 13));
    topMeniscus.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(r, topMeniscus);

    if (!m_subSurface) {
        QLinearGradient botBounce(QPointF(r.left(), r.bottom() - 22.0), r.bottomLeft());
        botBounce.setColorAt(0.0, QColor(255, 255, 255, 0));
        botBounce.setColorAt(1.0, QColor(255, 255, 255, 20));
        p.fillRect(r, botBounce);
    }

    p.restore();

    // 5. Double-edge Apple Liquid Glass specular refraction rim (or subtle crisp hairline for docked bars)
    if (docked) {
        p.setPen(QPen(QColor(255, 255, 255, 26), 1.0));
        p.drawLine(r.bottomLeft(), r.bottomRight());
        p.drawLine(r.topRight(), r.bottomRight());
    } else {
        p.setPen(QPen(QColor(255, 255, 255, m_subSurface ? 16 : 28), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(1.0, 1.0, -1.0, -1.0), std::max(1.0, m_radius - 1.0), std::max(1.0, m_radius - 1.0));

        QLinearGradient rimGrad(r.topLeft(), r.bottomRight());
        rimGrad.setColorAt(0.0, th.glassRimTop);
        rimGrad.setColorAt(0.28, QColor(255, 255, 255, 56));
        rimGrad.setColorAt(0.65, QColor(255, 255, 255, 24));
        rimGrad.setColorAt(1.0, th.glassRimBottom);
        p.setPen(QPen(QBrush(rimGrad), 1.25));
        p.drawRoundedRect(r, m_radius, m_radius);
    }
}

void GlassPanel::paintEvent(QPaintEvent* event) {
    if (m_cacheDirty || m_cachedShell.isNull() || std::abs(m_cachedDpr - devicePixelRatioF()) > 0.01) {
        rebuildShellCache();
    }
    QPainter p(this);
    const QRect r = event->rect();
    p.drawPixmap(r.topLeft(), m_cachedShell, QRectF(r.x() * m_cachedDpr, r.y() * m_cachedDpr,
                                                    r.width() * m_cachedDpr, r.height() * m_cachedDpr));
}

void GlassPanel::mousePressEvent(QMouseEvent* event) { event->accept(); }
void GlassPanel::mouseMoveEvent(QMouseEvent* event) { event->accept(); }
void GlassPanel::mouseReleaseEvent(QMouseEvent* event) { event->accept(); }
void GlassPanel::wheelEvent(QWheelEvent* event) { event->accept(); }

// ============================================================================
// LiquidGlassTabBar — Smooth Sliding Liquid Glass Bubble Navigation!
// ============================================================================
LiquidGlassTabBar::LiquidGlassTabBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    m_slideAnim = new QVariantAnimation(this);
    m_slideAnim->setDuration(320);
    m_slideAnim->setEasingCurve(QEasingCurve::OutBack);
    connect(m_slideAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_indicatorRect = val.toRectF();
        qreal t = (m_slideAnim->duration() > 0)
            ? static_cast<qreal>(m_slideAnim->currentTime()) / m_slideAnim->duration()
            : 1.0;
        // Subtle liquid surface-tension stretch at midpoint of glide
        m_stretchAmount = std::sin(std::clamp(t, 0.0, 1.0) * 3.14159265) * 2.5;
        update();
    });
    connect(m_slideAnim, &QVariantAnimation::finished, this, [this]() {
        m_stretchAmount = 0.0;
        update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

void LiquidGlassTabBar::setPillHeight(int h) {
    m_pillHeight = h;
    setFixedHeight(h);
    syncGeometryInstant();
    update();
}

void LiquidGlassTabBar::addTab(int id, const QString& label, const QString& iconName,
                               const QString& shortcut, const QString& tooltip) {
    m_tabs.push_back(TabItem{id, label, iconName, shortcut, tooltip});
    setFixedHeight(m_pillHeight);
    updateGeometry();
    syncGeometryInstant();
    update();
}

int LiquidGlassTabBar::currentId() const {
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_tabs.size())) {
        return m_tabs[m_currentIndex].id;
    }
    return -1;
}

void LiquidGlassTabBar::setCurrentId(int id, bool animate) {
    int targetIdx = -1;
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        if (m_tabs[i].id == id) {
            targetIdx = i;
            break;
        }
    }
    if (targetIdx < 0) return;

    QRectF targetRect = tabRectAt(targetIdx);
    if (!animate || m_indicatorRect.isEmpty() || targetIdx == m_currentIndex) {
        m_currentIndex = targetIdx;
        m_indicatorRect = targetRect;
        m_stretchAmount = 0.0;
        update();
        return;
    }

    QRectF startRect = m_indicatorRect.isEmpty() ? tabRectAt(m_currentIndex) : m_indicatorRect;
    m_currentIndex = targetIdx;

    m_slideAnim->stop();
    m_slideAnim->setStartValue(startRect);
    m_slideAnim->setEndValue(targetRect);
    m_slideAnim->start();
}

QSize LiquidGlassTabBar::sizeHint() const {
    if (m_tabs.empty()) return QSize(120, m_pillHeight);
    QFont f = font();
    f.setPixelSize(12);
    f.setWeight(QFont::DemiBold);
    QFontMetrics fm(f);

    int totalW = 6;
    for (const auto& t : m_tabs) {
        int tw = fm.horizontalAdvance(t.label);
        int iw = t.iconName.isEmpty() ? 0 : 21;
        int sw = t.shortcut.isEmpty() ? 0 : (fm.horizontalAdvance(t.shortcut) + 18);
        totalW += std::max(54, tw + iw + sw + 26);
    }
    totalW += 6;
    return QSize(totalW, m_pillHeight);
}

QRectF LiquidGlassTabBar::tabRectAt(int index) const {
    if (m_tabs.empty() || index < 0 || index >= static_cast<int>(m_tabs.size())) return QRectF();
    QFont f = font();
    f.setPixelSize(12);
    f.setWeight(QFont::DemiBold);
    QFontMetrics fm(f);

    std::vector<qreal> weights;
    qreal sumW = 0.0;
    for (const auto& t : m_tabs) {
        int tw = fm.horizontalAdvance(t.label);
        int iw = t.iconName.isEmpty() ? 0 : 21;
        int sw = t.shortcut.isEmpty() ? 0 : (fm.horizontalAdvance(t.shortcut) + 18);
        qreal w = std::max(54.0, static_cast<qreal>(tw + iw + sw + 26));
        weights.push_back(w);
        sumW += w;
    }

    const qreal pad = 3.0;
    const qreal innerW = std::max(1.0, width() - pad * 2.0);
    qreal x = pad;
    for (int i = 0; i < index; ++i) {
        x += (weights[i] / sumW) * innerW;
    }
    qreal w = (weights[index] / sumW) * innerW;
    return QRectF(x, pad, w, height() - pad * 2.0);
}

int LiquidGlassTabBar::hitTestTab(const QPointF& pos) const {
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        if (tabRectAt(i).contains(pos)) return i;
    }
    return -1;
}

void LiquidGlassTabBar::syncGeometryInstant() {
    if (!m_tabs.empty() && m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_tabs.size())) {
        m_indicatorRect = tabRectAt(m_currentIndex);
    }
}

void LiquidGlassTabBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_slideAnim->state() != QAbstractAnimation::Running) {
        syncGeometryInstant();
    }
}

void LiquidGlassTabBar::paintEvent(QPaintEvent*) {
    if (m_tabs.empty()) return;

    const qreal dpr = devicePixelRatioF();
    const bool animating = (m_slideAnim->state() == QAbstractAnimation::Running);
    quint64 key = 0x101ULL;
    key = mixHash(key, static_cast<quint64>(width()));
    key = mixHash(key, static_cast<quint64>(height()));
    key = mixHash(key, static_cast<quint64>(std::lround(dpr * 100.0)));
    key = mixHash(key, static_cast<quint64>(ThemeManager::instance().currentIndex()));
    key = mixHash(key, static_cast<quint64>(m_currentIndex + 16));
    key = mixHash(key, static_cast<quint64>(m_hoveredIndex + 16));
    key = mixHash(key, static_cast<quint64>(m_tabs.size()));
    key = mixHash(key, static_cast<quint64>(isEnabled() ? 1 : 0));

    if (!animating && key == m_cacheKey && !m_cachePixmap.isNull()) {
        QPainter screenP(this);
        screenP.drawPixmap(0, 0, m_cachePixmap);
        return;
    }

    QPixmap pm;
    QPainter p;
    if (!animating) {
        pm = makeHiDpiPixmap(width(), height(), dpr);
        p.begin(&pm);
    } else {
        p.begin(this);
    }
    p.setRenderHint(QPainter::Antialiasing, true);

    const GlassTheme& th = ThemeManager::instance().current();

    // Subtle hover pill behind hovered inactive tab
    if (m_hoveredIndex >= 0 && m_hoveredIndex != m_currentIndex) {
        QRectF hr = tabRectAt(m_hoveredIndex);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 20));
        p.drawRoundedRect(hr, hr.height() * 0.5, hr.height() * 0.5);
    }

    // Sliding Liquid Glass Refractive Lens Bubble (with subtle liquid stretch mid-flight!)
    QRectF bubble = m_indicatorRect.isEmpty() ? tabRectAt(m_currentIndex) : m_indicatorRect;
    if (m_stretchAmount > 0.01) {
        bubble.adjust(-m_stretchAmount, m_stretchAmount * 0.35, m_stretchAmount, -m_stretchAmount * 0.35);
    }
    qreal bRad = bubble.height() * 0.5;

    // Soft ambient theme glow beneath the active liquid glass lens
    p.save();
    QRadialGradient bubbleGlow(bubble.center(), bubble.width() * 0.65);
    bubbleGlow.setColorAt(0.0, QColor(th.accentStart.red(), th.accentStart.green(), th.accentStart.blue(), 66));
    bubbleGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(bubbleGlow);
    p.drawRoundedRect(bubble.adjusted(-2, -1, 2, 2), bRad, bRad);
    p.restore();

    // Frosted grayish-silver liquid glass lens body
    QLinearGradient lensFill(bubble.topLeft(), bubble.bottomLeft());
    lensFill.setColorAt(0.0, QColor(235, 240, 250, 74));
    lensFill.setColorAt(0.5, QColor(168, 178, 196, 42));
    lensFill.setColorAt(1.0, QColor(200, 210, 228, 56));
    p.setPen(Qt::NoPen);
    p.setBrush(lensFill);
    p.drawRoundedRect(bubble, bRad, bRad);

    // Top bright meniscus gloss inside the lens
    QLinearGradient lensGloss(bubble.topLeft(), QPointF(bubble.left(), bubble.center().y() + 2.0));
    lensGloss.setColorAt(0.0, QColor(255, 255, 255, 92));
    lensGloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setBrush(lensGloss);
    p.drawRoundedRect(bubble.adjusted(2.0, 1.2, -2.0, -bubble.height() * 0.44), bRad, bRad);

    // Crisp specular double-edge lens rim
    QLinearGradient lensRim(bubble.topLeft(), bubble.bottomRight());
    lensRim.setColorAt(0.0, QColor(255, 255, 255, 215));
    lensRim.setColorAt(0.45, QColor(255, 255, 255, 65));
    lensRim.setColorAt(1.0, QColor(th.accentEnd.red(), th.accentEnd.green(), th.accentEnd.blue(), 170));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(lensRim), 1.25));
    p.drawRoundedRect(bubble.adjusted(0.5, 0.5, -0.5, -0.5), bRad, bRad);

    // Draw Tab Icons, Labels & Shortcut Pills
    QFont tf = font();
    tf.setPixelSize(12);
    tf.setWeight(QFont::DemiBold);
    QFontMetrics fm(tf);

    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        const auto& t = m_tabs[i];
        QRectF tr = tabRectAt(i);
        bool active = (i == m_currentIndex);
        bool hovered = (i == m_hoveredIndex);

        QColor txtCol = active ? QColor(255, 255, 255) : (hovered ? QColor(238, 244, 255) : QColor(196, 204, 218));
        QColor icoCol = active ? th.accentEnd : txtCol;

        qreal iconS = t.iconName.isEmpty() ? 0.0 : 15.0;
        qreal gap = (!t.iconName.isEmpty() && !t.label.isEmpty()) ? 6.0 : 0.0;
        int txtW = fm.horizontalAdvance(t.label);
        int scW = t.shortcut.isEmpty() ? 0 : (fm.horizontalAdvance(t.shortcut) + 12);
        qreal scGap = t.shortcut.isEmpty() ? 0.0 : 7.0;
        qreal totalW = iconS + gap + txtW + scGap + scW;

        qreal x = tr.center().x() - totalW * 0.5;
        if (!t.iconName.isEmpty()) {
            GlassIcons::paintIcon(p, t.iconName, QRectF(x, tr.center().y() - iconS * 0.5, iconS, iconS), icoCol, 1.85);
            x += iconS + gap;
        }
        p.setFont(tf);
        p.setPen(txtCol);
        p.drawText(QRectF(x, tr.top(), txtW + 2, tr.height()), Qt::AlignVCenter | Qt::AlignLeft, t.label);
        x += txtW + scGap;

        if (!t.shortcut.isEmpty()) {
            QRectF br(x, tr.center().y() - 9.0, scW, 18.0);
            p.setPen(QPen(QColor(255, 255, 255, active ? 76 : 42), 0.9));
            p.setBrush(QColor(255, 255, 255, active ? 32 : 16));
            p.drawRoundedRect(br, 6.0, 6.0);
            QFont bf = tf;
            bf.setPixelSize(10);
            p.setFont(bf);
            p.setPen(active ? Qt::white : QColor(200, 208, 222));
            p.drawText(br, Qt::AlignCenter, t.shortcut);
        }
    }

    p.end();
    if (!animating) {
        m_cachePixmap = std::move(pm);
        m_cacheKey = key;
        QPainter screenP(this);
        screenP.drawPixmap(0, 0, m_cachePixmap);
    }
}

void LiquidGlassTabBar::mouseMoveEvent(QMouseEvent* event) {
    int idx = hitTestTab(event->position());
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        if (idx >= 0 && !m_tabs[idx].tooltip.isEmpty()) {
            setToolTip(m_tabs[idx].tooltip);
        }
        update();
    }
    event->accept();
}

void LiquidGlassTabBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int idx = hitTestTab(event->position());
        if (idx >= 0) {
            int id = m_tabs[idx].id;
            setCurrentId(id, true);
            emit tabSelected(id);
        }
        event->accept();
    }
}

void LiquidGlassTabBar::leaveEvent(QEvent*) {
    m_hoveredIndex = -1;
    update();
}

// ============================================================================
// GlassButton — Animated Liquid Glass Capsule & Circular Medallion Buttons
// ============================================================================
GlassButton::GlassButton(const QString& text, Variant variant, QWidget* parent)
    : QPushButton(text, parent)
    , m_variant(variant)
{
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setFlat(true);
    QFont f = font();
    f.setWeight(QFont::DemiBold);
    f.setPixelSize(12);
    setFont(f);

    m_hoverAnim = new QVariantAnimation(this);
    m_hoverAnim->setDuration(170);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_hoverFactor = v.toReal();
        update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

void GlassButton::enterEvent(QEnterEvent* event) {
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hoverFactor);
    m_hoverAnim->setEndValue(1.0);
    m_hoverAnim->start();
    QPushButton::enterEvent(event);
}

void GlassButton::leaveEvent(QEvent* event) {
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hoverFactor);
    m_hoverAnim->setEndValue(0.0);
    m_hoverAnim->start();
    QPushButton::leaveEvent(event);
}

QSize GlassButton::sizeHint() const {
    if (m_variant == IconCircle) {
        return QSize(34, 34);
    }
    QFontMetrics fm(font());
    int textW = text().isEmpty() ? 0 : fm.horizontalAdvance(text());
    int iconW = m_iconName.isEmpty() ? 0 : 18;
    int gap = (!text().isEmpty() && !m_iconName.isEmpty()) ? 7 : 0;
    int badgeW = m_shortcut.isEmpty() ? 0 : (fm.horizontalAdvance(m_shortcut) + 14);
    int padX = (m_variant == Primary || m_variant == White) ? 32 : 24;
    int h = (m_variant == Primary || m_variant == White) ? 34 : 32;
    return QSize(textW + iconW + gap + badgeW + padX, h);
}

void GlassButton::paintEvent(QPaintEvent*) {
    const qreal dpr = devicePixelRatioF();
    const bool pressed = isDown() && isEnabled();
    const bool checked = isChecked();
    const qreal hf = isEnabled() ? m_hoverFactor : 0.0;
    const int hfQ = static_cast<int>(std::lround(hf * 100.0));

    quint64 key = 0x202ULL;
    key = mixHash(key, static_cast<quint64>(width()));
    key = mixHash(key, static_cast<quint64>(height()));
    key = mixHash(key, static_cast<quint64>(std::lround(dpr * 100.0)));
    key = mixHash(key, static_cast<quint64>(ThemeManager::instance().currentIndex()));
    key = mixHash(key, static_cast<quint64>(m_variant));
    key = mixHash(key, static_cast<quint64>((isEnabled() ? 1 : 0) | (pressed ? 2 : 0) | (checked ? 4 : 0)));
    key = mixHash(key, static_cast<quint64>(hfQ));
    key = mixHash(key, static_cast<quint64>(qHash(text())));
    key = mixHash(key, static_cast<quint64>(qHash(m_iconName)));
    key = mixHash(key, static_cast<quint64>(qHash(m_shortcut)));

    if (key == m_cacheKey && !m_cachePixmap.isNull()) {
        QPainter screenP(this);
        screenP.drawPixmap(0, 0, m_cachePixmap);
        return;
    }

    QPixmap pm = makeHiDpiPixmap(width(), height(), dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const GlassTheme& th = ThemeManager::instance().current();
    QRectF r = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
    qreal rad = (m_radius >= 0.0) ? m_radius : (r.height() * 0.5);

    QColor textColor(242, 246, 252);
    QColor iconColor(236, 242, 250);

    if (m_variant == Primary) {
        QLinearGradient bg(r.topLeft(), r.bottomRight());
        if (!isEnabled()) {
            bg.setColorAt(0.0, QColor(68, 74, 86, 140));
            bg.setColorAt(1.0, QColor(48, 52, 62, 140));
            textColor = QColor(145, 154, 168);
            iconColor = textColor;
        } else {
            QColor c0 = pressed ? th.accentStart.darker(115) : lerpColor(th.accentStart, th.accentStart.lighter(118), hf);
            QColor c1 = pressed ? th.accentEnd.darker(115) : lerpColor(th.accentEnd, th.accentEnd.lighter(115), hf);
            bg.setColorAt(0.0, c0);
            bg.setColorAt(1.0, c1);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(r, rad, rad);

        if (isEnabled()) {
            QLinearGradient gloss(r.topLeft(), QPointF(r.left(), r.top() + r.height() * 0.55));
            gloss.setColorAt(0.0, QColor(255, 255, 255, 76));
            gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(gloss);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -r.height() * 0.42), rad, rad);
        }

        QLinearGradient rim(r.topLeft(), r.bottomRight());
        rim.setColorAt(0.0, QColor(255, 255, 255, 175));
        rim.setColorAt(1.0, QColor(255, 255, 255, 70));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QBrush(rim), 1.15));
        p.drawRoundedRect(r, rad, rad);
        textColor = Qt::white;
        iconColor = Qt::white;
    } else if (m_variant == White) {
        QColor fill = !isEnabled() ? QColor(140, 146, 158, 100)
                    : (pressed ? QColor(216, 222, 232) : lerpColor(QColor(238, 242, 248), QColor(255, 255, 255), hf));
        p.setPen(QPen(QColor(255, 255, 255, 225), 1.0));
        p.setBrush(fill);
        p.drawRoundedRect(r, rad, rad);
        textColor = !isEnabled() ? QColor(80, 88, 102) : QColor(10, 12, 18);
        iconColor = textColor;
    } else if (m_variant == Danger) {
        QLinearGradient bg(r.topLeft(), r.bottomRight());
        QColor c0 = pressed ? QColor(140, 32, 46) : lerpColor(QColor(156, 38, 54, 215), QColor(196, 48, 66, 238), hf);
        QColor c1 = pressed ? QColor(102, 24, 34) : lerpColor(QColor(118, 28, 42, 225), QColor(154, 36, 52, 242), hf);
        bg.setColorAt(0.0, c0);
        bg.setColorAt(1.0, c1);
        p.setBrush(bg);
        p.setPen(QPen(QColor(255, 160, 174, static_cast<int>(108 + 70 * hf)), 1.05));
        p.drawRoundedRect(r, rad, rad);
        textColor = Qt::white;
        iconColor = Qt::white;
    } else if (m_variant == IconCircle) {
        QColor fill = checked ? QColor(th.accentStart.red(), th.accentStart.green(), th.accentStart.blue(), 86)
                    : (pressed ? QColor(255, 255, 255, 58)
                    : lerpColor(QColor(255, 255, 255, 26), QColor(255, 255, 255, 52), hf));
        p.setBrush(fill);
        QLinearGradient rim(r.topLeft(), r.bottomRight());
        if (checked) {
            rim.setColorAt(0.0, QColor(255, 255, 255, 195));
            rim.setColorAt(1.0, QColor(th.accentEnd.red(), th.accentEnd.green(), th.accentEnd.blue(), 170));
        } else {
            rim.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(92 + 78 * hf)));
            rim.setColorAt(1.0, QColor(255, 255, 255, static_cast<int>(36 + 34 * hf)));
        }
        p.setPen(QPen(QBrush(rim), 1.1));
        p.drawEllipse(r);
        iconColor = !isEnabled() ? QColor(125, 132, 146) : (checked ? Qt::white : QColor(242, 246, 252));
    } else { // Ghost — Grayish Liquid Glass Pill
        QLinearGradient bg(r.topLeft(), r.bottomLeft());
        if (checked) {
            bg.setColorAt(0.0, QColor(245, 248, 255, 68));
            bg.setColorAt(1.0, QColor(180, 190, 208, 42));
        } else {
            int aTop = pressed ? 58 : static_cast<int>(32 + 26 * hf);
            int aBot = pressed ? 38 : static_cast<int>(18 + 18 * hf);
            bg.setColorAt(0.0, QColor(230, 236, 248, aTop));
            bg.setColorAt(1.0, QColor(160, 168, 184, aBot));
        }
        p.setBrush(bg);

        QLinearGradient rim(r.topLeft(), r.bottomRight());
        rim.setColorAt(0.0, QColor(255, 255, 255, checked ? 175 : static_cast<int>(86 + 68 * hf)));
        rim.setColorAt(1.0, QColor(255, 255, 255, checked ? 90 : static_cast<int>(34 + 34 * hf)));
        p.setPen(QPen(QBrush(rim), 1.05));
        p.drawRoundedRect(r, rad, rad);

        textColor = isEnabled() ? QColor(242, 246, 252) : QColor(122, 130, 144);
        iconColor = checked ? th.accentEnd : textColor;
    }

    const QFontMetrics fm(font());
    const int textW = text().isEmpty() ? 0 : fm.horizontalAdvance(text());
    const qreal iconS = (m_variant == IconCircle) ? 16.0 : 15.0;
    const qreal gap = (!text().isEmpty() && !m_iconName.isEmpty()) ? 7.0 : 0.0;
    const int badgeW = m_shortcut.isEmpty() ? 0 : (fm.horizontalAdvance(m_shortcut) + 12);
    const qreal badgeGap = m_shortcut.isEmpty() ? 0.0 : 8.0;
    const qreal totalW = (m_iconName.isEmpty() ? 0.0 : iconS) + gap + textW + badgeGap + badgeW;

    qreal x = r.center().x() - totalW * 0.5;
    if (!m_iconName.isEmpty()) {
        GlassIcons::paintIcon(p, m_iconName, QRectF(x, r.center().y() - iconS * 0.5, iconS, iconS), iconColor, 1.75);
        x += iconS + gap;
    }
    if (!text().isEmpty()) {
        p.setPen(textColor);
        p.setFont(font());
        p.drawText(QRectF(x, r.top(), textW + 2, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        x += textW + badgeGap;
    }
    if (!m_shortcut.isEmpty()) {
        QRectF bRect(x, r.center().y() - 9.0, badgeW, 18.0);
        if (m_variant == Primary) {
            p.setPen(QPen(QColor(255, 255, 255, 115), 0.9));
            p.setBrush(QColor(10, 14, 24, 68));
        } else {
            p.setPen(QPen(QColor(255, 255, 255, 58), 0.9));
            p.setBrush(QColor(255, 255, 255, 26));
        }
        p.drawRoundedRect(bRect, 5.0, 5.0);
        QFont bf = font();
        bf.setPixelSize(10);
        p.setFont(bf);
        p.setPen(m_variant == Primary ? QColor(255, 255, 255) : QColor(225, 232, 244));
        p.drawText(bRect, Qt::AlignCenter, m_shortcut);
    }

    p.end();
    m_cachePixmap = std::move(pm);
    m_cacheKey = key;
    QPainter screenP(this);
    screenP.drawPixmap(0, 0, m_cachePixmap);
}

// ============================================================================
// GlassToggleSwitch — Smoothly Animated iOS Liquid Glass Switch
// ============================================================================
GlassToggleSwitch::GlassToggleSwitch(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(46, 26);

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(200);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_knobPos = v.toReal();
        update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

void GlassToggleSwitch::animateToState(bool checked) {
    m_anim->stop();
    m_anim->setStartValue(m_knobPos);
    m_anim->setEndValue(checked ? 1.0 : 0.0);
    m_anim->start();
}

void GlassToggleSwitch::checkStateSet() {
    QAbstractButton::checkStateSet();
    animateToState(isChecked());
}

void GlassToggleSwitch::nextCheckState() {
    QAbstractButton::nextCheckState();
    animateToState(isChecked());
}

void GlassToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const GlassTheme& th = ThemeManager::instance().current();
    QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    qreal rad = r.height() * 0.5;

    QColor trackOff(16, 19, 26, 165);
    QColor trackOn = th.toggleActive;
    p.setBrush(lerpColor(trackOff, trackOn, m_knobPos));
    p.setPen(QPen(QColor(255, 255, 255, static_cast<int>(64 + 55 * m_knobPos)), 1.0));
    p.drawRoundedRect(r, rad, rad);

    qreal knobD = r.height() - 4.5;
    qreal minX = r.left() + 2.25;
    qreal maxX = r.right() - knobD - 2.25;
    qreal knobX = minX + (maxX - minX) * m_knobPos;
    QRectF knobRect(knobX, r.top() + 2.25, knobD, knobD);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 85));
    p.drawEllipse(knobRect.translated(0, 1.2));

    p.setBrush(Qt::white);
    p.drawEllipse(knobRect);
}

// ============================================================================
// GlassSliderControl — Animated Dark Liquid Glass Slider with Precision Up/Down Stepper
// ============================================================================
GlassSliderControl::GlassSliderControl(const QString& title, int minVal, int maxVal, int initVal,
                                       const QString& suffix, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
    , m_suffix(suffix)
    , m_min(minVal)
    , m_max(maxVal)
    , m_value(std::clamp(initVal, minVal, maxVal))
{
    setFixedHeight(46);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    setToolTip("Click ▲/▼ for ±1  ·  Ctrl + Click ▲/▼ (or Ctrl + Up/Down) for ±10");

    m_hoverAnim = new QVariantAnimation(this);
    m_hoverAnim->setDuration(150);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_hoverFactor = v.toReal();
        update();
    });

    m_repeatDelayTimer = new QTimer(this);
    m_repeatDelayTimer->setSingleShot(true);
    m_repeatTickTimer = new QTimer(this);
    m_repeatTickTimer->setInterval(52);
    connect(m_repeatDelayTimer, &QTimer::timeout, this, [this]() {
        if (m_repeatDir != 0) {
            const int mult = (QGuiApplication::keyboardModifiers() & Qt::ControlModifier) ? 10 : 1;
            stepBy(m_repeatDir * mult);
            m_repeatTickTimer->start();
        }
    });
    connect(m_repeatTickTimer, &QTimer::timeout, this, [this]() {
        if (m_repeatDir != 0) {
            const int mult = (QGuiApplication::keyboardModifiers() & Qt::ControlModifier) ? 10 : 1;
            stepBy(m_repeatDir * mult);
        }
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

void GlassSliderControl::setValue(int val, bool emitSignal) {
    int clamped = std::clamp(val, m_min, m_max);
    if (clamped != m_value) {
        m_value = clamped;
        update();
        if (emitSignal) emit valueChanged(m_value);
    }
}

void GlassSliderControl::stepBy(int delta) {
    setValue(m_value + delta, true);
}

QRectF GlassSliderControl::trackRect() const {
    return QRectF(11.0, height() - 12.0, width() - 22.0, 6.0);
}

QRectF GlassSliderControl::valueBoxRect() const {
    const qreal boxW = 78.0;
    return QRectF(width() - boxW - 2.0, 0.5, boxW, 24.0);
}

QRectF GlassSliderControl::stepUpRect() const {
    QRectF box = valueBoxRect();
    return QRectF(box.right() - 22.0, box.top(), 22.0, box.height() * 0.5);
}

QRectF GlassSliderControl::stepDownRect() const {
    QRectF box = valueBoxRect();
    return QRectF(box.right() - 22.0, box.top() + box.height() * 0.5, 22.0, box.height() * 0.5);
}

void GlassSliderControl::paintEvent(QPaintEvent*) {
    const qreal dpr = devicePixelRatioF();
    quint64 key = 0x303ULL;
    key = mixHash(key, static_cast<quint64>(width()));
    key = mixHash(key, static_cast<quint64>(height()));
    key = mixHash(key, static_cast<quint64>(std::lround(dpr * 100.0)));
    key = mixHash(key, static_cast<quint64>(ThemeManager::instance().currentIndex()));
    key = mixHash(key, static_cast<quint64>(m_value - m_min));
    key = mixHash(key, static_cast<quint64>(m_dragging ? 1 : 0));
    key = mixHash(key, static_cast<quint64>(m_hoverPart));
    key = mixHash(key, static_cast<quint64>(m_pressedPart));
    key = mixHash(key, static_cast<quint64>(std::lround(m_hoverFactor * 100.0)));
    key = mixHash(key, static_cast<quint64>(isEnabled() ? 1 : 0));

    if (key == m_cacheKey && !m_cachePixmap.isNull()) {
        QPainter screenP(this);
        screenP.drawPixmap(0, 0, m_cachePixmap);
        return;
    }

    QPixmap pm = makeHiDpiPixmap(width(), height(), dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const GlassTheme& th = ThemeManager::instance().current();
    QRectF box = valueBoxRect();
    QRectF upR = stepUpRect();
    QRectF dnR = stepDownRect();
    QRectF textR(box.left() + 2.0, box.top(), box.width() - 24.0, box.height());

    // Title label
    QFont titleFont = font();
    titleFont.setPixelSize(12);
    titleFont.setWeight(QFont::DemiBold);
    p.setFont(titleFont);
    p.setPen(QColor(238, 240, 245));
    p.drawText(QRectF(2.0, 1.0, box.left() - 6.0, 23.0), Qt::AlignVCenter | Qt::AlignLeft, m_title);

    // Liquid Glass Value + Up/Down Stepper Capsule
    QPainterPath boxPath;
    boxPath.addRoundedRect(box, 7.0, 7.0);
    p.save();
    p.setClipPath(boxPath);

    p.fillRect(box, QColor(18, 20, 24, 175));

    // Up / Down stepper button backgrounds
    QColor upFill = (m_pressedPart == PartStepUp) ? QColor(255, 255, 255, 68)
                  : (m_hoverPart == PartStepUp ? QColor(255, 255, 255, 42) : QColor(255, 255, 255, 18));
    QColor dnFill = (m_pressedPart == PartStepDown) ? QColor(255, 255, 255, 68)
                  : (m_hoverPart == PartStepDown ? QColor(255, 255, 255, 42) : QColor(255, 255, 255, 18));
    p.fillRect(upR, upFill);
    p.fillRect(dnR, dnFill);

    // Hairline glass dividers separating value from stepper and Up from Down
    p.setPen(QPen(QColor(255, 255, 255, 38), 1.0));
    p.drawLine(QPointF(upR.left(), box.top()), QPointF(upR.left(), box.bottom()));
    p.drawLine(QPointF(upR.left(), upR.bottom()), QPointF(box.right(), upR.bottom()));

    // Crisp vector Up (▲) and Down (▼) stepper arrows
    auto drawTri = [&](const QRectF& rr, bool up, bool active) {
        const qreal cx = rr.center().x();
        const qreal cy = rr.center().y() + (up ? 0.4 : -0.4);
        QPainterPath tri;
        if (up) {
            tri.moveTo(cx, cy - 2.6);
            tri.lineTo(cx + 3.6, cy + 2.2);
            tri.lineTo(cx - 3.6, cy + 2.2);
        } else {
            tri.moveTo(cx, cy + 2.6);
            tri.lineTo(cx + 3.6, cy - 2.2);
            tri.lineTo(cx - 3.6, cy - 2.2);
        }
        tri.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(active ? Qt::white : QColor(210, 214, 222));
        p.drawPath(tri);
    };
    drawTri(upR, true, m_hoverPart == PartStepUp || m_pressedPart == PartStepUp);
    drawTri(dnR, false, m_hoverPart == PartStepDown || m_pressedPart == PartStepDown);

    // Numeric value text
    QString valText = (m_value == 0 && !m_zeroLabel.isEmpty())
                        ? m_zeroLabel
                        : (QString::number(m_value) + m_suffix);
    QFont valFont = font();
    valFont.setPixelSize(11);
    valFont.setWeight(QFont::Bold);
    p.setFont(valFont);
    p.setPen(Qt::white);
    p.drawText(textR, Qt::AlignCenter, valText);
    p.restore();

    // Outer glass rim on stepper box
    p.setPen(QPen(QColor(255, 255, 255, m_dragging ? 128 : static_cast<int>(58 + 38 * m_hoverFactor)), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(box, 7.0, 7.0);

    // Slider Track
    QRectF tr = trackRect();
    p.setPen(QPen(QColor(255, 255, 255, 42), 0.9));
    p.setBrush(QColor(255, 255, 255, 26));
    p.drawRoundedRect(tr, 3.0, 3.0);

    double ratio = (m_max > m_min) ? static_cast<double>(m_value - m_min) / (m_max - m_min) : 0.0;
    qreal fillW = tr.width() * ratio;
    QColor g0 = m_useToggleAccent ? th.toggleActive : th.accentStart;
    QColor g1 = m_useToggleAccent ? th.toggleActive.lighter(130) : th.accentEnd;

    if (fillW > 1.0) {
        QRectF fillR(tr.left(), tr.top(), fillW, tr.height());
        QLinearGradient fg(tr.topLeft(), tr.topRight());
        fg.setColorAt(0.0, g0);
        fg.setColorAt(1.0, g1);
        p.setPen(Qt::NoPen);
        p.setBrush(fg);
        p.drawRoundedRect(fillR, 3.0, 3.0);
    }

    qreal knobX = tr.left() + fillW;
    qreal knobY = tr.center().y();
    qreal activeF = m_dragging ? 1.0 : m_hoverFactor;
    qreal knobR = 6.5 + 1.1 * activeF;

    if (activeF > 0.02) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(g1.red(), g1.green(), g1.blue(), static_cast<int>(76 * activeF)));
        p.drawEllipse(QPointF(knobX, knobY), knobR + 4.0 * activeF, knobR + 4.0 * activeF);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 95));
    p.drawEllipse(QPointF(knobX, knobY + 1.2), knobR, knobR);

    p.setBrush(Qt::white);
    p.setPen(QPen(QColor(255, 255, 255, 210), 0.9));
    p.drawEllipse(QPointF(knobX, knobY), knobR, knobR);

    p.end();
    m_cachePixmap = std::move(pm);
    m_cacheKey = key;
    QPainter screenP(this);
    screenP.drawPixmap(0, 0, m_cachePixmap);
}

void GlassSliderControl::updateValueFromX(qreal x) {
    QRectF tr = trackRect();
    double ratio = std::clamp((x - tr.left()) / std::max(1.0, tr.width()), 0.0, 1.0);
    int newVal = m_min + static_cast<int>(std::lround(ratio * (m_max - m_min)));
    setValue(newVal, true);
}

void GlassSliderControl::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->position();
        const int stepMult = (event->modifiers() & Qt::ControlModifier) ? 10 : 1;
        if (stepUpRect().contains(pos)) {
            m_pressedPart = PartStepUp;
            m_repeatDir = +1;
            stepBy(+1 * stepMult);
            m_repeatDelayTimer->start(320);
            update();
            event->accept();
            return;
        }
        if (stepDownRect().contains(pos)) {
            m_pressedPart = PartStepDown;
            m_repeatDir = -1;
            stepBy(-1 * stepMult);
            m_repeatDelayTimer->start(320);
            update();
            event->accept();
            return;
        }
        if (pos.y() >= height() - 22.0) {
            m_dragging = true;
            m_pressedPart = PartTrack;
            updateValueFromX(pos.x());
            event->accept();
            return;
        }
    }
}

void GlassSliderControl::mouseMoveEvent(QMouseEvent* event) {
    QPointF pos = event->position();
    if (m_dragging) {
        updateValueFromX(pos.x());
        event->accept();
        return;
    }

    HoverPart nextPart = PartNone;
    if (stepUpRect().contains(pos)) nextPart = PartStepUp;
    else if (stepDownRect().contains(pos)) nextPart = PartStepDown;
    else if (pos.y() >= height() - 22.0) nextPart = PartTrack;

    if (nextPart != m_hoverPart) {
        m_hoverPart = nextPart;
        update();
    }
}

void GlassSliderControl::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_repeatDelayTimer->stop();
    m_repeatTickTimer->stop();
    m_repeatDir = 0;
    if (m_dragging || m_pressedPart != PartNone) {
        m_dragging = false;
        m_pressedPart = PartNone;
        update();
    }
}

void GlassSliderControl::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        stepBy(event->angleDelta().y() > 0 ? 10 : -10);
        event->accept();
        return;
    }
    bool fineZone = valueBoxRect().contains(event->position());
    int step = fineZone ? 1 : ((m_max - m_min) > 50 ? 5 : 1);
    stepBy(event->angleDelta().y() > 0 ? step : -step);
    event->accept();
}

void GlassSliderControl::enterEvent(QEnterEvent*) {
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hoverFactor);
    m_hoverAnim->setEndValue(1.0);
    m_hoverAnim->start();
}

void GlassSliderControl::leaveEvent(QEvent*) {
    m_hoverPart = PartNone;
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hoverFactor);
    m_hoverAnim->setEndValue(0.0);
    m_hoverAnim->start();
}

// ============================================================================
// GlassActionRow — Animated Nested Grayish Liquid Glass Row
// ============================================================================
GlassActionRow::GlassActionRow(const QString& iconName, const QString& title,
                               const QString& subtitle, bool useThemeAccent,
                               const QColor& customAccent, QWidget* parent)
    : QPushButton(parent)
    , m_iconName(iconName)
    , m_title(title)
    , m_subtitle(subtitle)
    , m_useThemeAccent(useThemeAccent)
    , m_customAccent(customAccent)
{
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setFixedHeight(46);
    setFlat(true);

    m_hoverAnim = new QVariantAnimation(this);
    m_hoverAnim->setDuration(170);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_hoverFactor = v.toReal();
        update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

void GlassActionRow::enterEvent(QEnterEvent* event) {
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hoverFactor);
    m_hoverAnim->setEndValue(1.0);
    m_hoverAnim->start();
    QPushButton::enterEvent(event);
}

void GlassActionRow::leaveEvent(QEvent* event) {
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hoverFactor);
    m_hoverAnim->setEndValue(0.0);
    m_hoverAnim->start();
    QPushButton::leaveEvent(event);
}

void GlassActionRow::paintEvent(QPaintEvent*) {
    const qreal dpr = devicePixelRatioF();
    const bool pressed = isDown();
    const qreal hf = m_hoverFactor;

    quint64 key = 0x404ULL;
    key = mixHash(key, static_cast<quint64>(width()));
    key = mixHash(key, static_cast<quint64>(height()));
    key = mixHash(key, static_cast<quint64>(std::lround(dpr * 100.0)));
    key = mixHash(key, static_cast<quint64>(ThemeManager::instance().currentIndex()));
    key = mixHash(key, static_cast<quint64>(pressed ? 1 : 0));
    key = mixHash(key, static_cast<quint64>(std::lround(hf * 100.0)));

    if (key == m_cacheKey && !m_cachePixmap.isNull()) {
        QPainter screenP(this);
        screenP.drawPixmap(0, 0, m_cachePixmap);
        return;
    }

    QPixmap pm = makeHiDpiPixmap(width(), height(), dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const GlassTheme& th = ThemeManager::instance().current();
    QColor accent = m_useThemeAccent ? th.accentEnd : m_customAccent;

    QRectF r = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);

    QLinearGradient bg(r.topLeft(), r.bottomLeft());
    bg.setColorAt(0.0, QColor(235, 240, 250, pressed ? 52 : static_cast<int>(26 + 22 * hf)));
    bg.setColorAt(1.0, QColor(165, 175, 192, pressed ? 34 : static_cast<int>(14 + 16 * hf)));
    p.setBrush(bg);

    QLinearGradient rim(r.topLeft(), r.bottomRight());
    rim.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(78 + 64 * hf)));
    rim.setColorAt(1.0, QColor(accent.red(), accent.green(), accent.blue(), static_cast<int>(32 + 78 * hf)));
    p.setPen(QPen(QBrush(rim), 1.05));
    p.drawRoundedRect(r, 15.0, 15.0);

    // Circular liquid-glass icon medallion
    QRectF medRect(r.left() + 8.0, r.center().y() - 15.0, 30.0, 30.0);
    QRadialGradient medBg(medRect.center(), 17.0);
    medBg.setColorAt(0.0, QColor(accent.red(), accent.green(), accent.blue(), static_cast<int>(54 + 34 * hf)));
    medBg.setColorAt(1.0, QColor(255, 255, 255, 20));
    p.setBrush(medBg);
    p.setPen(QPen(QColor(255, 255, 255, static_cast<int>(92 + 54 * hf)), 1.0));
    p.drawEllipse(medRect);
    GlassIcons::paintIcon(p, m_iconName, medRect.adjusted(6.5, 6.5, -6.5, -6.5), accent, 1.7);

    qreal tx = medRect.right() + 10.0;
    qreal tw = r.right() - tx - 24.0;
    QFont tf = font();
    tf.setPixelSize(12);
    tf.setWeight(QFont::DemiBold);
    p.setFont(tf);
    p.setPen(QColor(248, 250, 255));
    p.drawText(QRectF(tx, r.top() + 6.0, tw, 17.0), Qt::AlignVCenter | Qt::AlignLeft, m_title);

    QFont sf = font();
    sf.setPixelSize(10);
    sf.setWeight(QFont::Normal);
    p.setFont(sf);
    p.setPen(QColor(184, 194, 210));
    p.drawText(QRectF(tx, r.top() + 23.0, tw, 16.0), Qt::AlignVCenter | Qt::AlignLeft, m_subtitle);

    QRectF chevRect(r.right() - 22.0 + 2.0 * hf, r.center().y() - 7.0, 14.0, 14.0);
    GlassIcons::paintIcon(p, "chevron_right", chevRect,
                          QColor(255, 255, 255, static_cast<int>(130 + 115 * hf)), 1.65);

    p.end();
    m_cachePixmap = std::move(pm);
    m_cacheKey = key;
    QPainter screenP(this);
    screenP.drawPixmap(0, 0, m_cachePixmap);
}

// ============================================================================
// GlassSwatchButton — Tactile Surface Swatches
// ============================================================================
GlassSwatchButton::GlassSwatchButton(int bgType, const QString& label, const QString& tip, QWidget* parent)
    : QPushButton(parent)
    , m_bgType(bgType)
    , m_label(label)
{
    setCheckable(true);
    setFixedSize(54, 36);
    setToolTip(tip);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

void GlassSwatchButton::paintEvent(QPaintEvent*) {
    const qreal dpr = devicePixelRatioF();
    quint64 key = 0x505ULL;
    key = mixHash(key, static_cast<quint64>(width()));
    key = mixHash(key, static_cast<quint64>(height()));
    key = mixHash(key, static_cast<quint64>(std::lround(dpr * 100.0)));
    key = mixHash(key, static_cast<quint64>(ThemeManager::instance().currentIndex()));
    key = mixHash(key, static_cast<quint64>((isChecked() ? 1 : 0) | (underMouse() ? 2 : 0)));

    if (key == m_cacheKey && !m_cachePixmap.isNull()) {
        QPainter screenP(this);
        screenP.drawPixmap(0, 0, m_cachePixmap);
        return;
    }

    QPixmap pm = makeHiDpiPixmap(width(), height(), dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const GlassTheme& th = ThemeManager::instance().current();
    QRectF r = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);
    QPainterPath clipPath;
    clipPath.addRoundedRect(r, 10.0, 10.0);

    p.save();
    p.setClipPath(clipPath);
    if (m_bgType == 0) { // Dark neutral gray checkerboard
        QColor c1(54, 54, 58);
        QColor c2(42, 42, 46);
        qreal cw = r.width() / 4.0;
        qreal ch = r.height() / 2.0;
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 4; ++x) {
                p.fillRect(QRectF(r.left() + x * cw, r.top() + y * ch, cw + 0.5, ch + 0.5),
                           ((x + y) % 2 == 0) ? c1 : c2);
            }
        }
    } else if (m_bgType == 1) { // Complete Pure White
        p.fillRect(r, QColor(250, 250, 250));
    } else if (m_bgType == 2) { // Complete Pitch Black
        p.fillRect(r, QColor(4, 4, 5));
    } else { // Infrared Dust Detector Swatch
        p.fillRect(r, QColor(10, 8, 12));
        QRadialGradient irGlow(QPointF(r.center().x(), r.top() + 8.0), 18.0);
        irGlow.setColorAt(0.0, QColor(255, 28, 68, 175));
        irGlow.setColorAt(0.55, QColor(255, 108, 0, 80));
        irGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(r, irGlow);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 42, 76));
        p.drawEllipse(QPointF(r.left() + 12.0, r.top() + 8.0), 2.3, 2.3);
        p.setBrush(QColor(255, 160, 24));
        p.drawEllipse(QPointF(r.center().x() + 2.0, r.top() + 11.0), 2.8, 2.8);
        p.setBrush(QColor(255, 36, 72));
        p.drawEllipse(QPointF(r.right() - 10.0, r.top() + 7.0), 1.8, 1.8);
    }

    QRectF lblBar(r.left(), r.bottom() - 15.0, r.width(), 15.0);
    p.fillRect(lblBar, QColor(10, 10, 12, 205));
    QFont f = font();
    f.setPixelSize(9);
    f.setWeight(QFont::Bold);
    p.setFont(f);
    p.setPen(m_bgType == 3 ? QColor(255, 146, 160) : QColor(240, 242, 246));
    p.drawText(lblBar, Qt::AlignCenter, m_label);
    p.restore();

    p.setBrush(Qt::NoBrush);
    if (isChecked()) {
        p.setPen(QPen(m_bgType == 3 ? QColor(255, 58, 92) : QColor(235, 238, 245), 2.0));
        p.drawRoundedRect(r, 10.0, 10.0);
    } else {
        p.setPen(QPen(underMouse() ? QColor(255, 255, 255, 130) : QColor(255, 255, 255, 58), 1.0));
        p.drawRoundedRect(r, 10.0, 10.0);
    }

    p.end();
    m_cachePixmap = std::move(pm);
    m_cacheKey = key;
    QPainter screenP(this);
    screenP.drawPixmap(0, 0, m_cachePixmap);
}

// ============================================================================
// GlassThemePicker — 1-Click Color Theme Medallion Swatches
// ============================================================================
GlassThemePicker::GlassThemePicker(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(32);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

QRectF GlassThemePicker::dotRect(int idx) const {
    const int n = static_cast<int>(ThemeManager::instance().themes().size());
    const qreal d = 22.0;
    const qreal spacing = (n > 1) ? (width() - 8.0 - n * d) / (n - 1) : 0.0;
    const qreal x = 4.0 + idx * (d + spacing);
    const qreal y = (height() - d) * 0.5;
    return QRectF(x, y, d, d);
}

void GlassThemePicker::paintEvent(QPaintEvent*) {
    const qreal dpr = devicePixelRatioF();
    const int activeIdx = ThemeManager::instance().currentIndex();
    quint64 key = 0x606ULL;
    key = mixHash(key, static_cast<quint64>(width()));
    key = mixHash(key, static_cast<quint64>(height()));
    key = mixHash(key, static_cast<quint64>(std::lround(dpr * 100.0)));
    key = mixHash(key, static_cast<quint64>(activeIdx));
    key = mixHash(key, static_cast<quint64>(m_hovered + 16));

    if (key == m_cacheKey && !m_cachePixmap.isNull()) {
        QPainter screenP(this);
        screenP.drawPixmap(0, 0, m_cachePixmap);
        return;
    }

    QPixmap pm = makeHiDpiPixmap(width(), height(), dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& themes = ThemeManager::instance().themes();

    for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
        QRectF dr = dotRect(i);
        bool active = (i == activeIdx);
        bool hovered = (i == m_hovered);

        if (active || hovered) {
            p.setPen(QPen(active ? Qt::white : QColor(255, 255, 255, 120), active ? 1.8 : 1.1));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(dr.adjusted(-2.5, -2.5, 2.5, 2.5));
        }

        // Two-tone dark smoked glass orb (sleek all-dark metallic palette)
        QLinearGradient orb(dr.topLeft(), dr.bottomRight());
        orb.setColorAt(0.0, themes[i].swatchColor);
        orb.setColorAt(1.0, themes[i].stageBgBottom);
        p.setPen(QPen(QColor(255, 255, 255, 115), 1.0));
        p.setBrush(orb);
        p.drawEllipse(dr);

        // Subtle glass specular catch-light
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 86));
        p.drawEllipse(QRectF(dr.left() + 4.0, dr.top() + 3.5, 6.0, 3.6));
    }

    p.end();
    m_cachePixmap = std::move(pm);
    m_cacheKey = key;
    QPainter screenP(this);
    screenP.drawPixmap(0, 0, m_cachePixmap);
}

void GlassThemePicker::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const int n = static_cast<int>(ThemeManager::instance().themes().size());
        for (int i = 0; i < n; ++i) {
            if (dotRect(i).adjusted(-4, -4, 4, 4).contains(event->position())) {
                ThemeManager::instance().setThemeIndex(i);
                event->accept();
                return;
            }
        }
    }
}

void GlassThemePicker::mouseMoveEvent(QMouseEvent* event) {
    const auto& themes = ThemeManager::instance().themes();
    int found = -1;
    for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
        if (dotRect(i).adjusted(-4, -4, 4, 4).contains(event->position())) {
            found = i;
            break;
        }
    }
    if (found != m_hovered) {
        m_hovered = found;
        setToolTip(found >= 0 ? QString("Theme: %1").arg(themes[found].name) : QString());
        update();
    }
}

void GlassThemePicker::leaveEvent(QEvent*) {
    m_hovered = -1;
    update();
}

// ============================================================================
// GlassModalDialog — Frameless Dark Smoked Liquid Glass Modal Base
// ============================================================================
GlassModalDialog::GlassModalDialog(const QString& title, QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    // Custom Liquid Glass Draggable Modal Header Bar (Zero Windows default titlebar!)
    QWidget* hdr = new QWidget(this);
    hdr->setFixedHeight(44);
    hdr->setStyleSheet("background: transparent;");
    QHBoxLayout* hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(18, 8, 12, 4);
    hdrLay->setSpacing(10);

    m_headerTitle = new QLabel(title);
    m_headerTitle->setStyleSheet("color: #f0f2f6; font-size: 13px; font-weight: 700; background: transparent;");

    GlassButton* closeBtn = new GlassButton("", GlassButton::IconCircle);
    closeBtn->setIconName("win_close");
    closeBtn->setFixedSize(28, 28);
    closeBtn->setToolTip("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    hdrLay->addWidget(m_headerTitle);
    hdrLay->addStretch();
    hdrLay->addWidget(closeBtn);
    outer->addWidget(hdr);

    QFrame* sep = new QFrame();
    sep->setFixedHeight(1);
    sep->setStyleSheet("background-color: rgba(255, 255, 255, 24); border: none;");
    outer->addWidget(sep);

    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(18, 16, 18, 18);
    m_contentLayout->setSpacing(14);
    outer->addLayout(m_contentLayout, 1);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

void GlassModalDialog::setModalTitle(const QString& title) {
    if (m_headerTitle) m_headerTitle->setText(title);
    setWindowTitle(title);
}

void GlassModalDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const GlassTheme& th = ThemeManager::instance().current();
    QRectF r = QRectF(rect()).adjusted(10.0, 10.0, -10.0, -10.0);
    const qreal rad = 22.0;

    // Multi-step soft drop shadow
    for (int i = 4; i >= 1; --i) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 26 + (5 - i) * 14));
        p.drawRoundedRect(r.adjusted(-i * 1.5, -i * 0.6 + 2.0, i * 1.5, i * 2.2), rad + i, rad + i);
    }

    QPainterPath clipPath;
    clipPath.addRoundedRect(r, rad, rad);
    p.save();
    p.setClipPath(clipPath);

    // Deep smoked glass base
    QLinearGradient bg(r.topLeft(), r.bottomLeft());
    bg.setColorAt(0.0, QColor(th.glassBaseTop.red(), th.glassBaseTop.green(), th.glassBaseTop.blue(), 244));
    bg.setColorAt(0.45, QColor(th.stageBgMid.red() + 14, th.stageBgMid.green() + 14, th.stageBgMid.blue() + 15, 246));
    bg.setColorAt(1.0, QColor(th.stageBgBottom.red() + 10, th.stageBgBottom.green() + 10, th.stageBgBottom.blue() + 12, 248));
    p.fillRect(r, bg);

    // Top liquid meniscus gloss sheen
    QLinearGradient sheen(r.topLeft(), QPointF(r.left(), r.top() + 56.0));
    sheen.setColorAt(0.0, QColor(255, 255, 255, 42));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(r, sheen);
    p.restore();

    // Double optical refraction rim
    p.setPen(QPen(QColor(255, 255, 255, 24), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(1.0, 1.0, -1.0, -1.0), rad - 1.0, rad - 1.0);

    QLinearGradient rim(r.topLeft(), r.bottomRight());
    rim.setColorAt(0.0, th.glassRimTop);
    rim.setColorAt(0.35, QColor(255, 255, 255, 58));
    rim.setColorAt(1.0, th.glassRimBottom);
    p.setPen(QPen(QBrush(rim), 1.25));
    p.drawRoundedRect(r, rad, rad);
}

void GlassModalDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->position().y() <= 58.0) {
        m_draggingWindow = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void GlassModalDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_draggingWindow) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void GlassModalDialog::mouseReleaseEvent(QMouseEvent* event) {
    m_draggingWindow = false;
    QDialog::mouseReleaseEvent(event);
}

void GlassModalDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    adjustSize();

    QRect anchorRect;
    if (QWidget* p = parentWidget()) {
        QWidget* top = p->window();
        anchorRect = top ? top->frameGeometry() : p->frameGeometry();
    }
    if (!anchorRect.isValid() || anchorRect.isEmpty()) {
        if (QScreen* scr = screen() ? screen() : QGuiApplication::primaryScreen()) {
            anchorRect = scr->availableGeometry();
        }
    }
    if (anchorRect.isValid()) {
        QPoint pos = anchorRect.center() - QPoint(width() / 2, height() / 2);
        if (QScreen* scr = QGuiApplication::screenAt(anchorRect.center())) {
            const QRect avail = scr->availableGeometry();
            pos.setX(std::clamp(pos.x(), avail.left(), std::max(avail.left(), avail.right() - width() + 1)));
            pos.setY(std::clamp(pos.y(), avail.top(), std::max(avail.top(), avail.bottom() - height() + 1)));
        }
        move(pos);
    }
}

// ============================================================================
// GlassDialog — Centralized Dark Liquid Glass Alert / Confirm System
// ============================================================================
GlassDialog::GlassDialog(Kind kind, const QString& title, const QString& text,
                         const QString& detail, QWidget* parent)
    : GlassModalDialog(title, parent)
{
    setMinimumWidth(420);
    setMaximumWidth(540);

    QHBoxLayout* bodyRow = new QHBoxLayout();
    bodyRow->setSpacing(14);

    // Custom Liquid Glass Vector Icon Badge
    QString iconName = "alert_info";
    QColor iconColor(220, 224, 232);
    QColor ringColor(255, 255, 255, 42);
    switch (kind) {
        case Warning:
            iconName = "alert_warn";
            iconColor = QColor(246, 196, 68);
            ringColor = QColor(246, 196, 68, 72);
            break;
        case Error:
            iconName = "alert_error";
            iconColor = QColor(244, 86, 102);
            ringColor = QColor(244, 86, 102, 76);
            break;
        case Success:
            iconName = "alert_check";
            iconColor = QColor(72, 214, 128);
            ringColor = QColor(72, 214, 128, 72);
            break;
        case Question:
            iconName = "alert_question";
            iconColor = QColor(225, 230, 240);
            ringColor = QColor(255, 255, 255, 64);
            break;
        case Info:
        default:
            break;
    }

    QLabel* iconBadge = new QLabel();
    iconBadge->setFixedSize(44, 44);
    QPixmap pm = makeHiDpiPixmap(44, 44, 2.0);
    {
        QPainter ip(&pm);
        ip.setRenderHint(QPainter::Antialiasing, true);
        ip.setBrush(QColor(255, 255, 255, 16));
        ip.setPen(QPen(ringColor, 1.3));
        ip.drawEllipse(QRectF(1.5, 1.5, 41.0, 41.0));
        GlassIcons::paintIcon(ip, iconName, QRectF(10.0, 10.0, 24.0, 24.0), iconColor, 1.9);
    }
    iconBadge->setPixmap(pm);
    iconBadge->setAlignment(Qt::AlignCenter);

    QVBoxLayout* txtCol = new QVBoxLayout();
    txtCol->setSpacing(6);

    QLabel* mainLbl = new QLabel(text);
    mainLbl->setWordWrap(true);
    mainLbl->setTextFormat(Qt::AutoText);
    mainLbl->setStyleSheet("color: #f4f6fa; font-size: 13px; font-weight: 600; background: transparent;");
    txtCol->addWidget(mainLbl);

    if (!detail.isEmpty()) {
        QLabel* detLbl = new QLabel(detail);
        detLbl->setWordWrap(true);
        detLbl->setStyleSheet("color: #a6acb8; font-size: 12px; font-weight: 400; background: transparent;");
        txtCol->addWidget(detLbl);
    }

    bodyRow->addWidget(iconBadge, 0, Qt::AlignVCenter);
    bodyRow->addLayout(txtCol, 1);
    contentLayout()->addLayout(bodyRow);
}

void GlassDialog::info(QWidget* parent, const QString& title, const QString& text, const QString& detail) {
    GlassDialog dlg(Info, title, text, detail, parent);
    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    GlassButton* ok = new GlassButton("OK", GlassButton::White);
    ok->setMinimumSize(96, 36);
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    row->addWidget(ok);
    dlg.contentLayout()->addLayout(row);
    dlg.adjustSize();
    dlg.exec();
}

void GlassDialog::warning(QWidget* parent, const QString& title, const QString& text, const QString& detail) {
    GlassDialog dlg(Warning, title, text, detail, parent);
    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    GlassButton* ok = new GlassButton("Got It", GlassButton::White);
    ok->setMinimumSize(102, 36);
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    row->addWidget(ok);
    dlg.contentLayout()->addLayout(row);
    dlg.adjustSize();
    dlg.exec();
}

void GlassDialog::error(QWidget* parent, const QString& title, const QString& text, const QString& detail) {
    GlassDialog dlg(Error, title, text, detail, parent);
    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    GlassButton* ok = new GlassButton("Close", GlassButton::Danger);
    ok->setMinimumSize(102, 36);
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    row->addWidget(ok);
    dlg.contentLayout()->addLayout(row);
    dlg.adjustSize();
    dlg.exec();
}

void GlassDialog::success(QWidget* parent, const QString& title, const QString& text, const QString& detail) {
    GlassDialog dlg(Success, title, text, detail, parent);
    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    GlassButton* ok = new GlassButton("Done", GlassButton::White);
    ok->setMinimumSize(96, 36);
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    row->addWidget(ok);
    dlg.contentLayout()->addLayout(row);
    dlg.adjustSize();
    dlg.exec();
}

bool GlassDialog::confirm(QWidget* parent, const QString& title, const QString& text,
                          const QString& detail, const QString& confirmText,
                          const QString& cancelText, bool destructive) {
    GlassDialog dlg(destructive ? Warning : Question, title, text, detail, parent);
    QHBoxLayout* row = new QHBoxLayout();
    row->setSpacing(10);
    row->addStretch();

    GlassButton* cancelBtn = new GlassButton(cancelText, GlassButton::Ghost);
    cancelBtn->setMinimumSize(96, 36);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    GlassButton* yesBtn = new GlassButton(confirmText, destructive ? GlassButton::Danger : GlassButton::White);
    yesBtn->setMinimumSize(108, 36);
    yesBtn->setDefault(true);
    QObject::connect(yesBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    row->addWidget(cancelBtn);
    row->addWidget(yesBtn);
    dlg.contentLayout()->addLayout(row);
    dlg.adjustSize();
    return dlg.exec() == QDialog::Accepted;
}

int GlassDialog::confirmThreeWay(QWidget* parent, const QString& title, const QString& text,
                                 const QString& detail, const QString& yesText,
                                 const QString& noText, const QString& cancelText) {
    GlassDialog dlg(Warning, title, text, detail, parent);
    QHBoxLayout* row = new QHBoxLayout();
    row->setSpacing(8);
    row->addStretch();

    GlassButton* cancelBtn = new GlassButton(cancelText, GlassButton::Ghost);
    cancelBtn->setMinimumSize(88, 36);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, [&dlg]() { dlg.m_choice = 0; dlg.reject(); });

    GlassButton* noBtn = new GlassButton(noText, GlassButton::Danger);
    noBtn->setMinimumSize(102, 36);
    QObject::connect(noBtn, &QPushButton::clicked, &dlg, [&dlg]() { dlg.m_choice = 2; dlg.accept(); });

    GlassButton* yesBtn = new GlassButton(yesText, GlassButton::White);
    yesBtn->setMinimumSize(108, 36);
    yesBtn->setDefault(true);
    QObject::connect(yesBtn, &QPushButton::clicked, &dlg, [&dlg]() { dlg.m_choice = 1; dlg.accept(); });

    row->addWidget(cancelBtn);
    row->addWidget(noBtn);
    row->addWidget(yesBtn);
    dlg.contentLayout()->addLayout(row);
    dlg.adjustSize();
    dlg.exec();
    return dlg.m_choice;
}

void GlassDialog::showShortcuts(QWidget* parent, const QString& title, const QString& bodyText) {
    GlassModalDialog dlg(title, parent);
    dlg.setMinimumWidth(450);

    GlassPanel* box = new GlassPanel();
    box->setCornerRadius(14.0);
    box->setSubSurface(true);
    box->setElevated(false);
    QVBoxLayout* bl = new QVBoxLayout(box);
    bl->setContentsMargins(16, 14, 16, 14);

    QLabel* lbl = new QLabel(bodyText);
    lbl->setStyleSheet(
        "color: #dfe2e8; font-family: 'Cascadia Mono', 'Consolas', monospace; "
        "font-size: 11px; background: transparent;"
    );
    bl->addWidget(lbl);
    dlg.contentLayout()->addWidget(box);

    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    GlassButton* ok = new GlassButton("Close", GlassButton::White);
    ok->setMinimumSize(96, 36);
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    row->addWidget(ok);
    dlg.contentLayout()->addLayout(row);
    dlg.exec();
}
