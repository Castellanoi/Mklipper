/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "clipcommandprocess.h"
#include "configdialog.h"
#include "editactiondialog.h"
#include "flaticons.h"
#include "helpdialog.h"
#include "historycycler.h"
#include "historymodel.h"
#include "hovertooltip.h"
#include "klipperpopup.h"
#include "previewwidget.h"
#include "qrcodewidget.h"
#include "settings.h"
#include "sourceinfo.h"
#include "systemclipboard.h"
#include "textutils.h"
#include "themestyle.h"
#include "tooltippreviewwidget.h"
#include "translations.h"
#include "updatedialog.h"
#include "updater.h"
#include "urlgrabber.h"
#include "x11keyboard.h"

#include <QApplication>
#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPalette>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include <cmath>
#include <iostream>

using namespace Qt::StringLiterals;

namespace
{
int g_failures = 0;
int g_checks = 0;

void expect(bool cond, const char *expr, int line)
{
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cerr << "FAIL [line " << line << "]: " << expr << std::endl;
    }
}

#define CHECK(expr) expect(static_cast<bool>(expr), #expr, __LINE__)

QByteArray makePngBytes(int w, int h, const QColor &col)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(col);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    QImageWriter writer(&buf, "PNG");
    writer.write(img);
    return bytes;
}

void testTextUtils()
{
    // Accent-insensitive folding & multi-token search
    CHECK(MklipperText::foldSearchText(QString()).isEmpty());
    CHECK(MklipperText::foldSearchText(u"Configuración"_s) == u"configuracion"_s);
    CHECK(MklipperText::foldSearchText(u"Français Über Código"_s) == u"francais uber codigo"_s);
    CHECK(MklipperText::splitSearchTokens(u"  alpha\t beta\n gamma  "_s) == QStringList({u"alpha"_s, u"beta"_s, u"gamma"_s}));
    CHECK(MklipperText::splitSearchTokens(u"   "_s).isEmpty());
    CHECK(MklipperText::matchesFoldedQuery(u"Configuración del Código"_s, u"configuracion codigo"_s));
    CHECK(MklipperText::matchesFoldedQuery(u"Anything"_s, u"   "_s));
    CHECK(!MklipperText::matchesFoldedQuery(QString(), u"needle"_s));
    CHECK(!MklipperText::matchesFoldedQuery(u"Configuración del Código"_s, u"configuracion missing"_s));

    // URL detection
    CHECK(MklipperText::containsUrl(u"Check https://github.com/Castellanoi/Mklipper now"_s));
    CHECK(MklipperText::containsUrl(u"HTTP://EXAMPLE.ORG/test"_s));
    CHECK(MklipperText::containsUrl(u"ftp://files.example.org/pub"_s));
    CHECK(!MklipperText::containsUrl(u"just plain text without links"_s));
    CHECK(!MklipperText::containsUrl(u"short"_s));

    // Exact URL detection (must be strictly a single URL, not text containing a URL)
    CHECK(MklipperText::isExactUrl(u"https://github.com/Castellanoi/Mklipper"_s));
    CHECK(MklipperText::isExactUrl(u"  HTTP://EXAMPLE.ORG/test?q=1#top  "_s));
    CHECK(MklipperText::isExactUrl(u"ftp://files.example.org/pub"_s));
    CHECK(!MklipperText::isExactUrl(u"Check https://github.com/Castellanoi/Mklipper now"_s));
    CHECK(!MklipperText::isExactUrl(u"https://a.com https://b.com"_s));
    CHECK(!MklipperText::isExactUrl(u"https://"_s));
    CHECK(!MklipperText::isExactUrl(u"plain text"_s));

    // Deterministic relative timestamps
    MklipperTranslations::applyLanguage(u"es"_s);
    const qint64 nowMs = QDateTime(QDate(2026, 6, 15), QTime(14, 30, 0)).toMSecsSinceEpoch();
    const qreal nowSec = nowMs / 1000.0;
    CHECK(MklipperText::formatRelativeTime(0.0, nowMs).isEmpty());
    CHECK(MklipperText::formatRelativeTime(-10.0, nowMs).isEmpty());
    CHECK(MklipperText::formatRelativeTime(nowSec - 20.0, nowMs) == u"Justo ahora"_s);
    CHECK(MklipperText::formatRelativeTime(nowSec - 300.0, nowMs) == u"Hace 5 min"_s);
    CHECK(MklipperText::formatRelativeTime(nowSec - 7200.0, nowMs) == u"Hace 2 h"_s);
    CHECK(MklipperText::formatRelativeTime(nowSec - 86400.0, nowMs).startsWith(u"Ayer,"_s));
    CHECK(!MklipperText::formatRelativeTime(nowSec - 5 * 86400.0, nowMs).isEmpty());
}

void testTranslationsAndChangelog()
{
    const auto langs = MklipperTranslations::availableLanguages();
    CHECK(langs.size() >= 7);

    const QList<QPair<QString, QString>> locales = {
        {u"es_ES"_s, u"es"_s},
        {u"en_US"_s, u"en"_s},
        {u"pt_BR"_s, u"pt_br"_s},
        {u"pt"_s, u"pt_br"_s},
        {u"fr_FR"_s, u"fr"_s},
        {u"it_IT"_s, u"it"_s},
        {u"de_DE"_s, u"de"_s},
        {u"ru_RU"_s, u"ru"_s},
    };
    for (const auto &[input, expected] : locales) {
        CHECK(MklipperTranslations::resolveLanguage(input) == expected);
    }

    for (const auto &l : langs) {
        MklipperTranslations::applyLanguage(l.code);
        const auto sections = MklipperTranslations::userManualSections();
        CHECK(sections.size() == 5);
        int cards = 0;
        for (const auto &sec : sections) {
            cards += sec.cards.size();
            for (const auto &card : sec.cards) {
                CHECK(!card.details.trimmed().isEmpty());
            }
        }
        CHECK(cards == 19);
    }

    const QString multiChangelog = u"### 🇪🇸 Español\n- Cambio ES\n\n### 🇬🇧 English\n- Change EN\n\n### 🇩🇪 Deutsch\n- Änderung DE"_s;
    CHECK(MklipperTranslations::filterReleaseNotesForLanguage(multiChangelog, u"es"_s) == u"- Cambio ES"_s);
    CHECK(MklipperTranslations::filterReleaseNotesForLanguage(multiChangelog, u"de"_s) == u"- Änderung DE"_s);
    // Fallback to English for language missing from changelog
    CHECK(MklipperTranslations::filterReleaseNotesForLanguage(multiChangelog, u"ru"_s) == u"- Change EN"_s);
    // Plain changelog untouched
    const QString plain = u"- Single change"_s;
    CHECK(MklipperTranslations::filterReleaseNotesForLanguage(plain, u"fr"_s) == plain);
    CHECK(MklipperTranslations::filterReleaseNotesForLanguage(QString(), u"es"_s).isEmpty());

    MklipperTranslations::applyLanguage(u"es"_s);
}

void testIgnoredAppsAndSourceInfo()
{
    CHECK(isIgnoredAppIdentifier(u"cinnamon-screensaver"_s));
    CHECK(isIgnoredAppIdentifier(u"gnome-screensaver"_s));
    CHECK(isIgnoredAppIdentifier(u"kscreenlocker_greet"_s));
    CHECK(isIgnoredAppIdentifier(u"polkit-gnome-authentication-agent-1"_s));
    CHECK(isIgnoredAppIdentifier(u"i3lock"_s));
    CHECK(!isIgnoredAppIdentifier(u"firefox"_s));

    const QStringList userList = {u"keepassxc"_s, u"MyBankApp"_s};
    CHECK(isIgnoredAppIdentifier(u"KeePassXC"_s, userList));
    CHECK(isIgnoredAppIdentifier(u"mybankapp-desktop"_s, userList));
    CHECK(!isIgnoredAppIdentifier(u"xed"_s, userList));

    // App name prettification & title cleanup
    CHECK(prettifyAppName(u"firefox-esr"_s, {}) == u"Firefox"_s);
    CHECK(prettifyAppName(u"code-oss"_s, {}) == u"VS Code"_s);
    CHECK(prettifyAppName(u"my_custom-tool.bin"_s, {}) == u"My custom tool"_s);
    CHECK(cleanWindowTitle(u"Docs — Mozilla Firefox"_s, u"Firefox"_s) == u"Docs"_s);
    CHECK(cleanWindowTitle(u"Firefox"_s, u"Firefox"_s).isEmpty());
    CHECK(formatCompactSourceLabel(u"Firefox"_s, u"Docs"_s, u"https://www.linuxmint.com/page"_s) == u"Firefox · linuxmint.com"_s);
    CHECK(formatCompactSourceLabel(u"Xed"_s, u"notes.txt"_s, {}) == u"Xed · notes.txt"_s);

    // Safe HTTP URL checks & MIME extraction security
    CHECK(isSafeHttpUrl(u"https://example.com/a"_s));
    CHECK(isSafeHttpUrl(u"http://example.com"_s));
    CHECK(!isSafeHttpUrl(u"file:///etc/passwd"_s));
    CHECK(!isSafeHttpUrl(u"javascript:alert(1)"_s));
    CHECK(!isSafeHttpUrl(u"https://"_s));

    QMimeData unsafeMime;
    unsafeMime.setData(u"chromium/x-source-url"_s, "javascript:alert(1)");
    unsafeMime.setHtml(u"<img src=\"file:///etc/passwd\"/>"_s);
    CHECK(extractSourceUrlFromMime(&unsafeMime).isEmpty());
}

void testHistoryModelAndFilters(const QString &tmpDir)
{
    auto model = HistoryModel::self();
    model->setMaxSize(50);
    model->clear();
    CHECK(model->rowCount() == 0);

    // Reject empty / whitespace payloads
    QMimeData emptyMime;
    emptyMime.setText(QString());
    CHECK(!model->insert(&emptyMime));

    QMimeData wsMime;
    wsMime.setText(u"   \t\n  "_s);
    CHECK(!model->insert(&wsMime));

    // Accept text, URL item, link text, and image
    QMimeData textMime;
    textMime.setText(u"Configuración de prueba rápida"_s);
    ClipboardSourceInfo src1;
    src1.app = u"Xed"_s;
    src1.title = u"borrador.txt"_s;
    CHECK(model->insert(&textMime, 0, src1));

    // Duplicate insertion with only sourceUrl must MERGE without wiping existing app/title
    ClipboardSourceInfo srcMerge;
    srcMerge.url = u"https://example.com/doc"_s;
    CHECK(model->insert(&textMime, 0, srcMerge));
    CHECK(model->index(0).data(HistoryModel::SourceAppRole).toString() == u"Xed"_s);
    CHECK(model->index(0).data(HistoryModel::SourceTitleRole).toString() == u"borrador.txt"_s);
    CHECK(model->index(0).data(HistoryModel::SourceUrlRole).toString() == u"https://example.com/doc"_s);

    QMimeData uriMime;
    uriMime.setUrls({QUrl(u"file:///tmp/demo.txt"_s)});
    CHECK(model->insert(&uriMime));
    CHECK(static_cast<HistoryItemType>(model->index(0).data(HistoryModel::TypeIntRole).toInt()) == HistoryItemType::Url);

    QMimeData linkMime;
    linkMime.setText(u"https://linuxmint.com/download"_s);
    CHECK(model->insert(&linkMime));

    QMimeData imgMime;
    QImage img(16, 12, QImage::Format_ARGB32);
    img.fill(QColor(255, 113, 57));
    imgMime.setImageData(img);
    CHECK(model->insert(&imgMime));

    CHECK(model->rowCount() == 4);

    // Verify timestamp is stored
    CHECK(model->index(0).data(HistoryModel::LastUsedTimeRole).toDouble() > 0.0);

    // Verify image export to PNG
    const QString imgUuid = model->index(0).data(HistoryModel::UuidRole).toString();
    const QString outPng = tmpDir + u"/exported.png"_s;
    CHECK(model->exportImageToFile(imgUuid, outPng));
    QImage reloaded(outPng);
    CHECK(!reloaded.isNull() && reloaded.width() == 16 && reloaded.height() == 12);

    // Star the text, URL, and image items & test HistoryFilterProxyModel categories + accent-insensitive search
    CHECK(model->setData(model->index(0), true, HistoryModel::StarredRole)); // image
    CHECK(model->setData(model->index(2), true, HistoryModel::StarredRole)); // url
    CHECK(model->setData(model->index(3), true, HistoryModel::StarredRole)); // text
    CHECK(model->starredCount() == 3);

    HistoryFilterProxyModel proxy;
    proxy.setSourceModel(model.get());
    CHECK(proxy.rowCount() == 4);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::Images);
    CHECK(proxy.rowCount() == 1);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::Links);
    CHECK(proxy.rowCount() == 2); // uriMime + linkMime

    // Paragraph merely containing a URL must NOT appear under Links filter
    QMimeData textWithEmbeddedUrl;
    textWithEmbeddedUrl.setText(u"Mira este sitio: https://linuxmint.com/download cuando puedas"_s);
    CHECK(model->insert(&textWithEmbeddedUrl));
    CHECK(proxy.rowCount() == 2);
    model->remove(model->index(0).data(HistoryModel::UuidRole).toString(), false);
    CHECK(proxy.rowCount() == 2);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::Starred);
    CHECK(proxy.rowCount() == 3);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::Text);
    CHECK(proxy.rowCount() == 3);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::All);
    proxy.setSearchQuery(u"configuracion rapida"_s);
    CHECK(proxy.rowCount() == 1);

    // Search matching across source metadata + text combined
    proxy.setSearchQuery(u"xed borrador"_s);
    CHECK(proxy.rowCount() == 1);
    proxy.setSearchQuery(QString());

    // Capture pause mode toggle
    CHECK(!model->isCapturePaused());
    model->setCapturePaused(true);
    CHECK(model->isCapturePaused());
    model->toggleCapturePaused();
    CHECK(!model->isCapturePaused());

    // Export & re-import starred items (Text + Url + Image!)
    const QString backupPath = tmpDir + u"/pinned.json"_s;
    int exported = 0;
    CHECK(model->exportStarredToJson(backupPath, &exported));
    CHECK(exported == 3);

    model->clear();
    CHECK(model->rowCount() == 0);

    int imported = 0;
    CHECK(model->importStarredFromJson(backupPath, &imported));
    CHECK(imported == 3);
    CHECK(model->rowCount() == 3);
    CHECK(model->starredCount() == 3);
    CHECK(static_cast<HistoryItemType>(model->index(0).data(HistoryModel::TypeIntRole).toInt()) == HistoryItemType::Image);
    CHECK(static_cast<HistoryItemType>(model->index(1).data(HistoryModel::TypeIntRole).toInt()) == HistoryItemType::Url);
    CHECK(model->index(2).data(Qt::DisplayRole).toString() == u"Configuración de prueba rápida"_s);

    // Re-importing same file should not duplicate
    int importedAgain = 0;
    CHECK(model->importStarredFromJson(backupPath, &importedAgain));
    CHECK(importedAgain == 0);
    CHECK(model->rowCount() == 3);

    // Security: malicious sourceUrl inside JSON backup MUST be stripped on import
    const QString evilBackup = tmpDir + u"/evil_pinned.json"_s;
    {
        QFile f(evilBackup);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write(R"JSON({
            "format": "mklipper-pinned",
            "version": 1,
            "items": [
                {
                    "type": "text",
                    "text": "Texto con origen malicioso",
                    "sourceApp": "Evil",
                    "sourceUrl": "javascript:alert(1)"
                },
                {
                    "type": "text",
                    "text": "Texto con file url",
                    "sourceUrl": "file:///etc/passwd"
                }
            ]
        })JSON");
        f.close();
    }
    int evilImported = 0;
    CHECK(model->importStarredFromJson(evilBackup, &evilImported));
    CHECK(evilImported == 2);
    CHECK(model->index(0).data(HistoryModel::SourceUrlRole).toString().isEmpty());
    CHECK(model->index(1).data(HistoryModel::SourceUrlRole).toString().isEmpty());

    // Reject invalid backup file
    const QString badPath = tmpDir + u"/bad.json"_s;
    QFile badFile(badPath);
    CHECK(badFile.open(QIODevice::WriteOnly));
    badFile.write("{\"format\":\"other\"}");
    badFile.close();
    CHECK(!model->importStarredFromJson(badPath, &imported));

    // Inline editing at row 0 preserves addedTime and updates text
    model->clear();
    const qreal oldTs = (QDateTime::currentMSecsSinceEpoch() / 1000.0) - 900.0;
    QMimeData editOrig;
    editOrig.setText(u"Original editable"_s);
    CHECK(model->insert(&editOrig, oldTs));
    CHECK(model->setData(model->index(0), u"Editado final"_s, Qt::EditRole));
    CHECK(model->index(0).data(Qt::DisplayRole).toString() == u"Editado final"_s);
    CHECK(std::abs(model->index(0).data(HistoryModel::AddedTimeRole).toDouble() - oldTs) < 0.01);
    CHECK(model->index(0).data(HistoryModel::LastUsedTimeRole).toDouble() > oldTs + 60.0);

    // Batch maxSize trimming preserves starred items and drops oldest non-starred items
    model->clear();
    model->setMaxSize(10);
    for (int i = 0; i < 6; ++i) {
        CHECK(model->insert(QStringLiteral("Item %1").arg(i)));
    }
    // Star oldest item (row 5 = "Item 0")
    CHECK(model->setData(model->index(5), true, HistoryModel::StarredRole));
    model->setMaxSize(3);
    CHECK(model->rowCount() == 3);
    CHECK(model->starredCount() == 1);
    CHECK(model->index(2).data(Qt::DisplayRole).toString() == u"Item 0"_s);
    model->setMaxSize(50);

    // Secret / password manager MIME payloads must NEVER be saved to history
    model->clear();
    QMimeData secretMime;
    secretMime.setText(u"SuperSecretPass123!"_s);
    secretMime.setData(u"x-kde-passwordManagerHint"_s, "secret");
    SystemClipboard::self()->checkClipData(QClipboard::Clipboard, &secretMime);
    CHECK(model->rowCount() == 0);
    CHECK(model->clipboardHasPassword());
    model->clearHasPassword();
    CHECK(!model->clipboardHasPassword());

    // PastePlainText strips HTML formatting when restoring text, keeping plain text intact
    QMimeData richMime;
    richMime.setText(u"Hola mundo"_s);
    richMime.setHtml(u"<b>Hola</b> <i>mundo</i>"_s);
    CHECK(model->insert(&richMime));
    const auto richItem = model->first();
    CHECK(richItem != nullptr);
    std::unique_ptr<QMimeData> plainRestored(SystemClipboard::self()->loadMimeDataFromDatabase(richItem, true));
    CHECK(plainRestored != nullptr);
    CHECK(plainRestored->hasText() && plainRestored->text() == u"Hola mundo"_s);
    CHECK(!plainRestored->hasHtml());
    std::unique_ptr<QMimeData> richRestored(SystemClipboard::self()->loadMimeDataFromDatabase(richItem, false));
    CHECK(richRestored != nullptr);
    CHECK(richRestored->hasHtml());
}

void testHistoryCycler()
{
    auto model = HistoryModel::self();
    model->clear();

    const qreal baseTs = (QDateTime::currentMSecsSinceEpoch() / 1000.0) - 500.0;
    QMimeData m1, m2, m3;
    m1.setText(u"First"_s);
    m2.setText(u"Second"_s);
    m3.setText(u"Third"_s);
    CHECK(model->insert(&m1, baseTs));
    CHECK(model->insert(&m2, baseTs + 10.0));
    CHECK(model->insert(&m3, baseTs + 20.0));

    // Order is Third (0), Second (1), First (2)
    HistoryCycler cycler;
    CHECK(cycler.prevInCycle() == nullptr);
    CHECK(cycler.nextInCycle() && cycler.nextInCycle()->text() == u"Second"_s);

    cycler.cycleNext();
    CHECK(model->first()->text() == u"Second"_s);
    CHECK(cycler.prevInCycle() && cycler.prevInCycle()->text() == u"Third"_s);
    CHECK(cycler.nextInCycle() && cycler.nextInCycle()->text() == u"First"_s);

    cycler.cycleNext();
    CHECK(model->first()->text() == u"First"_s);
    // At end of cycle, nextInCycle stops before wrapping back to start
    CHECK(cycler.nextInCycle() == nullptr);

    cycler.cyclePrev();
    CHECK(model->first()->text() == u"Second"_s);
    cycler.cyclePrev();
    CHECK(model->first()->text() == u"Third"_s);
    CHECK(cycler.prevInCycle() == nullptr);

    // Cycling must NOT mutate original addedTime or lastUsedTime
    CHECK(std::abs(model->index(0).data(HistoryModel::AddedTimeRole).toDouble() - (baseTs + 20.0)) < 0.01);
    CHECK(std::abs(model->index(0).data(HistoryModel::LastUsedTimeRole).toDouble() - (baseTs + 20.0)) < 0.01);
}

void testTimestampPreservationAndTrayBadge()
{
    auto model = HistoryModel::self();
    model->clear();
    CHECK(model->rowCount() == 0);

    const qreal nowSec = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const qreal tenMinAgo = nowSec - 600.0;
    const qreal fiveMinAgo = nowSec - 300.0;
    const qreal oneMinAgo = nowSec - 60.0;

    QMimeData olderMime;
    olderMime.setText(u"Elemento capturado hace 10 minutos"_s);
    CHECK(model->insert(&olderMime, tenMinAgo));

    QMimeData midMime;
    midMime.setText(u"Elemento capturado hace 5 minutos"_s);
    CHECK(model->insert(&midMime, fiveMinAgo));

    QMimeData newestMime;
    newestMime.setText(u"Elemento capturado hace 1 minuto"_s);
    CHECK(model->insert(&newestMime, oneMinAgo));

    CHECK(model->rowCount() == 3);
    CHECK(std::abs(model->index(0).data(HistoryModel::AddedTimeRole).toDouble() - oneMinAgo) < 0.01);
    CHECK(std::abs(model->index(1).data(HistoryModel::AddedTimeRole).toDouble() - fiveMinAgo) < 0.01);
    CHECK(std::abs(model->index(2).data(HistoryModel::AddedTimeRole).toDouble() - tenMinAgo) < 0.01);

    // 1) Deleting row 0 must NOT reset promoted row 1's AddedTime or LastUsedTime
    const QString newestUuid = model->index(0).data(HistoryModel::UuidRole).toString();
    CHECK(model->remove(newestUuid, false));
    CHECK(model->rowCount() == 2);
    CHECK(std::abs(model->index(0).data(HistoryModel::AddedTimeRole).toDouble() - fiveMinAgo) < 0.01);
    CHECK(std::abs(model->index(0).data(HistoryModel::LastUsedTimeRole).toDouble() - fiveMinAgo) < 0.01);

    // 2) Re-inserting duplicate already at row 0 must NOT reset its AddedTime or LastUsedTime
    CHECK(model->insert(&midMime));
    CHECK(std::abs(model->index(0).data(HistoryModel::AddedTimeRole).toDouble() - fiveMinAgo) < 0.01);
    CHECK(std::abs(model->index(0).data(HistoryModel::LastUsedTimeRole).toDouble() - fiveMinAgo) < 0.01);

    // 3) Moving older item to top (re-using / pasting it) must preserve its original AddedTime
    const QString olderUuid = model->index(1).data(HistoryModel::UuidRole).toString();
    model->moveToTop(olderUuid);
    CHECK(model->index(0).data(HistoryModel::UuidRole).toString() == olderUuid);
    CHECK(std::abs(model->index(0).data(HistoryModel::AddedTimeRole).toDouble() - tenMinAgo) < 0.01);

    // 4) Clearing non-starred history must preserve surviving starred item's AddedTime
    CHECK(model->setData(model->index(0), true, HistoryModel::StarredRole));
    model->clearNonStarredHistory();
    CHECK(model->rowCount() == 1);
    CHECK(std::abs(model->index(0).data(HistoryModel::AddedTimeRole).toDouble() - tenMinAgo) < 0.01);

    // 5) Tray notification badge setting & icon rendering
    auto *settings = MklipperSettings::self();
    const bool origBadge = settings->showTrayBadge();
    settings->setShowTrayBadge(false);
    CHECK(!settings->showTrayBadge());
    settings->setShowTrayBadge(true);
    CHECK(settings->showTrayBadge());
    settings->setShowTrayBadge(origBadge);

    const QColor accent(53, 168, 84);
    const QImage plainImg = FlatIcons::appLogoPixmap(accent, 32, 1.0, false, 0).toImage();
    const QImage plainClassic = FlatIcons::appLogoPixmap(accent, 32, 1.0, false).toImage();
    const QImage badge1Img = FlatIcons::appLogoPixmap(accent, 32, 1.0, false, 1).toImage();
    const QImage badge12Img = FlatIcons::appLogoPixmap(accent, 32, 1.0, false, 12).toImage();
    const QImage badgeOverImg = FlatIcons::appLogoPixmap(accent, 32, 1.0, false, 120).toImage();
    const QImage pausedImg = FlatIcons::appLogoPixmap(accent, 32, 1.0, true, 5).toImage();

    CHECK(!plainImg.isNull());
    CHECK(plainImg == plainClassic);
    CHECK(badge1Img != plainImg);
    CHECK(badge12Img != badge1Img);
    CHECK(badgeOverImg != badge12Img);
    CHECK(pausedImg != badge1Img);

    // 6) New-item highlight animation state on popup
    KlipperPopup popup;
    const QString testUuid = model->index(0).data(HistoryModel::UuidRole).toString();
    CHECK(popup.highlightIntensityFor(testUuid) == 0.0);
    popup.highlightNewItems({testUuid});
    CHECK(popup.highlightIntensityFor(testUuid) > 0.9);
    CHECK(popup.highlightIntensityFor(u"0000000000000000000000000000000000000000"_s) == 0.0);
    popup.show();
    popup.hide();
    CHECK(popup.highlightIntensityFor(testUuid) == 0.0);
}

void testWebImageCopyPreservesFullImageAndSourceUrl()
{
    auto model = HistoryModel::self();
    model->clear();
    CHECK(model->rowCount() == 0);

    // 1) Chrome / Brave "Copy Image": image/png + text/html + text/plain (image URL) + text/uri-list + chromium/x-source-url
    {
        QMimeData chromeImg;
        chromeImg.setData(u"image/png"_s, makePngBytes(640, 480, QColor(53, 168, 84)));
        chromeImg.setHtml(u"<img src=\"https://cdn.example.com/photo.jpg\" alt=\"Photo\"/>"_s);
        chromeImg.setText(u"https://cdn.example.com/photo.jpg"_s);
        chromeImg.setUrls({QUrl(u"https://cdn.example.com/photo.jpg"_s)});
        chromeImg.setData(u"chromium/x-source-url"_s, "https://example.com/gallery/page-1");

        ClipboardSourceInfo src;
        src.app = u"Chrome"_s;
        src.title = u"Gallery"_s;
        CHECK(model->insert(&chromeImg, 0, src));
        CHECK(model->rowCount() == 1);

        const QModelIndex idx = model->index(0);
        CHECK(static_cast<HistoryItemType>(idx.data(HistoryModel::TypeIntRole).toInt()) == HistoryItemType::Image);
        CHECK(idx.data(HistoryModel::ImageSizeRole).toSize() == QSize(640, 480));
        CHECK(idx.data(HistoryModel::SourceUrlRole).toString() == u"https://example.com/gallery/page-1"_s);
        CHECK(idx.data(HistoryModel::SourceAppRole).toString() == u"Chrome"_s);

        // Even with PastePlainText enabled, restoring to clipboard MUST preserve full-resolution image and NOT turn into URL text
        const auto item = model->first();
        CHECK(item != nullptr);
        std::unique_ptr<QMimeData> restored(SystemClipboard::self()->loadMimeDataFromDatabase(item, true));
        CHECK(restored != nullptr);
        CHECK(restored->hasImage());
        CHECK(!restored->hasText());
        CHECK(!restored->hasUrls());
        const QImage restoredImg = restored->imageData().value<QImage>();
        CHECK(restoredImg.size() == QSize(640, 480));
    }

    // 2) Firefox "Copy Image": image/png + <img src=... alt=...> + alt text in text/plain + text/x-moz-url-priv (UTF-16LE)
    {
        QMimeData ffImg;
        ffImg.setData(u"image/png"_s, makePngBytes(800, 600, QColor(41, 128, 185)));
        ffImg.setHtml(u"<html><body><!--StartFragment--><img src=\"https://img.example.org/cat.webp\" alt=\"Cute cat\"/><!--EndFragment--></body></html>"_s);
        ffImg.setText(u"Cute cat"_s);
        auto toUtf16Le = QStringEncoder(QStringEncoder::Utf16LE);
        ffImg.setData(u"text/x-moz-url-priv"_s, toUtf16Le(u"https://example.org/articles/cats\nCats"_s));
        ffImg.setData(u"text/x-moz-url"_s, toUtf16Le(u"https://img.example.org/cat.webp\ncat.webp"_s));

        CHECK(model->insert(&ffImg));
        const QModelIndex idx = model->index(0);
        CHECK(static_cast<HistoryItemType>(idx.data(HistoryModel::TypeIntRole).toInt()) == HistoryItemType::Image);
        CHECK(idx.data(HistoryModel::ImageSizeRole).toSize() == QSize(800, 600));
        CHECK(idx.data(HistoryModel::SourceUrlRole).toString() == u"https://example.org/articles/cats"_s);
    }

    // 3) Image with only <img src="..."> in HTML falls back to extracting src URL
    {
        QMimeData htmlOnlyImg;
        htmlOnlyImg.setData(u"image/png"_s, makePngBytes(320, 240, QColor(229, 57, 53)));
        htmlOnlyImg.setHtml(u"<img src=\"https://site.test/pic.png?a=1&amp;b=2\">"_s);

        CHECK(model->insert(&htmlOnlyImg));
        const QModelIndex idx = model->index(0);
        CHECK(static_cast<HistoryItemType>(idx.data(HistoryModel::TypeIntRole).toInt()) == HistoryItemType::Image);
        CHECK(idx.data(HistoryModel::ImageSizeRole).toSize() == QSize(320, 240));
        CHECK(idx.data(HistoryModel::SourceUrlRole).toString() == u"https://site.test/pic.png?a=1&b=2"_s);
    }

    // 4) Spreadsheet/rich-document copy (non-URL text + HTML table + bitmap preview) MUST stay Text, not Image
    {
        QMimeData calcMime;
        calcMime.setText(u"100\t200\n300\t400"_s);
        calcMime.setHtml(u"<table><tr><td>100</td><td>200</td></tr></table>"_s);
        calcMime.setData(u"image/png"_s, makePngBytes(120, 60, QColor(200, 200, 200)));

        CHECK(model->insert(&calcMime));
        const QModelIndex idx = model->index(0);
        CHECK(static_cast<HistoryItemType>(idx.data(HistoryModel::TypeIntRole).toInt()) == HistoryItemType::Text);
    }
}

void testClipCommandProcessSecurity(const QString &tmpDir)
{
    // 1) Shell quoting and macro expansion resistance against shell injection
    QHash<QChar, QString> map;
    const QString evil = u"hello'; touch /tmp/pwned; echo \"$(id)\" `whoami` $HOME"_s;
    map.insert(QLatin1Char('s'), evil);
    map.insert(QLatin1Char('1'), u"grp'1"_s);

    // Unquoted macro wraps in single quotes and escapes internal single quotes
    const QString unquoted = ClipCommandProcess::expandMacros(u"echo %s"_s, map);
    CHECK(unquoted.startsWith(u"echo '"_s) && unquoted.endsWith(u"'"_s));
    CHECK(unquoted.contains(u"'\\''"_s));

    // Inside single quotes, does NOT add redundant outer quotes and escapes internal '
    const QString inSingle = ClipCommandProcess::expandMacros(u"echo '%s'"_s, map);
    CHECK(inSingle.startsWith(u"echo 'hello'\\''"_s));

    // Inside double quotes, escapes \, ", $, `
    const QString inDouble = ClipCommandProcess::expandMacros(u"echo \"%s\""_s, map);
    CHECK(inDouble.contains(u"\\$(id)"_s));
    CHECK(inDouble.contains(u"\\`whoami\\`"_s));
    CHECK(inDouble.contains(u"\\$HOME"_s));
    CHECK(inDouble.contains(u"\\\""_s));

    // Literal %% and capture group %1
    CHECK(ClipCommandProcess::expandMacros(u"printf '100%% %1'"_s, map) == u"printf '100% grp'\\''1'"_s);

    // NUL bytes stripped
    QString withNul = u"abc"_s;
    withNul.append(QChar('\0'));
    withNul.append(u"def"_s);
    CHECK(ClipCommandProcess::shellQuote(withNul) == u"'abcdef'"_s);

    // 2) Live process execution: UTF-8 preservation & no command injection when running /bin/sh -c
    auto model = HistoryModel::self();
    model->clear();
    const QString markerFile = tmpDir + u"/injection_marker"_s;
    const QString hostileInput = QStringLiteral("café ñ 🚀 $(touch %1) `touch %1` 'quoted'").arg(markerFile);
    CHECK(model->insert(hostileInput));
    const auto origItem = model->first();

    ClipAction act(u".*"_s, u"Echo"_s, false);
    ClipCommand cmd(u"printf '%s' %s"_s, u"Safe echo"_s, true, {}, ClipCommand::ADD);
    auto *proc = new ClipCommandProcess(act, cmd, hostileInput, origItem);

    QEventLoop loop;
    QObject::connect(proc, &QProcess::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    proc->launch();
    loop.exec();

    CHECK(!QFile::exists(markerFile));
    CHECK(model->first() && model->first()->text() == hostileInput);

    // 3) Failing command in REPLACE mode must NOT delete the original item
    model->clear();
    CHECK(model->insert(u"Keep me safe"_s));
    const auto protectedItem = model->first();
    ClipCommand failCmd(u"exit 1"_s, u"Failing"_s, true, {}, ClipCommand::REPLACE);
    auto *failProc = new ClipCommandProcess(act, failCmd, u"Keep me safe"_s, protectedItem);

    QEventLoop loop2;
    QObject::connect(failProc, &QProcess::finished, &loop2, &QEventLoop::quit);
    QTimer::singleShot(3000, &loop2, &QEventLoop::quit);
    failProc->launch();
    loop2.exec();

    CHECK(model->rowCount() == 1);
    CHECK(model->first()->text() == u"Keep me safe"_s);
}

void testUrlGrabberAndActions()
{
    auto *settings = MklipperSettings::self();
    settings->setEnableMagicMimeActions(true);

    URLGrabber grabber;
    grabber.loadSettings();

    auto makeTextItem = [](const QString &txt) {
        return std::make_shared<HistoryItem>(
            u"0123456789abcdef0123456789abcdef01234567"_s,
            QStringList{u"text/plain"_s},
            txt);
    };

    // Magic MIME actions: valid URLs (including uppercase scheme), rejecting spaces & empty hosts
    CHECK(grabber.hasMatchingActions(makeTextItem(u"https://linuxmint.com"_s)));
    CHECK(grabber.hasMatchingActions(makeTextItem(u"HTTPS://EXAMPLE.ORG/path"_s)));
    CHECK(grabber.hasMatchingActions(makeTextItem(u"mailto:user@example.org"_s)));
    CHECK(!grabber.hasMatchingActions(makeTextItem(u"https://example.com with extra words"_s)));
    CHECK(!grabber.hasMatchingActions(makeTextItem(u"http://"_s)));
    CHECK(!grabber.hasMatchingActions(makeTextItem(u"plain text sentence"_s)));

    // Custom regex action + ReDoS oversized input guard + invalid regex guard
    ClipAction invalidAct(u"[unclosed"_s, u"Broken"_s, true);
    CHECK(!invalidAct.match(u"abc"_s).hasMatch());

    ClipAction ticketAct(u"^JIRA-([0-9]+)$"_s, u"Open Ticket"_s, true);
    ticketAct.addCommand(ClipCommand(u"echo %1"_s, u"Print ID"_s, true));
    CHECK(ticketAct.match(u"JIRA-42"_s).hasMatch());
    CHECK(ticketAct.match(u"JIRA-42"_s).captured(1) == u"42"_s);
    // Input > 64 KB must be safely skipped
    CHECK(!ticketAct.match(QString(70000, QLatin1Char('1'))).hasMatch());

    // Replacing action list cleanly
    ActionList custom;
    auto *dynAct = new ClipAction(u"^TICKET-[0-9]+$"_s, u"Tickets"_s, true);
    dynAct->addCommand(ClipCommand(u"echo %s"_s, u"Show"_s, true));
    custom.append(dynAct);
    grabber.setActionList(custom);
    CHECK(grabber.hasMatchingActions(makeTextItem(u"TICKET-123"_s)));
    grabber.setActionList({});
    CHECK(!grabber.hasMatchingActions(makeTextItem(u"TICKET-123"_s)));
}

void testUpdaterSecurityAndParsing()
{
    // Version normalization & semantic comparison
    CHECK(Updater::normalizeVersionString(u"v1.8.3"_s) == u"1.8.3"_s);
    CHECK(Updater::normalizeVersionString(u"V2.0.1-rc1+build42"_s) == u"2.0.1"_s);
    CHECK(Updater::isNewerVersion(u"v1.9.0"_s, u"1.8.3"_s));
    CHECK(Updater::isNewerVersion(u"1.8.10"_s, u"1.8.3"_s));
    CHECK(!Updater::isNewerVersion(u"1.8.3"_s, u"1.8.3"_s));
    CHECK(!Updater::isNewerVersion(u"1.8.2"_s, u"1.8.3"_s));
    CHECK(!Updater::isNewerVersion(u"not-a-version"_s, u"1.8.3"_s));

    // SHA-256 parsing
    const QString hashA = QString(64, QLatin1Char('a'));
    const QString hashB = QString(64, QLatin1Char('b'));
    const QByteArray sums = (hashA + u"  mklipper_1.9.0_amd64.deb\n"_s + hashB + u" *mklipper_1.9.0_arm64.deb\n"_s).toUtf8();
    CHECK(Updater::parseSha256ForFile(sums, u"mklipper_1.9.0_amd64.deb"_s) == hashA);
    CHECK(Updater::parseSha256ForFile(sums, u"mklipper_1.9.0_arm64.deb"_s) == hashB);
    CHECK(Updater::parseSha256ForFile(sums, u"mklipper_1.9.0_i386.deb"_s).isEmpty());
    CHECK(Updater::parseSha256ForFile((hashA.toUpper() + u"\n"_s).toUtf8(), u"anything.deb"_s) == hashA);
    CHECK(Updater::parseSha256ForFile("not-a-valid-hash\n", u"anything.deb"_s).isEmpty());

    // URL allowlist security
    qunsetenv("MKLIPPER_UPDATE_URL");
    CHECK(Updater::isAllowedUrl(QUrl(u"https://github.com/Castellanoi/Mklipper/releases/tag/v1.9.0"_s)));
    CHECK(Updater::isAllowedUrl(QUrl(u"https://api.github.com/repos/Castellanoi/Mklipper/releases/latest"_s)));
    CHECK(Updater::isAllowedUrl(QUrl(u"https://raw.githubusercontent.com/Castellanoi/Mklipper/main/dist/latest.json"_s)));
    CHECK(Updater::isAllowedUrl(QUrl(u"https://objects.githubusercontent.com/some/asset"_s)));
    CHECK(Updater::isAllowedUrl(QUrl(u"https://release-assets.githubusercontent.com/some/asset"_s)));
    CHECK(!Updater::isAllowedUrl(QUrl(u"http://github.com/Castellanoi/Mklipper"_s)));
    CHECK(!Updater::isAllowedUrl(QUrl(u"https://evil.example.com/mklipper.deb"_s)));
    CHECK(!Updater::isAllowedUrl(QUrl(u"https://github.com.evil.com/mklipper.deb"_s)));
    CHECK(!Updater::isAllowedUrl(QUrl(u"file:///tmp/mklipper.deb"_s)));
    CHECK(!Updater::isAllowedUrl(QUrl(u"http://127.0.0.1:8080/latest.json"_s)));

    qputenv("MKLIPPER_UPDATE_URL", "http://127.0.0.1:8080/latest.json");
    CHECK(Updater::isAllowedUrl(QUrl(u"http://127.0.0.1:8080/latest.json"_s)));
    CHECK(!Updater::isAllowedUrl(QUrl(u"http://evil.example.com/latest.json"_s)));
    qunsetenv("MKLIPPER_UPDATE_URL");

    // JSON parsing: valid manifest + security rejections + stale sha256 reset
    const QString arch = Updater::systemArchitecture();
    const QString goodDeb = QStringLiteral("mklipper_2.0.0_%1.deb").arg(arch);
    const QJsonObject manifest{
        {u"tag_name"_s, u"v2.0.0"_s},
        {u"name"_s, u"Mklipper v2.0.0"_s},
        {u"html_url"_s, u"https://github.com/Castellanoi/Mklipper/releases/tag/v2.0.0"_s},
        {u"body"_s, u"- Release notes"_s},
        {u"assets"_s, QJsonArray{
            // Untrusted host must be ignored
            QJsonObject{
                {u"name"_s, goodDeb},
                {u"url"_s, u"https://evil.com/"_s + goodDeb},
                {u"sha256"_s, hashA},
            },
            // Unsafe package name must be ignored
            QJsonObject{
                {u"name"_s, u"trojan_2.0.0_"_s + arch + u".deb"_s},
                {u"url"_s, u"https://github.com/Castellanoi/Mklipper/releases/download/v2.0.0/trojan.deb"_s},
                {u"sha256"_s, hashA},
            },
            // Valid asset with inline sha256
            QJsonObject{
                {u"name"_s, goodDeb},
                {u"url"_s, u"https://raw.githubusercontent.com/Castellanoi/Mklipper/main/dist/"_s + goodDeb},
                {u"size"_s, 480000},
                {u"sha256"_s, hashB},
            },
        }},
    };
    ReleaseInfo parsed;
    CHECK(Updater::parseReleaseJson(manifest, parsed));
    CHECK(parsed.version == u"2.0.0"_s);
    CHECK(parsed.assetName == goodDeb);
    CHECK(parsed.expectedSha256 == hashB);
    CHECK(parsed.assetSize == 480000);

    // Second matching asset without sha256 must NOT inherit first asset's sha256
    const QJsonObject multiManifest{
        {u"tag_name"_s, u"v2.0.1"_s},
        {u"assets"_s, QJsonArray{
            QJsonObject{
                {u"name"_s, QStringLiteral("mklipper_2.0.1_%1.deb").arg(arch)},
                {u"url"_s, u"https://github.com/Castellanoi/Mklipper/releases/download/v2.0.1/a.deb"_s},
                {u"sha256"_s, hashA},
            },
            QJsonObject{
                {u"name"_s, QStringLiteral("mklipper_2.0.1_%1.deb").arg(arch)},
                {u"url"_s, u"https://github.com/Castellanoi/Mklipper/releases/download/v2.0.1/b.deb"_s},
            },
        }},
    };
    ReleaseInfo parsedMulti;
    CHECK(Updater::parseReleaseJson(multiManifest, parsedMulti));
    CHECK(parsedMulti.expectedSha256.isEmpty());

    // Invalid version rejected
    const QJsonObject badVer{{u"tag_name"_s, u"invalid!!"_s}};
    ReleaseInfo badOut;
    CHECK(!Updater::parseReleaseJson(badVer, badOut));
}

void testX11KeyboardAndSettings(const QString &tmpDir)
{
    // Round-trip key conversions
    for (int k = Qt::Key_A; k <= Qt::Key_Z; ++k) {
        const auto qk = static_cast<Qt::Key>(k);
        CHECK(X11Keyboard::keysymToQtKey(X11Keyboard::qtKeyToKeysym(qk)) == qk);
    }
    for (int k = Qt::Key_0; k <= Qt::Key_9; ++k) {
        const auto qk = static_cast<Qt::Key>(k);
        CHECK(X11Keyboard::keysymToQtKey(X11Keyboard::qtKeyToKeysym(qk)) == qk);
    }
    for (int i = 0; i < 24; ++i) {
        const auto qk = static_cast<Qt::Key>(Qt::Key_F1 + i);
        CHECK(X11Keyboard::keysymToQtKey(X11Keyboard::qtKeyToKeysym(qk)) == qk);
    }
    CHECK(X11Keyboard::isModifierQtKey(Qt::Key_Control));
    CHECK(X11Keyboard::isModifierQtKey(Qt::Key_Super_L));
    CHECK(!X11Keyboard::isModifierQtKey(Qt::Key_V));

    CHECK(X11Keyboard::qtCombinationToCinnamonAccel(QKeyCombination(Qt::MetaModifier, Qt::Key_V)) == u"<Super>v"_s);
    CHECK(X11Keyboard::qtCombinationToCinnamonAccel(QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_F12))
          == u"<Primary><Shift>F12"_s);
    CHECK(X11Keyboard::qtCombinationToCinnamonAccel(QKeyCombination(Qt::ControlModifier, Qt::Key_Control)).isEmpty());

    X11ModifierMap modMap;
    modMap.numLockMask = (1u << 4);
    modMap.scrollLockMask = (1u << 7);
    const unsigned int xMask = modMap.qtModsToX11(Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    CHECK(modMap.x11MaskToQt(xMask) == (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    CHECK(modMap.ignoredLockCombinations().size() == 8); // 2^3 for Caps, Num, Scroll

    // Settings clamping & deduplication
    auto *s = MklipperSettings::self();
    s->setMaxClipItems(0);
    CHECK(s->maxClipItems() == 1);
    s->setMaxClipItems(99999);
    CHECK(s->maxClipItems() == 2048);
    s->setMaxClipItems(50);

    s->setTimeoutForActionPopups(-5);
    CHECK(s->timeoutForActionPopups() == 0);
    s->setTimeoutForActionPopups(999);
    CHECK(s->timeoutForActionPopups() == 200);
    s->setTimeoutForActionPopups(8);

    s->setIgnoredApps({u" KeePassXC "_s, u"keepassxc"_s, u""_s, u"Bitwarden"_s});
    CHECK(s->ignoredApps().size() == 2);

    // Autostart desktop parsing ignores comments and handles enable/disable cleanly
    MklipperSettings::setAutostartEnabled(true);
    CHECK(MklipperSettings::autostartEnabled());
    MklipperSettings::setAutostartEnabled(false);
    CHECK(!MklipperSettings::autostartEnabled());

    const QString desktopFile = tmpDir + u"/config/autostart/mklipper.desktop"_s;
    QFile f(desktopFile);
    CHECK(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    f.write("[Desktop Entry]\nType=Application\n# Hidden=true\nExec=mklipper\nX-GNOME-Autostart-enabled=true\n");
    f.close();
    CHECK(MklipperSettings::autostartEnabled());
}

void testUiWidgetsAndDialogsSmoke(const QString &tmpDir)
{
    // Verify light and dark stylesheets have zero unreplaced %TOKEN% placeholders
    static const QRegularExpression reToken(QStringLiteral("%[A-Z_]+%"));
    for (bool dark : {false, true}) {
        QPalette pal;
        pal.setColor(QPalette::Window, dark ? QColor(36, 36, 40) : QColor(246, 246, 248));
        pal.setColor(QPalette::WindowText, dark ? QColor(238, 238, 240) : QColor(32, 32, 36));
        pal.setColor(QPalette::Highlight, QColor(53, 168, 84));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        const ThemeTokens t = ThemeTokens::fromPalette(pal);
        CHECK(t.isDark == dark);
        CHECK(!reToken.match(ThemeStyle::popupStyleSheet(t)).hasMatch());
        CHECK(!reToken.match(ThemeStyle::dialogStyleSheet(t)).hasMatch());
    }

    // PreviewWidget text, image, clearPreview, theme refresh
    PreviewWidget preview;
    preview.setTextPreview(u"uuid1"_s, HistoryItemType::Text, u"Line 1\nLine 2"_s, u"Xed · Justo ahora"_s);
    const QString tmpPng = tmpDir + u"/smoke.png"_s;
    QFile pngFile(tmpPng);
    CHECK(pngFile.open(QIODevice::WriteOnly));
    pngFile.write(makePngBytes(64, 48, QColor(80, 140, 220)));
    pngFile.close();
    preview.setImagePreview(u"uuid2"_s, tmpPng, u"Chrome"_s);
    preview.refreshTheme();
    preview.clearPreview();

    // QRCodeWidget normal, empty, and oversized payload handling
    QRCodeWidget qr;
    qr.setText(u"https://linuxmint.com"_s);
    qr.setText(QString());
    qr.setText(QString(3000, QLatin1Char('x')));
    qr.refreshTheme();

    // HelpDialog, UpdateDialog, ConfigDialog, EditActionDialog smoke construction
    HelpDialog helpDlg;
    helpDlg.refreshTheme();

    Updater updater;
    UpdateDialog updDlg(&updater);
    ReleaseInfo sampleRel;
    sampleRel.version = u"2.0.0"_s;
    sampleRel.tagName = u"v2.0.0"_s;
    sampleRel.releaseNotes = u"### 🇪🇸 Español\n- Prueba\n### 🇬🇧 English\n- Test"_s;
    updDlg.showRelease(sampleRel);

    URLGrabber grabber;
    ConfigDialog cfgDlg(&grabber);
    EditActionDialog actDlg(nullptr);
    std::unique_ptr<ClipAction> takenAction(actDlg.takeAction());
    CHECK(takenAction != nullptr);
}

void testTooltipsFormattingControllerAndSettings()
{
    MklipperTranslations::applyLanguage(u"es"_s);
    const QFont tipFont = QToolTip::font();
    const QFontMetrics fm(tipFont);
    const int maxW = 260;

    // Short line untouched; long URL truncated to fixed width ending with "..."
    CHECK(HoverTooltip::elideLine(u"Firefox"_s, fm, maxW) == u"Firefox"_s);
    const QString longUrl = u"https://example.com/"_s + QString(300, QLatin1Char('a'));
    const QString elidedUrl = HoverTooltip::elideLine(longUrl, fm, maxW);
    CHECK(elidedUrl.endsWith(u"..."_s));
    CHECK(elidedUrl.size() < longUrl.size());
    CHECK(fm.horizontalAdvance(elidedUrl) <= maxW);

    // Multi-KB input is safely bounded and truncated with "..."
    const QString hugeUrl = u"https://example.com/"_s + QString(20000, QLatin1Char('z'));
    const QString elidedHuge = HoverTooltip::elideLine(hugeUrl, fm, maxW);
    CHECK(elidedHuge.endsWith(u"..."_s));
    CHECK(fm.horizontalAdvance(elidedHuge) <= maxW);

    // formatSourceTooltip trims each line to maxW and omits empty fields
    const QString srcTip = HoverTooltip::formatSourceTooltip(
        u"Firefox"_s,
        u"Un título de ventana extremadamente largo que debería cortarse de manera prolija para no ocupar toda la pantalla"_s,
        longUrl,
        u"Hace 2 min"_s,
        tipFont,
        maxW);
    const QStringList srcLines = srcTip.split(QLatin1Char('\n'));
    CHECK(srcLines.size() == 4);
    for (const QString &line : srcLines) {
        CHECK(fm.horizontalAdvance(line) <= maxW);
    }
    CHECK(srcLines.at(2).endsWith(u"..."_s));
    CHECK(HoverTooltip::formatSourceTooltip({}, {}, {}, {}).isEmpty());

    // formatWrappedTooltip wraps long text and elides unbreakable tokens
    const QString wrapped = HoverTooltip::formatWrappedTooltip(
        u"Quita colores, fuentes y estilos HTML al pegar texto (no afecta imágenes ni archivos). "_s + longUrl,
        tipFont,
        maxW);
    const QStringList wrappedLines = wrapped.split(QLatin1Char('\n'));
    CHECK(wrappedLines.size() >= 2);
    for (const QString &line : wrappedLines) {
        CHECK(fm.horizontalAdvance(line) <= maxW);
    }
    CHECK(wrappedLines.last().endsWith(u"..."_s));

    // prepareDisplayText escapes HTML-like text safely while leaving plain text untouched
    CHECK(HoverTooltip::prepareDisplayText(u"Aplicación: Firefox"_s) == u"Aplicación: Firefox"_s);
    const QString richSafe = HoverTooltip::prepareDisplayText(u"Ventana: <b>Prueba</b>\nEnlace: https://a.com"_s);
    CHECK(richSafe.contains(u"&lt;b&gt;Prueba&lt;/b&gt;"_s));
    CHECK(richSafe.contains(u"<br>"_s));

    // HoverTooltipController: category enable/disable, 0ms immediate show, >0ms delay & cancel
    QWidget host;
    host.resize(200, 100);
    host.show();
    HoverTooltipController ctrl(&host);

    TooltipConfig cfg;
    cfg.itemInfo = true;
    cfg.itemActions = false;
    cfg.headerButtons = true;
    cfg.plainTextSwitch = true;
    cfg.delayMs = 0;
    ctrl.setConfig(cfg);

    ctrl.hoverTarget(u"row:0:act:1"_s, TooltipCategory::ItemActions, u"Eliminar"_s, QPoint(10, 10), &host, host.rect());
    CHECK(ctrl.visibleKey().isEmpty());
    CHECK(!ctrl.isPending());

    ctrl.hoverTarget(u"row:0:info"_s, TooltipCategory::ItemInfo, u"Aplicación: Firefox"_s, QPoint(10, 10), &host, host.rect());
    CHECK(ctrl.visibleKey() == u"row:0:info"_s);
    CHECK(ctrl.visibleText() == u"Aplicación: Firefox"_s);

    // Disabling active category hides it immediately
    cfg.itemInfo = false;
    ctrl.setConfig(cfg);
    CHECK(ctrl.visibleKey().isEmpty());

    // Non-zero delay starts timer and fires after elapsed delay
    cfg.itemInfo = true;
    cfg.delayMs = 40;
    ctrl.setConfig(cfg);
    ctrl.hoverTarget(u"row:1:info"_s, TooltipCategory::ItemInfo, u"Aplicación: Xed"_s, QPoint(15, 15), &host, host.rect());
    CHECK(ctrl.isPending());
    CHECK(ctrl.visibleKey().isEmpty());

    QEventLoop loop;
    QTimer::singleShot(110, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(!ctrl.isPending());
    CHECK(ctrl.visibleKey() == u"row:1:info"_s);

    ctrl.cancel();
    CHECK(ctrl.visibleKey().isEmpty());

    // TooltipPreviewWidget smoke & live config propagation
    TooltipPreviewWidget tipPreview;
    cfg.actionBarMode = ActionBarMode::MenuClickOnly;
    cfg.actionBarAnimation = false;
    tipPreview.setTooltipConfig(cfg);
    CHECK(tipPreview.tooltipConfig().delayMs == 40);
    CHECK(!tipPreview.tooltipConfig().itemActions);
    CHECK(tipPreview.tooltipConfig().actionBarMode == ActionBarMode::MenuClickOnly);
    CHECK(!tipPreview.tooltipConfig().actionBarAnimation);
    cfg.actionBarMode = ActionBarMode::AlwaysVisible;
    tipPreview.setTooltipConfig(cfg);
    CHECK(tipPreview.tooltipConfig().actionBarMode == ActionBarMode::AlwaysVisible);
    cfg.actionBarMode = ActionBarMode::AlwaysOnHover;
    cfg.actionBarAnimation = true;
    tipPreview.setTooltipConfig(cfg);
    tipPreview.refreshTheme();

    // Settings persistence & delay clamping
    auto *s = MklipperSettings::self();
    s->setTooltipItemInfo(false);
    s->setTooltipItemActions(true);
    s->setTooltipHeaderButtons(false);
    s->setTooltipPlainTextSwitch(false);
    s->setActionBarAnimation(false);
    s->setTooltipDelayMs(99999);
    CHECK(s->tooltipDelayMs() == 3000);
    s->setTooltipDelayMs(-50);
    CHECK(s->tooltipDelayMs() == 0);
    s->setTooltipDelayMs(650);
    for (ActionBarMode m : {ActionBarMode::MenuHoverOrClick,
                            ActionBarMode::MenuClickOnly,
                            ActionBarMode::AlwaysOnHover,
                            ActionBarMode::AlwaysVisible}) {
        s->setActionBarMode(m);
        s->save();
        s->load();
        CHECK(s->actionBarMode() == m);
    }
    CHECK(!s->tooltipItemInfo());
    CHECK(s->tooltipItemActions());
    CHECK(!s->tooltipHeaderButtons());
    CHECK(!s->tooltipPlainTextSwitch());
    CHECK(!s->actionBarAnimation());
    CHECK(s->tooltipDelayMs() == 650);
    // Restore defaults
    s->setTooltipItemInfo(true);
    s->setTooltipItemActions(true);
    s->setTooltipHeaderButtons(true);
    s->setTooltipPlainTextSwitch(true);
    s->setTooltipDelayMs(500);
    s->setActionBarMode(ActionBarMode::MenuHoverOrClick);
    s->setActionBarAnimation(true);
    s->save();
}
} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QTemporaryDir tmpDir;
    qputenv("XDG_CONFIG_HOME", (tmpDir.path() + u"/config"_s).toUtf8());
    qputenv("MKLIPPER_DATABASE", (tmpDir.path() + u"/unit_history.sqlite"_s).toUtf8());

    QApplication app(argc, argv);
    app.setApplicationName(u"mklipper-tests"_s);
    app.setApplicationVersion(QStringLiteral(MKLIPPER_VERSION));

    testTextUtils();
    testTranslationsAndChangelog();
    testIgnoredAppsAndSourceInfo();
    testHistoryModelAndFilters(tmpDir.path());
    testHistoryCycler();
    testTimestampPreservationAndTrayBadge();
    testWebImageCopyPreservesFullImageAndSourceUrl();
    testClipCommandProcessSecurity(tmpDir.path());
    testUrlGrabberAndActions();
    testUpdaterSecurityAndParsing();
    testX11KeyboardAndSettings(tmpDir.path());
    testUiWidgetsAndDialogsSmoke(tmpDir.path());
    testTooltipsFormattingControllerAndSettings();

    if (g_failures > 0) {
        std::cerr << "Mklipper tests FAILED with " << g_failures << " failure(s) out of " << g_checks << " checks." << std::endl;
        return 1;
    }
    std::cout << "All Mklipper unit tests passed! (" << g_checks << " checks)" << std::endl;
    return 0;
}
