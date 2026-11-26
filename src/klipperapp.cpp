/*
    SPDX-FileCopyrightText: Andrew Stanley-Jones <asj@cban.com>
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "klipperapp.h"

#include "configdialog.h"
#include "flaticons.h"
#include "globalshortcut.h"
#include "helpdialog.h"
#include "historycycler.h"
#include "historymodel.h"
#include "klipperpopup.h"
#include "notifications.h"
#include "settings.h"
#include "systemclipboard.h"
#include "themestyle.h"
#include "translations.h"
#include "updatedialog.h"
#include "updater.h"
#include "urlgrabber.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QTimer>

#include <sys/stat.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

KlipperApp::KlipperApp(QObject *parent)
    : QObject(parent)
    , m_clip(SystemClipboard::self())
    , m_historyModel(HistoryModel::self())
    , m_historyCycler(new HistoryCycler(this))
    , m_urlGrabber(new URLGrabber(this))
    , m_shortcuts(new GlobalShortcutManager(this))
    , m_updater(new Updater(this))
    , m_popup(std::make_unique<KlipperPopup>())
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerService(u"org.mklipper.App"_s);
    bus.registerService(u"org.kde.klipper"_s);
    bus.registerObject(u"/klipper"_s,
                       this,
                       QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);

    if (const auto top = m_historyModel->first()) {
        m_lastUrlGrabberText = top->text();
    }

    m_historyModel->setURLGrabber(m_urlGrabber);
    connect(m_urlGrabber, &URLGrabber::sigPopup, this, &KlipperApp::showActionPopupMenu);
    connect(m_historyModel.get(), &HistoryModel::actionInvoked, m_urlGrabber, &URLGrabber::invokeAction);
    connect(m_historyModel.get(), &HistoryModel::selectionForAction, this, [this](const HistoryItemConstPtr &item) {
        offerActions(item, true);
    });
    connect(m_historyModel.get(), &HistoryModel::changed, this, &KlipperApp::onHistoryChanged);
    connect(m_historyModel.get(), &HistoryModel::changed, this, &KlipperApp::clipboardHistoryUpdated);
    connect(m_historyModel.get(), &HistoryModel::changed, this, &KlipperApp::updateTrayTooltip);
    connect(m_historyModel.get(), &HistoryModel::rowsInserted, this, &KlipperApp::onHistoryRowsInserted);
    connect(m_historyModel.get(), &HistoryModel::rowsRemoved, this, &KlipperApp::onHistoryRowsRemoved);
    connect(m_historyModel.get(), &HistoryModel::modelReset, this, &KlipperApp::resetUnreadCount);
    connect(m_historyModel.get(), &HistoryModel::capturePausedChanged, this, [this](bool paused) {
        updateTrayIcon();
        updateTrayTooltip();
        if (m_togglePauseAct) {
            QSignalBlocker blk(m_togglePauseAct);
            m_togglePauseAct->setChecked(paused);
        }
    });

    connect(m_popup.get(), &KlipperPopup::popupShown, this, &KlipperApp::resetUnreadCount);
    connect(m_popup.get(), &KlipperPopup::configureRequested, this, &KlipperApp::configure);
    connect(m_popup.get(), &KlipperPopup::invokeActionRequested, this, &KlipperApp::onInvokeActionByUuid);

    connect(MklipperSettings::self(), &MklipperSettings::settingsChanged, this, &KlipperApp::reloadConfig);
    connect(m_updater, &Updater::updateAvailable, this, &KlipperApp::onUpdateAvailable);
    connect(m_updater, &Updater::installSucceeded, this, &KlipperApp::onUpdateInstallSucceeded);

    setupTrayIcon();
    reloadConfig();
    updateTrayTooltip();
    setupUpgradeWatcher();

    m_themeSyncTimer = new QTimer(this);
    m_themeSyncTimer->setSingleShot(true);
    connect(m_themeSyncTimer, &QTimer::timeout, this, &KlipperApp::syncThemeLive);

    // Intercept Qt's QGtk3Theme XSettings palette resets so Cinnamon's exact theme & accent always win
    if (qApp) {
        qApp->installEventFilter(this);
    }

    // Live-sync when user switches Cinnamon theme / accent color in System Settings
    bus.connect(QString(),
                u"/ca/desrt/dconf/Writer/user"_s,
                u"ca.desrt.dconf.Writer"_s,
                u"Notify"_s,
                this,
                SLOT(onDconfNotify(QString,QStringList,QString)));

    if (MklipperSettings::self()->autoCheckUpdates()) {
        QTimer::singleShot(8000, this, [this]() {
            if (MklipperSettings::self()->autoCheckUpdates() && !m_updater->isBusy()) {
                m_updater->checkForUpdates(false);
            }
        });
    }
}

KlipperApp::~KlipperApp()
{
    saveClipboardHistory();
    delete m_trayMenu;
    m_trayMenu = nullptr;
}

void KlipperApp::setupTrayIcon()
{
    const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());
    const int badgeCount = MklipperSettings::self()->showTrayBadge() ? m_unreadCount : 0;
    QIcon icon = FlatIcons::appLogoIcon(t.accent, m_historyModel->isCapturePaused(), badgeCount);
    if (icon.isNull()) {
        icon = QIcon(u":/icons/mklipper.svg"_s);
    }

    m_trayIcon = new QSystemTrayIcon(icon, this);
    m_trayMenu = new QMenu;
    rebuildTrayMenu();
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &KlipperApp::onTrayActivated);
    m_trayIcon->show();
}

void KlipperApp::updateTrayIcon()
{
    if (!m_trayIcon) {
        return;
    }
    const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());
    const int badgeCount = MklipperSettings::self()->showTrayBadge() ? m_unreadCount : 0;
    m_trayIcon->setIcon(FlatIcons::appLogoIcon(t.accent, m_historyModel->isCapturePaused(), badgeCount));
}

bool KlipperApp::eventFilter(QObject *watched, QEvent *ev)
{
    if (watched == qApp && ev->type() == QEvent::ApplicationPaletteChange && !m_syncingTheme) {
        if (m_themeSyncTimer) {
            m_themeSyncTimer->start(45);
        }
    }
    return QObject::eventFilter(watched, ev);
}

void KlipperApp::syncThemeLive()
{
    if (m_syncingTheme) {
        return;
    }
    m_syncingTheme = true;

    ThemeStyle::syncApplicationPalette();
    const QPalette appPal = QApplication::palette();
    const ThemeTokens t = ThemeTokens::fromPalette(appPal);
    const QIcon themedIcon = FlatIcons::appLogoIcon(t.accent);

    if (qApp) {
        qApp->setWindowIcon(themedIcon);
    }
    updateTrayIcon();
    rebuildTrayMenu();

    if (m_popup) {
        m_popup->refreshTheme();
    }
    if (m_configDialog) {
        m_configDialog->setPalette(appPal);
        m_configDialog->setWindowIcon(themedIcon);
        m_configDialog->setStyleSheet(ThemeStyle::dialogStyleSheet(t));
        m_configDialog->update();
    }
    if (m_helpDialog) {
        m_helpDialog->refreshTheme();
    }
    if (m_updateDialog) {
        m_updateDialog->setPalette(appPal);
        m_updateDialog->setWindowIcon(themedIcon);
        m_updateDialog->setStyleSheet(ThemeStyle::dialogStyleSheet(t));
        m_updateDialog->update();
    }

    m_syncingTheme = false;
}

void KlipperApp::onDconfNotify(const QString &prefix, const QStringList &changes, const QString &/*tag*/)
{
    bool relevant = prefix.contains(u"gtk-theme"_s)
        || prefix.contains(u"color-scheme"_s)
        || prefix.contains(u"/org/cinnamon/theme/"_s);
    if (!relevant && (prefix.contains(u"/desktop/interface"_s) || prefix.contains(u"/apps/portal"_s) || prefix == u"/"_s)) {
        for (const QString &ch : changes) {
            if (ch.contains(u"gtk-theme"_s) || ch.contains(u"color-scheme"_s)) {
                relevant = true;
                break;
            }
        }
    }
    if (!relevant) {
        return;
    }

    syncThemeLive();
    // Follow-up pass after csd-xsettings finishes broadcasting Net/ThemeName to Qt's QGtk3Theme
    if (m_themeSyncTimer) {
        m_themeSyncTimer->start(200);
    }
}

void KlipperApp::rebuildTrayMenu()
{
    if (!m_trayMenu) {
        m_trayMenu = new QMenu;
    }
    m_trayMenu->clear();
    m_togglePauseAct = nullptr;
    m_toggleActionsAct = nullptr;

    const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());
    auto menuIcon = [&t](FlatIcon id) {
        return FlatIcons::icon(id, t.textPrimary, t.accentFg, t.textMuted, 16);
    };

    const bool hasPending = m_updater && m_updater->hasPendingUpdate();
    if (hasPending) {
        auto *pendingAct = m_trayMenu->addAction(
            FlatIcons::icon(FlatIcon::Update, t.accent, t.accentFg, t.textMuted, 16),
            tr("Actualizar a Mklipper %1…").arg(m_updater->latestRelease().version));
        connect(pendingAct, &QAction::triggered, this, [this]() {
            if (m_updater->hasPendingUpdate()) {
                auto *dlg = ensureUpdateDialog();
                dlg->showRelease(m_updater->latestRelease());
                dlg->show();
                dlg->raise();
                dlg->activateWindow();
            } else {
                checkForUpdates();
            }
        });
        m_trayMenu->addSeparator();
    }

    auto *openAct = m_trayMenu->addAction(menuIcon(FlatIcon::Clipboard), tr("Abrir historial del portapapeles"));
    connect(openAct, &QAction::triggered, this, [this]() {
        m_popup->showNearTray(m_trayIcon ? m_trayIcon->geometry() : QRect());
    });

    m_togglePauseAct = m_trayMenu->addAction(tr("Pausar captura del portapapeles"));
    m_togglePauseAct->setCheckable(true);
    m_togglePauseAct->setChecked(m_historyModel->isCapturePaused());
    connect(m_togglePauseAct, &QAction::toggled, this, [this](bool paused) {
        m_historyModel->setCapturePaused(paused);
    });

    m_trayMenu->addSeparator();

    auto *actionAct = m_trayMenu->addAction(menuIcon(FlatIcon::Bolt), tr("Ejecutar acciones en elemento actual"));
    connect(actionAct, &QAction::triggered, this, [this]() { slotRepeatAction(false); });

    auto *editAct = m_trayMenu->addAction(menuIcon(FlatIcon::Edit), tr("Editar contenido actual…"));
    connect(editAct, &QAction::triggered, this, [this]() { m_popup->editCurrentClipboard(); });

    auto *qrAct = m_trayMenu->addAction(menuIcon(FlatIcon::QrCode), tr("Mostrar código QR actual…"));
    connect(qrAct, &QAction::triggered, this, [this]() { m_popup->showCurrentBarcode(); });

    m_toggleActionsAct = m_trayMenu->addAction(tr("Menú automático de acciones"));
    m_toggleActionsAct->setCheckable(true);
    m_toggleActionsAct->setChecked(m_urlGrabberEnabled);
    connect(m_toggleActionsAct, &QAction::toggled, this, &KlipperApp::setURLGrabberEnabled);

    m_trayMenu->addSeparator();

    auto *clearAct = m_trayMenu->addAction(menuIcon(FlatIcon::Trash), tr("Vaciar historial…"));
    connect(clearAct, &QAction::triggered, this, [this]() {
        m_historyModel->clearHistoryInteractive(nullptr);
    });

    auto *configAct = m_trayMenu->addAction(menuIcon(FlatIcon::Settings), tr("Configurar Mklipper…"));
    connect(configAct, &QAction::triggered, this, &KlipperApp::configure);

    auto *manualAct = m_trayMenu->addAction(menuIcon(FlatIcon::Book), tr("Manual de usuario…"));
    connect(manualAct, &QAction::triggered, this, &KlipperApp::showManual);

    auto *updateAct = m_trayMenu->addAction(menuIcon(FlatIcon::Refresh), tr("Buscar actualizaciones…"));
    connect(updateAct, &QAction::triggered, this, &KlipperApp::checkForUpdates);

    m_trayMenu->addSeparator();

    auto *quitAct = m_trayMenu->addAction(menuIcon(FlatIcon::Exit), tr("Salir"));
    connect(quitAct, &QAction::triggered, this, &KlipperApp::quit);
}

void KlipperApp::recreatePopup()
{
    const bool wasVisible = m_popup && m_popup->isVisible();
    m_popup = std::make_unique<KlipperPopup>();
    connect(m_popup.get(), &KlipperPopup::popupShown, this, &KlipperApp::resetUnreadCount);
    connect(m_popup.get(), &KlipperPopup::configureRequested, this, &KlipperApp::configure);
    connect(m_popup.get(), &KlipperPopup::invokeActionRequested, this, &KlipperApp::onInvokeActionByUuid);
    if (wasVisible) {
        m_popup->showNearTray(m_trayIcon ? m_trayIcon->geometry() : QRect());
    }
}

void KlipperApp::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        m_popup->toggleNearTray(m_trayIcon->geometry());
    } else if (reason == QSystemTrayIcon::Context) {
        if (m_trayMenu) {
            m_trayMenu->popup(QCursor::pos());
        }
    } else if (reason == QSystemTrayIcon::MiddleClick) {
        m_popup->toggleAtCursor();
    }
}

void KlipperApp::registerShortcuts()
{
    m_shortcuts->unregisterAll();
    const auto *s = MklipperSettings::self();

    if (GlobalShortcutManager::isCinnamonDesktop()) {
        QString exe = QCoreApplication::applicationFilePath();
        if (exe.contains(QLatin1Char(' ')) || exe.contains(QLatin1Char('\'')) || exe.contains(QLatin1Char('"'))) {
            exe.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
            exe = QLatin1Char('\'') + exe + QLatin1Char('\'');
        }
        const QList<CinnamonShortcutSpec> specs = {
            {u"mklipper-popup"_s, tr("Mklipper — Mostrar historial"), exe + u" --toggle"_s, QKeySequence::fromString(s->shortcutPopup())},
            {u"mklipper-toggle-plain-text"_s, tr("Mklipper — Alternar «Pegar sin formato»"), exe + u" --toggle-plain-text"_s, QKeySequence::fromString(s->shortcutTogglePlainText())},
            {u"mklipper-toggle-pause"_s, tr("Mklipper — Pausar / reanudar captura"), exe + u" --toggle-pause"_s, QKeySequence::fromString(s->shortcutTogglePause())},
            {u"mklipper-edit-current"_s, tr("Mklipper — Editar contenido actual"), exe + u" --edit"_s, QKeySequence::fromString(s->shortcutEditCurrent())},
            {u"mklipper-show-qr"_s, tr("Mklipper — Mostrar código QR actual"), exe + u" --qr"_s, QKeySequence::fromString(s->shortcutShowQr())},
            {u"mklipper-repeat-action"_s, tr("Mklipper — Invocar acciones"), exe + u" --action"_s, QKeySequence::fromString(s->shortcutRepeatAction())},
            {u"mklipper-cycle-next"_s, tr("Mklipper — Rotar historial (siguiente)"), exe + u" --cycle-next"_s, QKeySequence::fromString(s->shortcutCycleNext())},
            {u"mklipper-cycle-prev"_s, tr("Mklipper — Rotar historial (anterior)"), exe + u" --cycle-prev"_s, QKeySequence::fromString(s->shortcutCyclePrev())},
            {u"mklipper-clear-history"_s, tr("Mklipper — Vaciar historial"), exe + u" --clear-interactive"_s, QKeySequence::fromString(s->shortcutClearHistory())},
        };
        if (m_shortcuts->syncCinnamonShortcuts(specs)) {
            return;
        }
    }

    if (!s->shortcutPopup().isEmpty()) {
        m_shortcuts->registerShortcut(u"popup"_s, QKeySequence::fromString(s->shortcutPopup()), [this]() {
            m_popup->toggleAtCursor();
        });
    }
    if (!s->shortcutTogglePlainText().isEmpty()) {
        m_shortcuts->registerShortcut(u"togglePlainText"_s, QKeySequence::fromString(s->shortcutTogglePlainText()), [this]() {
            togglePastePlainText();
        });
    }
    if (!s->shortcutTogglePause().isEmpty()) {
        m_shortcuts->registerShortcut(u"togglePause"_s, QKeySequence::fromString(s->shortcutTogglePause()), [this]() {
            toggleCapturePaused();
        });
    }
    if (!s->shortcutEditCurrent().isEmpty()) {
        m_shortcuts->registerShortcut(u"editCurrent"_s, QKeySequence::fromString(s->shortcutEditCurrent()), [this]() {
            editCurrentClipboard();
        });
    }
    if (!s->shortcutShowQr().isEmpty()) {
        m_shortcuts->registerShortcut(u"showQr"_s, QKeySequence::fromString(s->shortcutShowQr()), [this]() {
            showCurrentBarcode();
        });
    }
    if (!s->shortcutRepeatAction().isEmpty()) {
        m_shortcuts->registerShortcut(u"repeatAction"_s, QKeySequence::fromString(s->shortcutRepeatAction()), [this]() {
            slotRepeatAction(false);
        });
    }
    if (!s->shortcutCycleNext().isEmpty()) {
        m_shortcuts->registerShortcut(u"cycleNext"_s, QKeySequence::fromString(s->shortcutCycleNext()), [this]() {
            slotCycleNext();
        });
    }
    if (!s->shortcutCyclePrev().isEmpty()) {
        m_shortcuts->registerShortcut(u"cyclePrev"_s, QKeySequence::fromString(s->shortcutCyclePrev()), [this]() {
            slotCyclePrev();
        });
    }
    if (!s->shortcutClearHistory().isEmpty()) {
        m_shortcuts->registerShortcut(u"clearHistory"_s, QKeySequence::fromString(s->shortcutClearHistory()), [this]() {
            clearClipboardHistoryInteractive();
        });
    }
}

void KlipperApp::reloadConfig()
{
    auto *s = MklipperSettings::self();
    if (calledFromDBus()) {
        s->load();
    }
    const bool langChanged = MklipperTranslations::applyLanguage(s->language());

    m_urlGrabberEnabled = s->urlGrabberEnabled();
    if (langChanged) {
        rebuildTrayMenu();
        recreatePopup();
        if (m_helpDialog) {
            m_helpDialog->close();
        }
        if (m_updateDialog && (!m_updater || !m_updater->isBusy())) {
            m_updateDialog->close();
        }
        updateTrayTooltip();
    } else if (m_toggleActionsAct) {
        QSignalBlocker blocker(m_toggleActionsAct);
        m_toggleActionsAct->setChecked(m_urlGrabberEnabled);
    }
    m_urlGrabber->loadSettings();
    m_historyModel->loadSettings();
    registerShortcuts();
    updateTrayIcon();
    updateTrayTooltip();
}

void KlipperApp::setURLGrabberEnabled(bool enable)
{
    if (m_urlGrabberEnabled != enable) {
        m_urlGrabberEnabled = enable;
        m_lastUrlGrabberText.clear();
        auto *s = MklipperSettings::self();
        s->setUrlGrabberEnabled(enable);
        s->save();
    }
}

void KlipperApp::offerActions(const HistoryItemConstPtr &item, bool fromSelection)
{
    if (fromSelection && !MklipperSettings::self()->actionOnSelection()) {
        return;
    }
    if (m_urlGrabberEnabled && item && (item->allTypes() & HistoryItemType::Text) && !item->text().isEmpty()) {
        if (item->text() != m_lastUrlGrabberText) {
            m_lastUrlGrabberText = item->text();
            m_urlGrabber->checkNewData(item);
        }
    } else {
        m_lastUrlGrabberText.clear();
    }
}

void KlipperApp::onHistoryChanged(bool isTop)
{
    if (!isTop) {
        return;
    }
    const bool newFromSelection = m_clip->isLocked(QClipboard::Selection);
    const bool newFromClipboard = m_clip->isLocked(QClipboard::Clipboard);
    if (newFromSelection || newFromClipboard) {
        offerActions(m_historyModel->first(), newFromSelection);
    }
}

void KlipperApp::onHistoryRowsInserted(const QModelIndex &/*parent*/, int first, int last)
{
    const int added = last - first + 1;
    if (added <= 0) {
        return;
    }
    if (m_popup && m_popup->isVisible()) {
        return;
    }
    m_unreadCount += added;
    updateTrayIcon();
    updateTrayTooltip();
}

void KlipperApp::onHistoryRowsRemoved()
{
    if (m_unreadCount <= 0) {
        return;
    }
    const int clamped = qMin(m_unreadCount, m_historyModel->rowCount());
    if (clamped != m_unreadCount) {
        m_unreadCount = clamped;
        updateTrayIcon();
        updateTrayTooltip();
    }
}

void KlipperApp::resetUnreadCount()
{
    if (m_unreadCount == 0) {
        return;
    }
    m_unreadCount = 0;
    updateTrayIcon();
    updateTrayTooltip();
}

void KlipperApp::onInvokeActionByUuid(const QString &uuid)
{
    const int row = m_historyModel->indexOf(uuid);
    if (row < 0) {
        return;
    }
    const auto item = m_historyModel->index(row).data(HistoryModel::HistoryItemConstPtrRole).value<HistoryItemConstPtr>();
    if (item) {
        m_urlGrabber->invokeAction(item);
    }
}

void KlipperApp::showActionPopupMenu(QMenu *menu)
{
    if (menu) {
        menu->popup(QCursor::pos());
    }
}

void KlipperApp::updateTrayTooltip()
{
    if (!m_trayIcon) {
        return;
    }
    if (m_historyModel->isCapturePaused()) {
        m_trayIcon->setToolTip(tr("Mklipper — Captura en pausa (modo incógnito)"));
        return;
    }
    const auto top = m_historyModel->first();
    if (!top) {
        m_trayIcon->setToolTip(tr("Mklipper — Portapapeles vacío"));
        return;
    }
    QString preview;
    if (top->type() == HistoryItemType::Image) {
        preview = m_historyModel->index(0).data(Qt::DisplayRole).toString();
    } else {
        preview = top->text().simplified();
        if (preview.size() > 80) {
            preview = preview.left(77) + u"…"_s;
        }
    }
    if (MklipperSettings::self()->showTrayBadge() && m_unreadCount > 0) {
        if (m_unreadCount == 1) {
            m_trayIcon->setToolTip(tr("Mklipper (1 nuevo) — Actual:\n%1").arg(preview));
        } else {
            m_trayIcon->setToolTip(tr("Mklipper (%1 nuevos) — Actual:\n%2").arg(m_unreadCount).arg(preview));
        }
        return;
    }
    m_trayIcon->setToolTip(tr("Mklipper — Actual:\n%1").arg(preview));
}

// ======================== D-Bus / Scriptable API ========================
QString KlipperApp::getClipboardContents()
{
    return getClipboardHistoryItem(0);
}

void KlipperApp::setClipboardContents(const QString &s)
{
    if (s.isEmpty()) {
        return;
    }
    auto data = std::make_unique<QMimeData>();
    data->setText(s);
    m_clip->setMimeData(data.get(), SystemClipboard::SelectionMode(SystemClipboard::Selection | SystemClipboard::Clipboard));
    m_clip->checkClipData(QClipboard::Clipboard, data.get());
}

void KlipperApp::clearClipboardContents()
{
    m_historyModel->clearBuffers();
}

void KlipperApp::clearClipboardHistory()
{
    m_historyModel->clear();
}

void KlipperApp::clearClipboardHistoryInteractive()
{
    m_historyModel->clearHistoryInteractive(nullptr);
}

void KlipperApp::saveClipboardHistory()
{
    m_historyModel->saveClipboardHistory();
}

QStringList KlipperApp::getClipboardHistoryMenu()
{
    QStringList list;
    const int count = m_historyModel->rowCount();
    list.reserve(count);
    for (int i = 0; i < count; ++i) {
        list.append(m_historyModel->index(i).data(Qt::DisplayRole).toString());
    }
    return list;
}

QString KlipperApp::getClipboardHistoryItem(int i)
{
    if (i < 0 || i >= m_historyModel->rowCount()) {
        return {};
    }
    return m_historyModel->index(i).data(Qt::DisplayRole).toString();
}

void KlipperApp::showKlipperPopupMenu()
{
    m_popup->showAtCursor();
}

void KlipperApp::toggleKlipperPopupMenu()
{
    m_popup->toggleAtCursor();
}

void KlipperApp::togglePastePlainText()
{
    auto *s = MklipperSettings::self();
    const bool next = !s->pastePlainText();
    s->setPastePlainText(next);
    s->save();
    Notifications::show(
        tr("Pegar sin formato"),
        next ? tr("Activado: el texto se pegará sin estilos ni colores HTML.")
             : tr("Desactivado: el texto conservará su formato original."),
        2500);
}

void KlipperApp::toggleCapturePaused()
{
    const bool paused = m_historyModel->toggleCapturePaused();
    Notifications::show(
        tr("Captura del portapapeles"),
        paused ? tr("En pausa (modo incógnito): no se guardará lo que copies.")
               : tr("Reanudada: Mklipper vuelve a guardar el historial."),
        2500);
}

void KlipperApp::editCurrentClipboard()
{
    m_popup->editCurrentClipboard();
}

void KlipperApp::showCurrentBarcode()
{
    m_popup->showCurrentBarcode();
}

void KlipperApp::showKlipperManuallyInvokeActionMenu()
{
    slotRepeatAction(false);
}

void KlipperApp::configure()
{
    if (m_configDialog) {
        m_configDialog->raise();
        m_configDialog->activateWindow();
        return;
    }
    m_configDialog = new ConfigDialog(m_urlGrabber);
    m_configDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_configDialog, &ConfigDialog::checkForUpdatesRequested, this, &KlipperApp::checkForUpdates);
    connect(m_configDialog, &ConfigDialog::showManualRequested, this, &KlipperApp::showManual);
    m_configDialog->show();
}

void KlipperApp::showManual()
{
    if (m_helpDialog) {
        m_helpDialog->raise();
        m_helpDialog->activateWindow();
        return;
    }
    m_helpDialog = new HelpDialog();
    m_helpDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_helpDialog->show();
}

UpdateDialog *KlipperApp::ensureUpdateDialog()
{
    if (!m_updateDialog) {
        m_updateDialog = new UpdateDialog(m_updater);
        m_updateDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    return m_updateDialog;
}

void KlipperApp::checkForUpdates()
{
    auto *dlg = ensureUpdateDialog();
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    if (!m_updater->isBusy()) {
        dlg->startCheck();
    }
}

void KlipperApp::onUpdateAvailable(const ReleaseInfo &release, bool userInitiated)
{
    rebuildTrayMenu();

    if (!userInitiated) {
        if (release.version == MklipperSettings::self()->skippedUpdateVersion()) {
            return;
        }
        Notifications::show(
            tr("Nueva versión de Mklipper disponible"),
            tr("Mklipper %1 ya está disponible. Haz clic en el icono de la bandeja → «Actualizar a Mklipper %1…» para instalarla.")
                .arg(release.version),
            9000);
    }
}

void KlipperApp::onUpdateInstallSucceeded(const ReleaseInfo &release)
{
    rebuildTrayMenu();
    Notifications::show(
        tr("Mklipper actualizado"),
        tr("Se instaló Mklipper %1 correctamente. Reiniciando…").arg(release.version),
        5000);
    restartAfterUpdate();
}

void KlipperApp::setupUpgradeWatcher()
{
    const QString exePath = QCoreApplication::applicationFilePath();
    if (exePath != u"/usr/bin/mklipper"_s && exePath != u"/usr/local/bin/mklipper"_s) {
        return;
    }

    struct stat st{};
    if (stat(exePath.toUtf8().constData(), &st) != 0) {
        return;
    }
    m_watchedExePath = exePath;
    m_exeDev = static_cast<quint64>(st.st_dev);
    m_exeIno = static_cast<quint64>(st.st_ino);

    m_upgradeDebounceTimer = new QTimer(this);
    m_upgradeDebounceTimer->setSingleShot(true);
    m_upgradeDebounceTimer->setInterval(2000);
    connect(m_upgradeDebounceTimer, &QTimer::timeout, this, &KlipperApp::checkInstalledBinaryUpdated);

    // Watch /usr/bin so package upgrades via Mint Update Manager / apt / dpkg trigger an automatic clean restart
    m_exeWatcher = new QFileSystemWatcher(this);
    const QString parentDir = QFileInfo(exePath).absolutePath();
    if (!parentDir.isEmpty()) {
        m_exeWatcher->addPath(parentDir);
    }
    m_exeWatcher->addPath(exePath);
    connect(m_exeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        if (m_upgradeDebounceTimer && !m_restarting) {
            m_upgradeDebounceTimer->start();
        }
    });
    connect(m_exeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        if (m_upgradeDebounceTimer && !m_restarting) {
            m_upgradeDebounceTimer->start();
        }
    });

    // Lightweight fallback poll in case system inotify watches are exhausted
    auto *fallbackTimer = new QTimer(this);
    fallbackTimer->setInterval(30000);
    connect(fallbackTimer, &QTimer::timeout, this, &KlipperApp::checkInstalledBinaryUpdated);
    fallbackTimer->start();
}

void KlipperApp::checkInstalledBinaryUpdated()
{
    if (m_restarting || m_watchedExePath.isEmpty()) {
        return;
    }
    if (m_updater && m_updater->isBusy()) {
        // In-app updater already handles its own restart once pkexec finishes
        return;
    }

    const QByteArray pathUtf8 = m_watchedExePath.toUtf8();
    struct stat st{};
    if (stat(pathUtf8.constData(), &st) != 0) {
        // Package was removed or is mid-replace; do not restart
        return;
    }
    if (!S_ISREG(st.st_mode) || access(pathUtf8.constData(), X_OK) != 0) {
        return;
    }

    const quint64 curDev = static_cast<quint64>(st.st_dev);
    const quint64 curIno = static_cast<quint64>(st.st_ino);
    if (curDev == m_exeDev && curIno == m_exeIno) {
        return;
    }

    m_restarting = true;
    Notifications::show(
        tr("Mklipper actualizado"),
        tr("Se detectó una nueva versión instalada. Reiniciando Mklipper…"),
        4000);
    restartAfterUpdate();
}

void KlipperApp::restartAfterUpdate()
{
    m_restarting = true;
    saveClipboardHistory();

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(u"/klipper"_s);
    bus.unregisterService(u"org.mklipper.App"_s);
    bus.unregisterService(u"org.kde.klipper"_s);

    QString targetExe = !m_watchedExePath.isEmpty() ? m_watchedExePath : u"/usr/bin/mklipper"_s;
    if (!QFileInfo::exists(targetExe)) {
        targetExe = u"/usr/bin/mklipper"_s;
    }
    if (!QFileInfo::exists(targetExe)) {
        targetExe = QCoreApplication::applicationFilePath();
    }

    const QString pidStr = QString::number(QCoreApplication::applicationPid());
    QProcess::startDetached(
        u"/bin/sh"_s,
        {u"-c"_s,
         u"i=0; while kill -0 \"$1\" 2>/dev/null && [ \"$i\" -lt 100 ]; do sleep 0.1; i=$((i+1)); done; exec \"$2\""_s,
         u"mklipper-restart"_s,
         pidStr,
         targetExe});

    QTimer::singleShot(150, qApp, &QCoreApplication::quit);
}

void KlipperApp::quit()
{
    saveClipboardHistory();
    qApp->quit();
}

void KlipperApp::slotRepeatAction(bool fromHistory)
{
    if (fromHistory && !MklipperSettings::self()->replayActionInHistory()) {
        return;
    }
    const auto top = m_historyModel->first();
    if (!top) {
        return;
    }
    if (fromHistory) {
        m_urlGrabber->checkNewData(top);
    } else {
        m_urlGrabber->invokeAction(top);
    }
}

void KlipperApp::slotCycleNext()
{
    if (m_historyModel->first()) {
        m_historyCycler->cycleNext();
        slotRepeatAction(true);
        Notifications::show(tr("Historial del portapapeles"), cycleNotificationText());
    }
}

void KlipperApp::slotCyclePrev()
{
    if (m_historyModel->first()) {
        m_historyCycler->cyclePrev();
        slotRepeatAction(true);
        Notifications::show(tr("Historial del portapapeles"), cycleNotificationText());
    }
}

QString KlipperApp::cycleNotificationText() const
{
    auto shorten = [](const HistoryItemConstPtr &item) -> QString {
        if (!item) {
            return {};
        }
        QString s = item->text().simplified();
        if (s.size() > 60) {
            s = s.left(57) + u"…"_s;
        }
        return s;
    };

    QStringList lines;
    if (auto prev = m_historyCycler->prevInCycle()) {
        lines.append(tr("Más reciente: %1").arg(shorten(prev)));
    }
    if (auto cur = m_historyModel->first()) {
        lines.append(tr("Actual: %1").arg(shorten(cur)));
    }
    if (auto next = m_historyCycler->nextInCycle()) {
        lines.append(tr("Anterior: %1").arg(shorten(next)));
    }
    return lines.join(QLatin1Char('\n'));
}
