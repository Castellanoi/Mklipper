/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "translations.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMap>
#include <QStandardPaths>
#include <QTranslator>

#include <algorithm>
#include <memory>

using namespace Qt::StringLiterals;

namespace
{
struct LanguagePack {
    LanguageInfo info;
    QString englishName;
    QString filePath;
    QList<ManualSection> manualSections;
    QHash<QByteArray, QString> strings;
    bool loadedBody = false;
};

QString normalizeCode(const QString &code)
{
    QString c = code.trimmed().toLower();
    c.replace(QLatin1Char('-'), QLatin1Char('_'));
    return c;
}

class JsonMapTranslator : public QTranslator
{
public:
    explicit JsonMapTranslator(QHash<QByteArray, QString> map)
        : m_map(std::move(map))
    {
    }

    QString translate(const char * /*context*/,
                      const char *sourceText,
                      const char * /*disambiguation*/ = nullptr,
                      int /*n*/ = -1) const override
    {
        if (!sourceText) {
            return {};
        }
        const auto it = m_map.constFind(QByteArray::fromRawData(sourceText, static_cast<qsizetype>(std::strlen(sourceText))));
        if (it != m_map.constEnd()) {
            return it.value();
        }
        return {};
    }

private:
    QHash<QByteArray, QString> m_map;
};

QMap<QString, LanguagePack> &packRegistry()
{
    static QMap<QString, LanguagePack> registry;
    static bool initialized = false;
    if (initialized) {
        return registry;
    }
    initialized = true;

    auto scanDir = [&](const QString &dirPath) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            return;
        }
        const QStringList files = dir.entryList({u"*.json"_s}, QDir::Files, QDir::Name);
        for (const QString &fileName : files) {
            const QString fullPath = dir.filePath(fileName);
            QFile f(fullPath);
            if (!f.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (!doc.isObject()) {
                continue;
            }
            const QJsonObject root = doc.object();
            const QJsonObject meta = root.value(u"_meta"_s).toObject();
            QString code = normalizeCode(meta.value(u"code"_s).toString());
            if (code.isEmpty()) {
                code = normalizeCode(fileName.chopped(5));
            }
            if (code.isEmpty()) {
                continue;
            }

            LanguagePack pack;
            pack.info.code = code;
            pack.englishName = meta.value(u"name"_s).toString().trimmed();
            pack.info.nativeName = meta.value(u"nativeName"_s).toString().trimmed();
            if (pack.info.nativeName.isEmpty()) {
                pack.info.nativeName = pack.englishName;
            }
            if (pack.info.nativeName.isEmpty()) {
                pack.info.nativeName = code.toUpper();
            }
            pack.info.isSource = meta.value(u"isSource"_s).toBool(false);
            pack.filePath = fullPath;
            registry.insert(code, std::move(pack));
        }
    };

    // Priority order (later entries override earlier ones if a user adds custom translations)
    scanDir(u":/i18n"_s);
    scanDir(u"/usr/share/mklipper/i18n"_s);
    const QString userDataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!userDataDir.isEmpty()) {
        scanDir(userDataDir + u"/mklipper/i18n"_s);
    }

    return registry;
}

void ensurePackLoaded(LanguagePack &pack)
{
    if (pack.loadedBody) {
        return;
    }
    pack.loadedBody = true;

    QFile f(pack.filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject root = doc.object();
    const QJsonArray secArr = root.value(u"manualSections"_s).toArray();
    for (const QJsonValue &sv : secArr) {
        if (!sv.isObject()) {
            continue;
        }
        const QJsonObject so = sv.toObject();
        ManualSection sec;
        sec.icon = so.value(u"icon"_s).toString();
        sec.title = so.value(u"title"_s).toString();
        sec.intro = so.value(u"intro"_s).toString();
        const QJsonArray cardsArr = so.value(u"cards"_s).toArray();
        for (const QJsonValue &cv : cardsArr) {
            if (!cv.isObject()) {
                continue;
            }
            const QJsonObject co = cv.toObject();
            sec.cards.append({
                co.value(u"icon"_s).toString(),
                co.value(u"title"_s).toString(),
                co.value(u"body"_s).toString(),
            });
        }
        if (!sec.title.isEmpty()) {
            pack.manualSections.append(std::move(sec));
        }
    }

    const QJsonObject strObj = root.value(u"strings"_s).toObject();
    pack.strings.reserve(strObj.size());
    for (auto it = strObj.constBegin(); it != strObj.constEnd(); ++it) {
        if (it.value().isString()) {
            pack.strings.insert(it.key().toUtf8(), it.value().toString());
        }
    }
}

QString findSourceLanguageCode()
{
    const auto &reg = packRegistry();
    for (auto it = reg.constBegin(); it != reg.constEnd(); ++it) {
        if (it.value().info.isSource) {
            return it.key();
        }
    }
    return reg.contains(u"es"_s) ? u"es"_s : (reg.isEmpty() ? u"es"_s : reg.firstKey());
}

QString matchLocaleToPack(const QString &localeTag)
{
    const auto &reg = packRegistry();
    const QString norm = normalizeCode(localeTag);
    if (norm.isEmpty()) {
        return {};
    }
    if (reg.contains(norm)) {
        return norm;
    }
    const QString base = norm.section(QLatin1Char('_'), 0, 0);
    if (reg.contains(base)) {
        return base;
    }
    for (auto it = reg.constBegin(); it != reg.constEnd(); ++it) {
        if (it.key().section(QLatin1Char('_'), 0, 0) == base) {
            return it.key();
        }
    }
    return {};
}

std::unique_ptr<JsonMapTranslator> s_translator;
QString s_activeLang;
} // namespace

namespace MklipperTranslations
{
QList<LanguageInfo> availableLanguages()
{
    const auto &reg = packRegistry();
    QList<LanguageInfo> out;
    out.reserve(reg.size());
    for (auto it = reg.constBegin(); it != reg.constEnd(); ++it) {
        out.append(it.value().info);
    }
    std::sort(out.begin(), out.end(), [](const LanguageInfo &a, const LanguageInfo &b) {
        if (a.isSource != b.isSource) {
            return a.isSource;
        }
        return a.nativeName.localeAwareCompare(b.nativeName) < 0;
    });
    return out;
}

QString resolveLanguage(const QString &configured)
{
    const QString norm = normalizeCode(configured);
    if (!norm.isEmpty() && norm != u"system"_s) {
        const QString matched = matchLocaleToPack(norm);
        if (!matched.isEmpty()) {
            return matched;
        }
    }

    // Match system UI languages against available JSON packs dynamically
    const QLocale sysLocale = QLocale::system();
    const QStringList uiLangs = sysLocale.uiLanguages();
    for (const QString &lang : uiLangs) {
        const QString matched = matchLocaleToPack(lang);
        if (!matched.isEmpty()) {
            return matched;
        }
    }
    const QString fallbackLocaleMatch = matchLocaleToPack(sysLocale.name());
    if (!fallbackLocaleMatch.isEmpty()) {
        return fallbackLocaleMatch;
    }

    // Default fallback for locales without a dedicated pack yet: English if available, otherwise source language
    const auto &reg = packRegistry();
    if (reg.contains(u"en"_s)) {
        return u"en"_s;
    }
    return findSourceLanguageCode();
}

QString activeLanguage()
{
    if (s_activeLang.isEmpty()) {
        s_activeLang = findSourceLanguageCode();
    }
    return s_activeLang;
}

bool applyLanguage(const QString &configured)
{
    const QString target = resolveLanguage(configured);
    const QString previous = activeLanguage();
    const bool changed = (target != previous);

    if (auto *app = QCoreApplication::instance()) {
        if (s_translator) {
            app->removeTranslator(s_translator.get());
            s_translator.reset();
        }

        auto &reg = packRegistry();
        auto it = reg.find(target);
        if (it != reg.end() && !it.value().info.isSource) {
            ensurePackLoaded(it.value());
            if (!it.value().strings.isEmpty()) {
                s_translator = std::make_unique<JsonMapTranslator>(it.value().strings);
                app->installTranslator(s_translator.get());
            }
        }
    }

    s_activeLang = target;
    return changed;
}

QList<ManualSection> userManualSections()
{
    auto &reg = packRegistry();
    const QString active = activeLanguage();
    if (auto it = reg.find(active); it != reg.end()) {
        ensurePackLoaded(it.value());
        if (!it.value().manualSections.isEmpty()) {
            return it.value().manualSections;
        }
    }
    // Fallback to English, then source language
    for (const QString &fb : {u"en"_s, findSourceLanguageCode()}) {
        if (auto it = reg.find(fb); it != reg.end()) {
            ensurePackLoaded(it.value());
            if (!it.value().manualSections.isEmpty()) {
                return it.value().manualSections;
            }
        }
    }
    return {};
}

QString filterReleaseNotesForLanguage(const QString &markdown, const QString &langCode)
{
    const QString trimmedInput = markdown.trimmed();
    if (trimmedInput.isEmpty()) {
        return {};
    }

    auto foldForMatch = [](const QString &text) -> QString {
        const QString decomposed = text.normalized(QString::NormalizationForm_D).toLower();
        QString out;
        out.reserve(decomposed.size());
        for (const QChar ch : decomposed) {
            if (ch.category() == QChar::Mark_NonSpacing || ch.category() == QChar::Mark_SpacingCombining) {
                continue;
            }
            if (ch.isLetterOrNumber()) {
                out.append(ch);
            } else if (ch == QLatin1Char('_') || ch == QLatin1Char('-') || ch.isSpace()) {
                if (!out.isEmpty() && out.back() != QLatin1Char(' ')) {
                    out.append(QLatin1Char(' '));
                }
            }
        }
        return out.trimmed();
    };

    const auto &reg = packRegistry();
    auto matchHeadingToLang = [&](const QString &headingLine) -> QString {
        QString h = headingLine.trimmed();
        if (!h.startsWith(u"### "_s) && !h.startsWith(u"## "_s)) {
            return {};
        }
        while (h.startsWith(QLatin1Char('#'))) {
            h.remove(0, 1);
        }
        const QString foldedHeading = foldForMatch(h);
        if (foldedHeading.isEmpty()) {
            return {};
        }

        const QString firstToken = foldedHeading.section(QLatin1Char(' '), 0, 0);
        for (auto it = reg.constBegin(); it != reg.constEnd(); ++it) {
            const LanguagePack &pack = it.value();
            const QString foldedCode = foldForMatch(pack.info.code);
            const QString foldedNative = foldForMatch(pack.info.nativeName);
            const QString foldedEnglish = foldForMatch(pack.englishName);
            const QString nativeFirst = foldedNative.section(QLatin1Char(' '), 0, 0);
            const QString englishFirst = foldedEnglish.section(QLatin1Char(' '), 0, 0);

            if (foldedHeading == foldedCode
                || foldedHeading == foldedNative
                || (!foldedEnglish.isEmpty() && foldedHeading == foldedEnglish)
                || (!nativeFirst.isEmpty() && firstToken == nativeFirst)
                || (!englishFirst.isEmpty() && firstToken == englishFirst)) {
                return it.key();
            }
        }
        return {};
    };

    QStringList preambleLines;
    QMap<QString, QStringList> sections;
    QStringList sectionOrder;
    QString currentLang;

    const QStringList lines = trimmedInput.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString detectedLang = matchHeadingToLang(line);
        if (!detectedLang.isEmpty()) {
            currentLang = detectedLang;
            if (!sections.contains(currentLang)) {
                sectionOrder.append(currentLang);
            }
            continue;
        }
        if (currentLang.isEmpty()) {
            preambleLines.append(line);
        } else {
            sections[currentLang].append(line);
        }
    }

    if (sections.isEmpty()) {
        return trimmedInput;
    }

    const QString target = langCode.isEmpty() ? activeLanguage() : resolveLanguage(langCode);
    QString chosen;
    if (sections.contains(target)) {
        chosen = target;
    } else {
        const QString base = target.section(QLatin1Char('_'), 0, 0);
        for (const QString &k : sectionOrder) {
            if (k.section(QLatin1Char('_'), 0, 0) == base) {
                chosen = k;
                break;
            }
        }
    }
    if (chosen.isEmpty() && sections.contains(u"en"_s)) {
        chosen = u"en"_s;
    }
    if (chosen.isEmpty()) {
        const QString src = findSourceLanguageCode();
        if (sections.contains(src)) {
            chosen = src;
        }
    }
    if (chosen.isEmpty() && !sectionOrder.isEmpty()) {
        chosen = sectionOrder.first();
    }

    const QString body = sections.value(chosen).join(QLatin1Char('\n')).trimmed();
    const QString preamble = preambleLines.join(QLatin1Char('\n')).trimmed();
    if (!preamble.isEmpty() && !body.isEmpty()) {
        return preamble + u"\n\n"_s + body;
    }
    return body.isEmpty() ? trimmedInput : body;
}
} // namespace MklipperTranslations
