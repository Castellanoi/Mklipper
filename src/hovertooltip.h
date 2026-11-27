/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QFont>
#include <QHash>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QToolTip>

#include "settings.h"

class QFontMetrics;
class QWidget;

enum class TooltipCategory {
    ItemInfo,
    ItemActions,
    HeaderButtons,
    PlainTextSwitch,
};

struct TooltipConfig {
    bool itemInfo = true;
    bool itemActions = true;
    bool headerButtons = true;
    bool plainTextSwitch = true;
    int delayMs = 500;
    ActionBarMode actionBarMode = ActionBarMode::MenuHoverOrClick;
    bool actionBarAnimation = true;

    bool isEnabled(TooltipCategory category) const;
    static TooltipConfig fromSettings();
};

namespace HoverTooltip
{
inline constexpr int kMaxContentWidthPx = 340;
inline constexpr int kMaxDelayMs = 3000;

int effectiveMaxWidthPx(const QFont &font = QToolTip::font());

QString elideLine(const QString &text, const QFontMetrics &fm, int maxWidthPx);

QString formatSourceTooltip(const QString &app,
                            const QString &title,
                            const QString &url,
                            const QString &relTime,
                            const QFont &font = QToolTip::font(),
                            int maxWidthPx = -1);

QString formatWrappedTooltip(const QString &text,
                             const QFont &font = QToolTip::font(),
                             int maxWidthPx = -1);

QString prepareDisplayText(const QString &plainFormattedText);
} // namespace HoverTooltip

class HoverTooltipController : public QObject
{
    Q_OBJECT

public:
    explicit HoverTooltipController(QObject *parent = nullptr);
    ~HoverTooltipController() override;

    void setConfig(const TooltipConfig &config);
    TooltipConfig config() const { return m_config; }

    void registerWidget(QWidget *widget,
                        TooltipCategory category,
                        std::function<QString()> textProvider,
                        bool wrapLongText = false);

    void hoverTarget(const QString &key,
                     TooltipCategory category,
                     const QString &formattedText,
                     const QPoint &globalPos,
                     QWidget *widget,
                     const QRect &validRect = {});

    void cancel();

    bool isPending() const { return m_timer.isActive(); }
    QString pendingKey() const { return m_pendingKey; }
    QString visibleKey() const { return m_visibleKey; }
    QString visibleText() const { return m_visibleText; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void onTimeout();

private:
    struct RegisteredWidget {
        TooltipCategory category = TooltipCategory::HeaderButtons;
        std::function<QString()> textProvider;
        bool wrapLongText = false;
        QString key;
    };

    void showNow(const QString &key,
                 TooltipCategory category,
                 const QString &formattedText,
                 const QPoint &globalPos,
                 QWidget *widget,
                 const QRect &validRect);

    TooltipConfig m_config;
    QTimer m_timer;
    QHash<QObject *, RegisteredWidget> m_registered;

    QString m_pendingKey;
    TooltipCategory m_pendingCategory = TooltipCategory::ItemInfo;
    QString m_pendingText;
    QPoint m_pendingGlobalPos;
    QPointer<QWidget> m_pendingWidget;
    QRect m_pendingValidRect;

    QString m_visibleKey;
    TooltipCategory m_visibleCategory = TooltipCategory::ItemInfo;
    QString m_visibleText;
    QPointer<QWidget> m_visibleWidget;
};
