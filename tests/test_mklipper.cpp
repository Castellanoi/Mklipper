/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "flaticons.h"
#include "historymodel.h"
#include "klipperpopup.h"
#include "settings.h"
#include "sourceinfo.h"
#include "systemclipboard.h"
#include "textutils.h"
#include "translations.h"
#include "urlgrabber.h"

#include <QApplication>
#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QStringConverter>
#include <QTemporaryDir>
#include <QUrl>

#include <cmath>
#include <iostream>

using namespace Qt::StringLiterals;

namespace
{
int g_failures = 0;

void expect(bool cond, const char *expr, int line)
{
    if (!cond) {
        ++g_failures;
        std::cerr << "FAIL [line " << line << "]: " << expr << std::endl;
    }
}

#define CHECK(expr) expect(static_cast<bool>(expr), #expr, __LINE__)

void testTextUtils()
{
    // Accent-insensitive folding & multi-token search
    CHECK(MklipperText::foldSearchText(u"Configuración"_s) == u"configuracion"_s);
    CHECK(MklipperText::foldSearchText(u"Français Über Código"_s) == u"francais uber codigo"_s);
    CHECK(MklipperText::matchesFoldedQuery(u"Configuración del Código"_s, u"configuracion codigo"_s));
    CHECK(!MklipperText::matchesFoldedQuery(u"Configuración del Código"_s, u"configuracion missing"_s));

    // URL detection
    CHECK(MklipperText::containsUrl(u"Check https://github.com/Castellanoi/Mklipper now"_s));
    CHECK(MklipperText::containsUrl(u"http://example.org/test"_s));
    CHECK(!MklipperText::containsUrl(u"just plain text without links"_s));

    // Deterministic relative timestamps
    MklipperTranslations::applyLanguage(u"es"_s);
    const qint64 nowMs = QDateTime(QDate(2026, 6, 15), QTime(14, 30, 0)).toMSecsSinceEpoch();
    const qreal nowSec = nowMs / 1000.0;
    CHECK(MklipperText::formatRelativeTime(nowSec - 20.0, nowMs) == u"Justo ahora"_s);
    CHECK(MklipperText::formatRelativeTime(nowSec - 300.0, nowMs) == u"Hace 5 min"_s);
    CHECK(MklipperText::formatRelativeTime(nowSec - 7200.0, nowMs) == u"Hace 2 h"_s);
    CHECK(MklipperText::formatRelativeTime(nowSec - 86400.0, nowMs).startsWith(u"Ayer,"_s));
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

    MklipperTranslations::applyLanguage(u"es"_s);
}

void testIgnoredAppsAndLockscreens()
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
}

void testHistoryModelAndFilters(const QString &tmpDir)
{
    auto model = HistoryModel::self();
    model->clear();
    CHECK(model->rowCount() == 0);

    // Reject empty / whitespace payloads
    QMimeData emptyMime;
    emptyMime.setText(QString());
    CHECK(!model->insert(&emptyMime));

    QMimeData wsMime;
    wsMime.setText(u"   \t\n  "_s);
    CHECK(!model->insert(&wsMime));

    // Accept text, link text, and image
    QMimeData textMime;
    textMime.setText(u"Configuración de prueba rápida"_s);
    ClipboardSourceInfo src1;
    src1.app = u"Xed"_s;
    CHECK(model->insert(&textMime, 0, src1));

    QMimeData linkMime;
    linkMime.setText(u"https://linuxmint.com/download"_s);
    CHECK(model->insert(&linkMime));

    QMimeData imgMime;
    QImage img(16, 12, QImage::Format_ARGB32);
    img.fill(QColor(255, 113, 57));
    imgMime.setImageData(img);
    CHECK(model->insert(&imgMime));

    CHECK(model->rowCount() == 3);

    // Verify timestamp is stored
    CHECK(model->index(0).data(HistoryModel::LastUsedTimeRole).toDouble() > 0.0);

    // Verify image export to PNG
    const QString imgUuid = model->index(0).data(HistoryModel::UuidRole).toString();
    const QString outPng = tmpDir + u"/exported.png"_s;
    CHECK(model->exportImageToFile(imgUuid, outPng));
    QImage reloaded(outPng);
    CHECK(!reloaded.isNull() && reloaded.width() == 16 && reloaded.height() == 12);

    // Star the text item & test HistoryFilterProxyModel categories + accent-insensitive search
    const QString textUuid = model->index(2).data(HistoryModel::UuidRole).toString();
    CHECK(model->setData(model->index(2), true, HistoryModel::StarredRole));
    CHECK(model->starredCount() == 1);

    HistoryFilterProxyModel proxy;
    proxy.setSourceModel(model.get());
    CHECK(proxy.rowCount() == 3);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::Images);
    CHECK(proxy.rowCount() == 1);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::Links);
    CHECK(proxy.rowCount() == 1);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::Starred);
    CHECK(proxy.rowCount() == 1);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::Text);
    CHECK(proxy.rowCount() == 2);

    proxy.setCategoryFilter(HistoryFilterProxyModel::CategoryFilter::All);
    proxy.setSearchQuery(u"configuracion rapida"_s);
    CHECK(proxy.rowCount() == 1);
    proxy.setSearchQuery(QString());

    // Capture pause mode toggle
    CHECK(!model->isCapturePaused());
    model->setCapturePaused(true);
    CHECK(model->isCapturePaused());
    model->toggleCapturePaused();
    CHECK(!model->isCapturePaused());

    // Export & re-import starred items
    const QString backupPath = tmpDir + u"/pinned.json"_s;
    int exported = 0;
    CHECK(model->exportStarredToJson(backupPath, &exported));
    CHECK(exported == 1);

    model->clear();
    CHECK(model->rowCount() == 0);

    int imported = 0;
    CHECK(model->importStarredFromJson(backupPath, &imported));
    CHECK(imported == 1);
    CHECK(model->rowCount() == 1);
    CHECK(model->starredCount() == 1);
    CHECK(model->index(0).data(Qt::DisplayRole).toString() == u"Configuración de prueba rápida"_s);

    // Re-importing same file should not duplicate
    int importedAgain = 0;
    CHECK(model->importStarredFromJson(backupPath, &importedAgain));
    CHECK(importedAgain == 0);
    CHECK(model->rowCount() == 1);

    // Reject invalid backup file
    const QString badPath = tmpDir + u"/bad.json"_s;
    QFile badFile(badPath);
    CHECK(badFile.open(QIODevice::WriteOnly));
    badFile.write("{\"format\":\"other\"}");
    badFile.close();
    CHECK(!model->importStarredFromJson(badPath, &imported));
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
    // Row 0 = newest, Row 1 = mid, Row 2 = older
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

    auto makePngBytes = [](int w, int h, const QColor &col) {
        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(col);
        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        QImageWriter writer(&buf, "PNG");
        writer.write(img);
        return bytes;
    };

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
        // Prefers webpage URL (text/x-moz-url-priv) over raw image URL
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
} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QTemporaryDir tmpDir;
    qputenv("MKLIPPER_DATABASE", (tmpDir.path() + u"/unit_history.sqlite"_s).toUtf8());

    QApplication app(argc, argv);
    app.setApplicationName(u"mklipper-tests"_s);
    app.setApplicationVersion(QStringLiteral(MKLIPPER_VERSION));

    testTextUtils();
    testTranslationsAndChangelog();
    testIgnoredAppsAndLockscreens();
    testHistoryModelAndFilters(tmpDir.path());
    testTimestampPreservationAndTrayBadge();
    testWebImageCopyPreservesFullImageAndSourceUrl();

    if (g_failures > 0) {
        std::cerr << "Mklipper tests FAILED with " << g_failures << " failure(s)." << std::endl;
        return 1;
    }
    std::cout << "All Mklipper unit tests passed!" << std::endl;
    return 0;
}
