/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "hovertooltip.h"

#include "settings.h"

#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QStringList>
#include <QTextDocument>
#include <QWidget>

using namespace Qt::StringLiterals;

bool TooltipConfig::isEnabled(TooltipCategory category) const
{
    switch (category) {
    case TooltipCategory::ItemInfo:
        return itemInfo;
    case TooltipCategory::ItemActions:
        return itemActions;
    case TooltipCategory::HeaderButtons:
        return headerButtons;
    case TooltipCategory::PlainTextSwitch:
        return plainTextSwitch;
    }
    return true;
}

TooltipConfig TooltipConfig::fromSettings()
{
    const auto *s = MklipperSettings::self();
    TooltipConfig cfg;
    cfg.itemInfo = s->tooltipItemInfo();
    cfg.itemActions = s->tooltipItemActions();
    cfg.headerButtons = s->tooltipHeaderButtons();
    cfg.plainTextSwitch = s->tooltipPlainTextSwitch();
    cfg.delayMs = qBound(0, s->tooltipDelayMs(), HoverTooltip::kMaxDelayMs);
    cfg.actionBarMode = s->actionBarMode();
    cfg.actionBarAnimation = s->actionBarAnimation();
    return cfg;
}

namespace HoverTooltip
{
int effectiveMaxWidthPx(const QFont &font)
{
    const QFontMetrics fm(font);
    const qreal scale = qBound(1.0, fm.height() / 17.0, 1.75);
    return qRound(kMaxContentWidthPx * scale);
}

QString elideLine(const QString &text, const QFontMetrics &fm, int maxWidthPx)
{
    if (text.isEmpty()) {
        return {};
    }

    // Bound input length first so multi-MB clipboard strings remain O(1)
    constexpr qsizetype kMaxScanChars = 512;
    const bool preTruncated = text.size() > kMaxScanChars;
    QString clean = preTruncated ? text.left(kMaxScanChars) : text;
    clean.replace(QLatin1Char('\r'), QLatin1Char(' '));
    clean.replace(QLatin1Char('\n'), QLatin1Char(' '));
    clean.replace(QLatin1Char('\t'), QLatin1Char(' '));
    clean = clean.trimmed();

    if (clean.isEmpty()) {
        return {};
    }

    const QString dots = u"..."_s;
    if (maxWidthPx <= 0) {
        return preTruncated ? (clean + dots) : clean;
    }

    if (!preTruncated && fm.horizontalAdvance(clean) <= maxWidthPx) {
        return clean;
    }

    if (fm.horizontalAdvance(dots) >= maxWidthPx) {
        return dots;
    }

    qsizetype lo = 0;
    qsizetype hi = clean.size();
    while (lo < hi) {
        const qsizetype mid = lo + (hi - lo + 1) / 2;
        if (fm.horizontalAdvance(clean.left(mid) + dots) <= maxWidthPx) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    QString prefix = clean.left(lo);
    while (!prefix.isEmpty() && prefix.back().isSpace()) {
        prefix.chop(1);
    }
    return prefix.isEmpty() ? dots : (prefix + dots);
}

QString formatSourceTooltip(const QString &app,
                            const QString &title,
                            const QString &url,
                            const QString &relTime,
                            const QFont &font,
                            int maxWidthPx)
{
    if (app.isEmpty() && title.isEmpty() && url.isEmpty() && relTime.isEmpty()) {
        return {};
    }

    const QFontMetrics fm(font);
    const int limitPx = (maxWidthPx > 0) ? maxWidthPx : effectiveMaxWidthPx(font);

    QStringList lines;
    if (!app.isEmpty()) {
        lines.append(elideLine(
            QCoreApplication::translate("HoverTooltip", "Aplicación: %1").arg(app),
            fm,
            limitPx));
    }
    if (!title.isEmpty()) {
        lines.append(elideLine(
            QCoreApplication::translate("HoverTooltip", "Ventana: %1").arg(title),
            fm,
            limitPx));
    }
    if (!url.isEmpty()) {
        lines.append(elideLine(
            QCoreApplication::translate("HoverTooltip", "Enlace: %1").arg(url),
            fm,
            limitPx));
    }
    if (!relTime.isEmpty()) {
        lines.append(elideLine(
            QCoreApplication::translate("HoverTooltip", "Copiado: %1").arg(relTime),
            fm,
            limitPx));
    }
    return lines.join(QLatin1Char('\n'));
}

QString formatWrappedTooltip(const QString &text, const QFont &font, int maxWidthPx)
{
    if (text.isEmpty()) {
        return {};
    }

    const QFontMetrics fm(font);
    const int limitPx = (maxWidthPx > 0) ? maxWidthPx : effectiveMaxWidthPx(font);

    QStringList outLines;
    const QStringList paragraphs = text.split(QLatin1Char('\n'));
    for (const QString &para : paragraphs) {
        const QString trimmed = para.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (fm.horizontalAdvance(trimmed) <= limitPx) {
            outLines.append(trimmed);
            continue;
        }

        const QStringList words = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QString current;
        for (const QString &word : words) {
            if (current.isEmpty()) {
                if (fm.horizontalAdvance(word) <= limitPx) {
                    current = word;
                } else {
                    outLines.append(elideLine(word, fm, limitPx));
                }
            } else {
                const QString candidate = current + QLatin1Char(' ') + word;
                if (fm.horizontalAdvance(candidate) <= limitPx) {
                    current = candidate;
                } else {
                    outLines.append(current);
                    if (fm.horizontalAdvance(word) <= limitPx) {
                        current = word;
                    } else {
                        outLines.append(elideLine(word, fm, limitPx));
                        current.clear();
                    }
                }
            }
        }
        if (!current.isEmpty()) {
            outLines.append(current);
        }
    }
    return outLines.join(QLatin1Char('\n'));
}

QString prepareDisplayText(const QString &plainFormattedText)
{
    if (plainFormattedText.isEmpty()) {
        return {};
    }
    if (Qt::mightBeRichText(plainFormattedText)) {
        QString escaped = plainFormattedText.toHtmlEscaped();
        escaped.replace(QLatin1Char('\n'), u"<br>"_s);
        return u"<span style='white-space:pre;'>"_s + escaped + u"</span>"_s;
    }
    return plainFormattedText;
}
} // namespace HoverTooltip

HoverTooltipController::HoverTooltipController(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &HoverTooltipController::onTimeout);
}

HoverTooltipController::~HoverTooltipController()
{
    cancel();
}

void HoverTooltipController::setConfig(const TooltipConfig &config)
{
    m_config = config;
    m_config.delayMs = qBound(0, m_config.delayMs, HoverTooltip::kMaxDelayMs);

    if (!m_visibleKey.isEmpty() && !m_config.isEnabled(m_visibleCategory)) {
        cancel();
        return;
    }
    if (m_timer.isActive() && !m_config.isEnabled(m_pendingCategory)) {
        cancel();
    }
}

void HoverTooltipController::registerWidget(QWidget *widget,
                                            TooltipCategory category,
                                            std::function<QString()> textProvider,
                                            bool wrapLongText)
{
    if (!widget) {
        return;
    }
    widget->setToolTip(QString());
    widget->setAttribute(Qt::WA_Hover, true);
    widget->installEventFilter(this);

    RegisteredWidget entry;
    entry.category = category;
    entry.textProvider = std::move(textProvider);
    entry.wrapLongText = wrapLongText;
    entry.key = QStringLiteral("w:%1").arg(reinterpret_cast<quintptr>(widget), 0, 16);
    m_registered.insert(widget, std::move(entry));

    connect(widget, &QObject::destroyed, this, [this](QObject *obj) {
        m_registered.remove(obj);
    });
}

void HoverTooltipController::hoverTarget(const QString &key,
                                         TooltipCategory category,
                                         const QString &formattedText,
                                         const QPoint &globalPos,
                                         QWidget *widget,
                                         const QRect &validRect)
{
    if (!widget || formattedText.isEmpty() || !m_config.isEnabled(category)) {
        cancel();
        return;
    }

    if (m_visibleKey == key && m_visibleText == formattedText && m_visibleWidget == widget) {
        return;
    }

    if (m_config.delayMs <= 0) {
        m_timer.stop();
        m_pendingKey.clear();
        m_pendingText.clear();
        m_pendingWidget.clear();
        showNow(key, category, formattedText, globalPos, widget, validRect);
        return;
    }

    if (m_timer.isActive() && m_pendingKey == key && m_pendingText == formattedText && m_pendingWidget == widget) {
        m_pendingGlobalPos = globalPos;
        m_pendingValidRect = validRect;
        return;
    }

    if (!m_visibleKey.isEmpty()) {
        QToolTip::hideText();
        m_visibleKey.clear();
        m_visibleText.clear();
        m_visibleWidget.clear();
    }

    m_pendingKey = key;
    m_pendingCategory = category;
    m_pendingText = formattedText;
    m_pendingGlobalPos = globalPos;
    m_pendingWidget = widget;
    m_pendingValidRect = validRect;
    m_timer.start(m_config.delayMs);
}

void HoverTooltipController::cancel()
{
    m_timer.stop();
    m_pendingKey.clear();
    m_pendingText.clear();
    m_pendingWidget.clear();
    m_pendingValidRect = {};

    if (!m_visibleKey.isEmpty()) {
        QToolTip::hideText();
    }
    m_visibleKey.clear();
    m_visibleText.clear();
    m_visibleWidget.clear();
}

bool HoverTooltipController::eventFilter(QObject *watched, QEvent *event)
{
    const auto it = m_registered.constFind(watched);
    if (it == m_registered.constEnd()) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::ToolTip) {
        // Suppress Qt's built-in fixed-delay tooltip so our delay & toggle config governs it
        return true;
    }

    auto *w = qobject_cast<QWidget *>(watched);
    if (!w) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::Enter:
    case QEvent::HoverEnter:
    case QEvent::MouseMove: {
        if (!w->isEnabled() || !w->isVisible() || !m_config.isEnabled(it->category)) {
            if (m_pendingWidget == w || m_visibleWidget == w) {
                cancel();
            }
            break;
        }
        QString text = it->textProvider ? it->textProvider() : QString();
        if (it->wrapLongText && !text.isEmpty()) {
            text = HoverTooltip::formatWrappedTooltip(text);
        }
        QPoint gPos = QCursor::pos();
        if (event->type() == QEvent::MouseMove) {
            gPos = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
        }
        hoverTarget(it->key, it->category, text, gPos, w, w->rect());
        break;
    }
    case QEvent::Leave:
    case QEvent::HoverLeave:
    case QEvent::MouseButtonPress:
    case QEvent::Hide:
    case QEvent::EnabledChange:
    case QEvent::WindowDeactivate:
        if (m_pendingWidget == w || m_visibleWidget == w) {
            cancel();
        }
        break;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

void HoverTooltipController::onTimeout()
{
    QWidget *w = m_pendingWidget.data();
    if (!w || !w->isVisible() || (w->window() && !w->window()->isVisible())
        || !m_config.isEnabled(m_pendingCategory) || m_pendingText.isEmpty()) {
        cancel();
        return;
    }

    QPoint showPos = m_pendingGlobalPos;
    if (QGuiApplication::platformName() != u"offscreen"_s) {
        const QPoint cursorGlobal = QCursor::pos();
        const QPoint localPos = w->mapFromGlobal(cursorGlobal);
        if (!w->rect().contains(localPos)
            || (m_pendingValidRect.isValid() && !m_pendingValidRect.contains(localPos))) {
            cancel();
            return;
        }
        showPos = cursorGlobal;
    }

    const QString key = m_pendingKey;
    const TooltipCategory cat = m_pendingCategory;
    const QString text = m_pendingText;
    const QRect validRect = m_pendingValidRect;

    m_pendingKey.clear();
    m_pendingText.clear();
    m_pendingWidget.clear();
    m_pendingValidRect = {};

    showNow(key, cat, text, showPos, w, validRect);
}

void HoverTooltipController::showNow(const QString &key,
                                     TooltipCategory category,
                                     const QString &formattedText,
                                     const QPoint &globalPos,
                                     QWidget *widget,
                                     const QRect &validRect)
{
    m_visibleKey = key;
    m_visibleCategory = category;
    m_visibleText = formattedText;
    m_visibleWidget = widget;

    const QString display = HoverTooltip::prepareDisplayText(formattedText);
    if (validRect.isValid()) {
        QToolTip::showText(globalPos, display, widget, validRect);
    } else {
        QToolTip::showText(globalPos, display, widget);
    }
}
