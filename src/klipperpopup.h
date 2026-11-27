/*
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include <QFrame>
#include <QHash>
#include <QPersistentModelIndex>
#include <QRect>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>

class HistoryModel;
class QAbstractButton;
class QAction;
class QButtonGroup;
class QLabel;
class QLineEdit;
class QListView;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class PreviewWidget;
class QRCodeWidget;
class ClipboardItemDelegate;
class HoverTooltipController;
class X11Paster;

class HistoryFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum class CategoryFilter {
        All,
        Text,
        Images,
        Links,
        Starred,
    };

    explicit HistoryFilterProxyModel(QObject *parent = nullptr);

    void setCategoryFilter(CategoryFilter filter);
    CategoryFilter categoryFilter() const { return m_category; }

    void setStarredOnly(bool starredOnly);
    bool starredOnly() const { return m_category == CategoryFilter::Starred; }

    void setSearchQuery(const QString &query);
    QString searchQuery() const { return m_rawQuery; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    CategoryFilter m_category = CategoryFilter::All;
    QString m_rawQuery;
    QString m_foldedQuery;
    QStringList m_foldedTokens;
};

class KlipperPopup : public QFrame
{
    Q_OBJECT

public:
    explicit KlipperPopup(QWidget *parent = nullptr);
    ~KlipperPopup() override = default;

    void showAtCursor();
    void showNearTray(const QRect &trayRect);
    void toggleAtCursor();
    void toggleNearTray(const QRect &trayRect);

    void editCurrentClipboard();
    void showCurrentBarcode();
    void refreshTheme();
    void highlightNewItems(const QSet<QString> &uuids);
    qreal highlightIntensityFor(const QString &uuid) const;

Q_SIGNALS:
    void popupShown();
    void configureRequested();
    void invokeActionRequested(const QString &uuid);

protected:
    bool event(QEvent *ev) override;
    void changeEvent(QEvent *ev) override;
    bool eventFilter(QObject *watched, QEvent *ev) override;
    void keyPressEvent(QKeyEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;

private Q_SLOTS:
    void activateItem(const QModelIndex &proxyIndex, bool invertPlainText = false);
    void activateItemByUuid(const QString &uuid, bool invertPlainText = false);
    void openEditPage(const QModelIndex &proxyIndex);
    void openEditPageByUuid(const QString &uuid);
    void openQrPage(const QModelIndex &proxyIndex);
    void openPreviewPage(const QModelIndex &proxyIndex);
    void saveImageByUuid(const QString &uuid);
    void toggleStar(const QModelIndex &proxyIndex);
    void deleteItem(const QModelIndex &proxyIndex);
    void invokeItemAction(const QModelIndex &proxyIndex);
    void saveEditedText();
    void updatePasswordBanner();
    void updatePauseUi();
    void updateStatusLabel();
    void updatePersistentUi();
    void onPasteFinished();
    void tickNewItemHighlights();
    void tickActionBarAnimations();

private:
    void syncActionBarTargets();
    void applyTheme();
    void showPositioned(const QPoint &anchorPos, bool anchorAbove);
    void returnToListPage();
    void setActiveCategory(HistoryFilterProxyModel::CategoryFilter cat);
    bool tryQuickSelectDigit(QKeyEvent *ke, bool allowBareDigit);
    QModelIndex sourceIndexFor(const QModelIndex &proxyIndex) const;

    std::shared_ptr<HistoryModel> m_model;
    HistoryFilterProxyModel *m_proxy = nullptr;
    ClipboardItemDelegate *m_delegate = nullptr;
    X11Paster *m_paster = nullptr;

    QStackedWidget *m_stack = nullptr;
    // Page 0: list
    QLineEdit *m_searchEdit = nullptr;
    QAction *m_searchIconAction = nullptr;
    QToolButton *m_pauseBtn = nullptr;
    QToolButton *m_clearBtn = nullptr;
    QToolButton *m_configBtn = nullptr;
    QToolButton *m_closeBtn = nullptr;
    QButtonGroup *m_categoryGroup = nullptr;
    QPushButton *m_filterAllBtn = nullptr;
    QPushButton *m_filterTextBtn = nullptr;
    QPushButton *m_filterImagesBtn = nullptr;
    QPushButton *m_filterLinksBtn = nullptr;
    QPushButton *m_filterStarredBtn = nullptr;
    QAbstractButton *m_plainTextSwitch = nullptr;
    QLabel *m_plainTextHintLabel = nullptr;
    QWidget *m_pauseBanner = nullptr;
    QLabel *m_pauseIconLabel = nullptr;
    QLabel *m_pauseLabel = nullptr;
    QWidget *m_passwordBanner = nullptr;
    QLabel *m_pwIconLabel = nullptr;
    QLabel *m_pwLabel = nullptr;
    QStackedWidget *m_listStack = nullptr;
    QListView *m_listView = nullptr;
    QFrame *m_emptyBadge = nullptr;
    QLabel *m_emptyIconLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_emptySubLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    // Page 1: edit
    QPushButton *m_editBackBtn = nullptr;
    QLabel *m_editTitleLabel = nullptr;
    QPushButton *m_saveEditBtn = nullptr;
    QPlainTextEdit *m_editArea = nullptr;
    QString m_editingUuid;

    // Page 2: QR
    QRCodeWidget *m_qrWidget = nullptr;

    // Page 3: Preview
    PreviewWidget *m_previewWidget = nullptr;

    bool m_applyingTheme = false;
    bool m_suppressAutoHide = false;
    bool m_pastingInProgress = false;
    quint64 m_pasteGeneration = 0;
    bool m_dragging = false;
    QPoint m_dragOffset;
    qint64 m_lastHideTimeMs = 0;
    QTimer *m_relTimeTimer = nullptr;
    QTimer *m_highlightTimer = nullptr;
    QHash<QString, qint64> m_highlightStartMs;
    QTimer *m_actionAnimTimer = nullptr;
    qint64 m_lastActionAnimMs = 0;
    QHash<QString, qreal> m_actionBarProgress;
    QSet<QString> m_pinnedMenuUuids;
    QString m_hoverMenuUuid;
    QString m_suppressedHoverMenuUuid;
    HoverTooltipController *m_tooltipController = nullptr;
};
