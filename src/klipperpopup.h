/*
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include <QFrame>
#include <QRect>
#include <QSortFilterProxyModel>

class HistoryModel;
class QLabel;
class QLineEdit;
class QListView;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QRCodeWidget;
class ClipboardItemDelegate;
class X11Paster;

class HistoryFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit HistoryFilterProxyModel(QObject *parent = nullptr);

    void setStarredOnly(bool starredOnly);
    bool starredOnly() const { return m_starredOnly; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool m_starredOnly = false;
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

Q_SIGNALS:
    void configureRequested();
    void invokeActionRequested(const QString &uuid);

protected:
    bool event(QEvent *ev) override;
    bool eventFilter(QObject *watched, QEvent *ev) override;
    void keyPressEvent(QKeyEvent *ev) override;

private Q_SLOTS:
    void activateItem(const QModelIndex &proxyIndex);
    void openEditPage(const QModelIndex &proxyIndex);
    void openQrPage(const QModelIndex &proxyIndex);
    void toggleStar(const QModelIndex &proxyIndex);
    void deleteItem(const QModelIndex &proxyIndex);
    void invokeItemAction(const QModelIndex &proxyIndex);
    void saveEditedText();
    void updatePasswordBanner();
    void updateStatusLabel();

private:
    void showPositioned(const QPoint &anchorPos, bool anchorAbove);
    void returnToListPage();
    bool tryQuickSelectDigit(QKeyEvent *ke, bool allowBareDigit);
    QModelIndex sourceIndexFor(const QModelIndex &proxyIndex) const;

    std::shared_ptr<HistoryModel> m_model;
    HistoryFilterProxyModel *m_proxy = nullptr;
    ClipboardItemDelegate *m_delegate = nullptr;
    X11Paster *m_paster = nullptr;

    QStackedWidget *m_stack = nullptr;
    // Page 0: list
    QLineEdit *m_searchEdit = nullptr;
    QToolButton *m_starFilterBtn = nullptr;
    QToolButton *m_clearBtn = nullptr;
    QToolButton *m_configBtn = nullptr;
    QWidget *m_passwordBanner = nullptr;
    QListView *m_listView = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    // Page 1: edit
    QPlainTextEdit *m_editArea = nullptr;
    QString m_editingUuid;

    // Page 2: QR
    QRCodeWidget *m_qrWidget = nullptr;

    bool m_suppressAutoHide = false;
    qint64 m_lastHideTimeMs = 0;
};
