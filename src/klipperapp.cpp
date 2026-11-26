/*
    SPDX-FileCopyrightText: Andrew Stanley-Jones <asj@cban.com>
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "klipperapp.h"

#include "configdialog.h"
#include "globalshortcut.h"
#include "historycycler.h"
#include "historymodel.h"
#include "klipperpopup.h"
#include "notifications.h"
#include "settings.h"
#include "systemclipboard.h"
#include "translations.h"
#include "updatedialog.h"
#include "updater.h"
#include "urlgrabber.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QFileInfo>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QTimer>

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

    connect(m_popup.get(), &KlipperPopup::configureRequested, this, &KlipperApp::configure);
    connect(m_popup.get(), &KlipperPopup::invokeActionRequested, this, &KlipperApp::onInvokeActionByUuid);

    connect(MklipperSettings::self(), &MklipperSettings::settingsChanged, this, &KlipperApp::reloadConfig);
    connect(m_updater, &Updater::updateAvailable, this, &KlipperApp::onUpdateAvailable);
    connect(m_updater, &Updater::installSucceeded, this, &KlipperApp::onUpdateInstallSucceeded);

    setupTrayIcon();
    reloadConfig();
    updateTrayTooltip();

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
}

void KlipperApp::setupTrayIcon()
{
    QIcon icon(u":/icons/mklipper.svg"_s);
    if (icon.isNull()) {
        icon = QIcon::fromTheme(u"edit-paste"_s);
    }

    m_trayIcon = new QSystemTrayIcon(icon, this);
    rebuildTrayMenu();
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &KlipperApp::onTrayActivated);
    m_trayIcon->show();
}

void KlipperApp::rebuildTrayMenu()
{
    auto *oldMenu = m_trayMenu;
    m_trayMenu = new QMenu;

    m_pendingUpdateAct = m_trayMenu->addAction(QIcon::fromTheme(u"system-software-update"_s),
                                               tr("Actualizar Mklipper…"));
    const bool hasPending = m_updater && m_updater->hasPendingUpdate();
    if (hasPending) {
        m_pendingUpdateAct->setText(tr("Actualizar a Mklipper %1…").arg(m_updater->latestRelease().version));
    }
    m_pendingUpdateAct->setVisible(hasPending);
    connect(m_pendingUpdateAct, &QAction::triggered, this, [this]() {
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
    m_pendingUpdateSep = m_trayMenu->addSeparator();
    m_pendingUpdateSep->setVisible(hasPending);

    auto *openAct = m_trayMenu->addAction(QIcon::fromTheme(u"view-list-text"_s), tr("Abrir historial del portapapeles"));
    connect(openAct, &QAction::triggered, this, [this]() {
        m_popup->showNearTray(m_trayIcon ? m_trayIcon->geometry() : QRect());
    });

    m_trayMenu->addSeparator();

    auto *actionAct = m_trayMenu->addAction(QIcon::fromTheme(u"system-run"_s), tr("Ejecutar acciones en elemento actual"));
    connect(actionAct, &QAction::triggered, this, [this]() { slotRepeatAction(false); });

    auto *editAct = m_trayMenu->addAction(QIcon::fromTheme(u"document-edit"_s), tr("Editar contenido actual…"));
    connect(editAct, &QAction::triggered, this, [this]() { m_popup->editCurrentClipboard(); });

    auto *qrAct = m_trayMenu->addAction(QIcon::fromTheme(u"view-barcode-qr"_s), tr("Mostrar código QR actual…"));
    connect(qrAct, &QAction::triggered, this, [this]() { m_popup->showCurrentBarcode(); });

    m_toggleActionsAct = m_trayMenu->addAction(tr("Menú automático de acciones"));
    m_toggleActionsAct->setCheckable(true);
    m_toggleActionsAct->setChecked(m_urlGrabberEnabled);
    connect(m_toggleActionsAct, &QAction::toggled, this, &KlipperApp::setURLGrabberEnabled);

    m_trayMenu->addSeparator();

    auto *clearAct = m_trayMenu->addAction(QIcon::fromTheme(u"edit-clear-history"_s, QIcon::fromTheme(u"edit-clear"_s)),
                                           tr("Vaciar historial…"));
    connect(clearAct, &QAction::triggered, this, [this]() {
        m_historyModel->clearHistoryInteractive(nullptr);
    });

    auto *configAct = m_trayMenu->addAction(QIcon::fromTheme(u"preferences-system"_s), tr("Configurar Mklipper…"));
    connect(configAct, &QAction::triggered, this, &KlipperApp::configure);

    auto *updateAct = m_trayMenu->addAction(QIcon::fromTheme(u"system-software-update"_s, QIcon::fromTheme(u"view-refresh"_s)),
                                            tr("Buscar actualizaciones…"));
    connect(updateAct, &QAction::triggered, this, &KlipperApp::checkForUpdates);

    m_trayMenu->addSeparator();

    auto *quitAct = m_trayMenu->addAction(QIcon::fromTheme(u"application-exit"_s), tr("Salir"));
    connect(quitAct, &QAction::triggered, this, &KlipperApp::quit);

    if (m_trayIcon) {
        m_trayIcon->setContextMenu(m_trayMenu);
    }
    if (oldMenu) {
        oldMenu->deleteLater();
    }
}

void KlipperApp::recreatePopup()
{
    const bool wasVisible = m_popup && m_popup->isVisible();
    m_popup = std::make_unique<KlipperPopup>();
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
    } else if (reason == QSystemTrayIcon::MiddleClick) {
        slotRepeatAction(false);
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
            {u"mklipper-cycle-next"_s, tr("Mklipper — Rotar historial (siguiente)"), exe + u" --cycle-next"_s, QKeySequence::fromString(s->shortcutCycleNext())},
            {u"mklipper-cycle-prev"_s, tr("Mklipper — Rotar historial (anterior)"), exe + u" --cycle-prev"_s, QKeySequence::fromString(s->shortcutCyclePrev())},
            {u"mklipper-repeat-action"_s, tr("Mklipper — Invocar acciones"), exe + u" --action"_s, QKeySequence::fromString(s->shortcutRepeatAction())},
            {u"mklipper-clear-history"_s, tr("Mklipper — Vaciar historial"), exe + u" --clear"_s, QKeySequence::fromString(s->shortcutClearHistory())},
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
    if (!s->shortcutRepeatAction().isEmpty()) {
        m_shortcuts->registerShortcut(u"repeatAction"_s, QKeySequence::fromString(s->shortcutRepeatAction()), [this]() {
            slotRepeatAction(false);
        });
    }
    if (!s->shortcutClearHistory().isEmpty()) {
        m_shortcuts->registerShortcut(u"clearHistory"_s, QKeySequence::fromString(s->shortcutClearHistory()), [this]() {
            m_historyModel->clearHistoryInteractive(nullptr);
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
    m_configDialog->show();
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
    if (m_pendingUpdateAct && m_pendingUpdateSep) {
        m_pendingUpdateAct->setText(tr("Actualizar a Mklipper %1…").arg(release.version));
        m_pendingUpdateAct->setVisible(true);
        m_pendingUpdateSep->setVisible(true);
    }

    if (!userInitiated) {
        if (release.version == MklipperSettings::self()->skippedUpdateVersion()) {
            return;
        }
        Notifications::show(
            tr("Nueva versión de Mklipper disponible"),
            tr("Mklipper %1 ya está disponible. Hacé clic en el ícono de la bandeja → «Actualizar a Mklipper %1…» para instalarla.")
                .arg(release.version),
            9000);
    }
}

void KlipperApp::onUpdateInstallSucceeded(const ReleaseInfo &release)
{
    if (m_pendingUpdateAct && m_pendingUpdateSep) {
        m_pendingUpdateAct->setVisible(false);
        m_pendingUpdateSep->setVisible(false);
    }
    Notifications::show(
        tr("Mklipper actualizado"),
        tr("Se instaló Mklipper %1 correctamente. Reiniciando…").arg(release.version),
        5000);
    restartAfterUpdate();
}

void KlipperApp::restartAfterUpdate()
{
    saveClipboardHistory();

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(u"/klipper"_s);
    bus.unregisterService(u"org.mklipper.App"_s);
    bus.unregisterService(u"org.kde.klipper"_s);

    QString targetExe = u"/usr/bin/mklipper"_s;
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
