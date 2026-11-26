/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "historymodel.h"

#include "mimetypes.h"
#include "settings.h"
#include "urlgrabber.h"

#include <QBuffer>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMimeData>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <array>
#include <list>

using namespace Qt::StringLiterals;

namespace
{
struct TransactionGuard {
    explicit TransactionGuard(QSqlDatabase *database)
        : db(database)
        , inited(database->transaction())
    {
    }

    ~TransactionGuard()
    {
        if (!inited) {
            return;
        }
        if (committed) {
            db->commit();
        } else {
            db->rollback();
        }
    }

    bool exec(QSqlQuery &query)
    {
        if (!inited || !committed) {
            return false;
        }
        committed = query.exec();
        if (!committed) {
            qWarning() << "Mklipper SQL query failed:" << query.lastQuery() << query.lastError().text();
        }
        return committed;
    }

    bool exec(const QString &sql)
    {
        QSqlQuery q(*db);
        q.prepare(sql);
        return exec(q);
    }

    QSqlDatabase *db;
    bool inited = false;
    bool committed = true;
};

struct StoredMimeEntry {
    QString type;
    QByteArray data;
    QString dataUuid;
};

bool hasAcceptableTextFormatPrefix(const QString &format)
{
    return !format.startsWith(MimeTypes::plainText)
        && (format.startsWith(MimeTypes::textPrefix)
            || format.startsWith(MimeTypes::json)
            || format.startsWith(MimeTypes::xml));
}

QString computeUuid(const QMimeData *data)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (data->hasText()) {
        hash.addData(data->text().toUtf8());
    }
    if (data->hasUrls()) {
        const auto urls = data->urls();
        for (const QUrl &url : urls) {
            hash.addData(url.toEncoded());
        }
    }
    if (data->hasImage()) {
        const QImage image = data->imageData().value<QImage>();
        if (!image.isNull()) {
            hash.addData(QByteArrayView(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()));
        }
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString computeUuid(const QString &text)
{
    QMimeData data;
    data.setText(text);
    return computeUuid(&data);
}

QString textFromMimeData(const QMimeData *mimeData)
{
    if (!mimeData->hasUrls()) {
        return mimeData->text();
    }
    QStringList urlText;
    const auto urls = mimeData->urls();
    for (const QUrl &url : urls) {
        urlText.append(url.toString(QUrl::FullyEncoded));
    }
    return urlText.join(u' ');
}

constexpr qsizetype kMaxClipboardPayloadBytes = 20 * 1000 * 1000;

QString imageMimeTypesSqlList()
{
    const auto supported = QImageReader::supportedMimeTypes();
    if (supported.isEmpty()) {
        return u"'image/png'"_s;
    }
    QStringList quoted;
    quoted.reserve(supported.size());
    for (const QByteArray &m : supported) {
        bool safe = !m.isEmpty();
        for (const char ch : m) {
            const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                || (ch >= '0' && ch <= '9') || ch == '/' || ch == '+' || ch == '-' || ch == '.';
            if (!ok) {
                safe = false;
                break;
            }
        }
        if (safe) {
            quoted.append(u'\'' + QString::fromLatin1(m) + u'\'');
        }
    }
    return quoted.isEmpty() ? u"'image/png'"_s : quoted.join(u", "_s);
}

bool isSecretMimeData(const QMimeData *data)
{
    if (!data) {
        return false;
    }
    if (data->data(u"x-kde-passwordManagerHint"_s) == QByteArrayView("secret")) {
        return true;
    }
    if (data->hasFormat(u"application/x-keepassxc-secret"_s)
        || data->hasFormat(u"application/x-bitwarden-secret"_s)
        || data->hasFormat(u"application/x-1password-secret"_s)
        || data->hasFormat(u"application/x-gnome-password"_s)
        || data->hasFormat(u"x-gnome-password"_s)
        || data->hasFormat(u"ExcludeClipboardContentFromMonitorProcessing"_s)) {
        return true;
    }
    if (data->hasFormat(u"CanIncludeInClipboardHistory"_s)) {
        const QByteArray v = data->data(u"CanIncludeInClipboardHistory"_s);
        if (v == QByteArrayView("0") || (v.size() == 4 && v[0] == '\0' && v[1] == '\0' && v[2] == '\0' && v[3] == '\0')) {
            return true;
        }
    }
    return false;
}

bool hasMeaningfulClipboardContent(const QMimeData *data)
{
    if (!data) {
        return false;
    }
    if (data->hasUrls()) {
        const auto urls = data->urls();
        for (const QUrl &url : urls) {
            if (url.isValid() && !url.isEmpty()) {
                return true;
            }
        }
    }
    if (data->hasImage()) {
        const QImage img = data->imageData().value<QImage>();
        if (!img.isNull()) {
            return true;
        }
        const QStringList fmts = data->formats();
        for (const QString &fmt : fmts) {
            if (fmt.startsWith(MimeTypes::imagePrefix) && !data->data(fmt).isEmpty()) {
                return true;
            }
        }
    }
    if (data->hasText()) {
        if (!data->text().trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}
} // namespace

std::shared_ptr<HistoryModel> HistoryModel::self()
{
    static std::weak_ptr<HistoryModel> instance;
    if (instance.expired()) {
        struct make_shared_enabler : public HistoryModel {
        };
        std::shared_ptr<HistoryModel> ptr = std::make_shared<make_shared_enabler>();
        instance = ptr;
        return ptr;
    }
    return instance.lock();
}

HistoryModel::HistoryModel()
    : QAbstractListModel(nullptr)
    , m_clip(SystemClipboard::self())
{
    if (!QSqlDatabase::isDriverAvailable(u"QSQLITE"_s)) {
        qCritical() << "Mklipper: QSQLITE driver is not available!";
        return;
    }

    loadSettings();
    if (!loadHistory()) {
        return;
    }

    auto modelChanged = [this](const QModelIndex &, int first, int) {
        Q_EMIT changed(first == 0);
    };
    connect(this, &HistoryModel::rowsInserted, this, modelChanged);
    connect(this, &HistoryModel::rowsRemoved, this, modelChanged);
    connect(this, &HistoryModel::rowsMoved, this,
            [this](const QModelIndex &, int sourceStart, int, const QModelIndex &, int destinationRow) {
                Q_EMIT changed(sourceStart == 0 || destinationRow == 0);
            });
    connect(this, &HistoryModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &, const QList<int> &roles) {
        if (roles.contains(UuidRole)) {
            Q_EMIT changed(topLeft.row() == 0);
        }
    });
    connect(this, &HistoryModel::modelReset, this, [this] {
        Q_EMIT changed(true);
    });

    connect(this, &HistoryModel::changed, this, [this](bool isTop) {
        const std::optional<bool> plainOverride = m_nextPlainTextOverride;
        m_nextPlainTextOverride.reset();

        const SystemClipboard::SelectionMode buffersWithoutPassword = buffersHolding(Holding::NoPassword);
        if (buffersWithoutPassword == 0) {
            return;
        }
        if (m_items.isEmpty()) {
            m_clip->clear(buffersWithoutPassword);
            return;
        }
        if (!isTop) {
            return;
        }

        if (m_clip->isLocked(QClipboard::Selection) || m_clip->isLocked(QClipboard::Clipboard)) {
            return;
        }
        m_clip->setMimeData(m_items[0],
                            buffersWithoutPassword,
                            SystemClipboard::ClipboardUpdateReason::UpdateClipboard,
                            plainOverride);
    });

    connect(m_clip.get(), &SystemClipboard::newClipData, this, &HistoryModel::checkClipData);
}

HistoryModel::~HistoryModel()
{
    if (!m_bKeepContents && m_db.isOpen()) {
        const QString dbName = m_db.databaseName();
        m_db.close();
        QFile::remove(dbName);
        QFile::remove(dbName + u"-wal"_s);
        QFile::remove(dbName + u"-shm"_s);
        QDir(m_dbFolder + u"/data"_s).removeRecursively();
    }
}

void HistoryModel::clear()
{
    if (!m_db.isOpen()) {
        return;
    }
    {
        TransactionGuard tx(&m_db);
        if (!tx.exec(u"DELETE FROM main"_s) || !tx.exec(u"DELETE FROM aux"_s)) {
            return;
        }
    }
    QDir dataDir(m_dbFolder + u"/data"_s);
    if (dataDir.exists()) {
        dataDir.removeRecursively();
        dataDir.mkpath(dataDir.absolutePath());
    }
    reclaimDiskSpace(true);
    enforcePrivateStoragePermissions();

    if (!m_items.isEmpty()) {
        beginResetModel();
        m_items.clear();
        m_starredCache.clear();
        m_starredCount = 0;
        endResetModel();
        Q_EMIT starredCountChanged(0);
    }
    clearBuffers();
}

void HistoryModel::clearNonStarredHistory()
{
    if (!m_db.isOpen()) {
        return;
    }

    QStringList nonStarredUuids;
    for (const auto &item : std::as_const(m_items)) {
        if (!isItemStarred(item->uuid())) {
            nonStarredUuids.append(item->uuid());
        }
    }

    if (nonStarredUuids.isEmpty()) {
        clearPasswords();
        return;
    }

    {
        TransactionGuard tx(&m_db);
        for (const QString &uuid : std::as_const(nonStarredUuids)) {
            QSqlQuery q1(m_db);
            q1.prepare(u"DELETE FROM main WHERE uuid = ?"_s);
            q1.addBindValue(uuid);
            QSqlQuery q2(m_db);
            q2.prepare(u"DELETE FROM aux WHERE uuid = ?"_s);
            q2.addBindValue(uuid);
            if (!tx.exec(q1) || !tx.exec(q2)) {
                return;
            }
        }
    }

    for (const QString &uuid : std::as_const(nonStarredUuids)) {
        removeItemDataDir(uuid);
        m_starredCache.remove(uuid);
    }

    clearPasswords();

    beginResetModel();
    const QSet<QString> removedSet(nonStarredUuids.cbegin(), nonStarredUuids.cend());
    for (qsizetype i = m_items.size() - 1; i >= 0; --i) {
        if (removedSet.contains(m_items[i]->uuid())) {
            m_items.removeAt(i);
        }
    }
    endResetModel();

    reclaimDiskSpace(true);
}

void HistoryModel::clearHistoryInteractive(QWidget *dialogParent)
{
    auto *settings = MklipperSettings::self();
    if (!settings->confirmClearHistory()) {
        clearNonStarredHistory();
        return;
    }

    QMessageBox box(dialogParent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Vaciar historial del portapapeles"));
    box.setText(tr("¿Seguro que quieres vaciar el historial del portapapeles?\n(Los elementos fijados se conservarán)."));
    auto *dontAsk = new QCheckBox(tr("No volver a preguntar"), &box);
    box.setCheckBox(dontAsk);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    box.button(QMessageBox::Yes)->setText(tr("Vaciar"));
    box.button(QMessageBox::Cancel)->setText(tr("Cancelar"));
    box.setDefaultButton(QMessageBox::Cancel);

    if (box.exec() == QMessageBox::Yes) {
        if (dontAsk->isChecked()) {
            settings->setConfirmClearHistory(false);
            settings->save();
        }
        clearNonStarredHistory();
    }
}

void HistoryModel::clearHasPassword()
{
    setHasPassword(QClipboard::Clipboard, false);
    setHasPassword(QClipboard::Selection, false);
}

SystemClipboard::SelectionMode HistoryModel::buffersHolding(Holding holding) const
{
    const bool password = (holding == Holding::APassword);
    return SystemClipboard::SelectionMode((m_clipboardHasPassword == password ? SystemClipboard::Clipboard : 0)
                                          | (m_selectionHasPassword == password ? SystemClipboard::Selection : 0));
}

void HistoryModel::clearPasswords()
{
    m_clip->clear(buffersHolding(Holding::APassword));
    clearHasPassword();
}

void HistoryModel::clearBuffers()
{
    m_clip->clear();
    clearHasPassword();
}

void HistoryModel::setMaxSize(qsizetype size)
{
    if (m_maxSize == size) {
        return;
    }
    m_maxSize = size;
    if (m_items.size() > m_maxSize) {
        qsizetype itemsToRemove = m_items.size() - m_maxSize;
        qsizetype removedCount = 0;
        for (qsizetype i = m_items.size() - 1; i >= 0 && removedCount < itemsToRemove; --i) {
            if (!isItemStarred(m_items[i]->uuid())) {
                removeRow(static_cast<int>(i));
                ++removedCount;
            }
        }
    }
}

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size() || index.column() != 0) {
        return {};
    }

    const HistoryItemPtr &item = m_items.at(index.row());

    auto queryImagePath = [this, &item]() -> QString {
        if (!m_db.isOpen() || !HistoryItem::isValidUuid(item->uuid())) {
            return {};
        }
        QSqlQuery query(m_db);
        query.prepare(u"SELECT data_uuid FROM aux WHERE uuid = ? AND mimetype IN (%1) ORDER BY rowid ASC LIMIT 1"_s.arg(imageMimeTypesSqlList()));
        query.addBindValue(item->uuid());
        if (query.exec() && query.next()) {
            const QString dataUuid = query.value(0).toString();
            if (!HistoryItem::isValidUuid(dataUuid)) {
                return {};
            }
            return m_dbFolder + u"/data/"_s + item->uuid() + u'/' + dataUuid;
        }
        return {};
    };

    switch (role) {
    case Qt::DisplayRole: {
        if (item->type() == HistoryItemType::Image) {
            const QString path = queryImagePath();
            if (!path.isEmpty()) {
                const QSize size = QImageReader(path).size();
                if (size.isValid()) {
                    return tr("Imagen %1×%2").arg(size.width()).arg(size.height());
                }
            }
            return tr("Imagen");
        }
        return item->text();
    }
    case ImageUrlRole: {
        if (!(item->allTypes() & HistoryItemType::Image)) {
            return QUrl();
        }
        const QString path = queryImagePath();
        return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
    }
    case ImageSizeRole: {
        if (!(item->allTypes() & HistoryItemType::Image)) {
            return QSize();
        }
        const QString path = queryImagePath();
        return path.isEmpty() ? QSize() : QImageReader(path).size();
    }
    case HistoryItemConstPtrRole:
        return QVariant::fromValue<HistoryItemConstPtr>(std::const_pointer_cast<const HistoryItem>(item));
    case UuidRole:
        return item->uuid();
    case TypeRole:
        return QVariant::fromValue<HistoryItemType>(item->type());
    case TypeIntRole:
        return static_cast<int>(item->type());
    case StarredRole:
        return isItemStarred(item->uuid());
    case HasActionRole:
        return m_urlGrabber && m_urlGrabber->hasMatchingActions(item);
    case SourceAppRole:
        return item->sourceApp();
    case SourceTitleRole:
        return item->sourceTitle();
    case SourceUrlRole:
        return item->sourceUrl();
    case AddedTimeRole:
        return item->addedTime();
    case LastUsedTimeRole:
        return item->lastUsedTime();
    default:
        return {};
    }
}

bool HistoryModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!m_db.isOpen() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    auto &item = m_items[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole: {
        if (item->type() != HistoryItemType::Text || !value.canConvert<QString>()) {
            return false;
        }
        QString text = value.toString();
        if (text == item->text()) {
            return false;
        }
        const QStringList mimetypes{u"text/plain"_s, u"text/plain;charset=utf-8"_s};
        QString newUuid = computeUuid(text);
        const int existingRow = indexOf(newUuid);
        if (existingRow >= 0) {
            if (isItemStarred(item->uuid())) {
                setData(this->index(existingRow), true, StarredRole);
            }
            if (!removeRow(index.row())) {
                return false;
            }
            moveToTop(newUuid);
            return true;
        }

        const QString oldUuid = item->uuid();
        const bool wasStarred = isItemStarred(oldUuid);
        {
            TransactionGuard tx(&m_db);
            QSqlQuery upd(m_db);
            upd.prepare(u"UPDATE main SET uuid = ?, mimetypes = ?, text = ? WHERE uuid = ?"_s);
            upd.addBindValue(newUuid);
            upd.addBindValue(mimetypes.join(u','));
            upd.addBindValue(text);
            upd.addBindValue(oldUuid);

            QSqlQuery delAux(m_db);
            delAux.prepare(u"DELETE FROM aux WHERE uuid = ?"_s);
            delAux.addBindValue(oldUuid);

            if (!tx.exec(upd) || !tx.exec(delAux)) {
                return false;
            }

            for (const QString &mt : mimetypes) {
                QSqlQuery insAux(m_db);
                insAux.prepare(u"INSERT INTO aux (uuid, mimetype, data_uuid) VALUES (?, ?, ?)"_s);
                insAux.addBindValue(newUuid);
                insAux.addBindValue(mt);
                insAux.addBindValue(newUuid);
                if (!tx.exec(insAux)) {
                    return false;
                }
            }
        }

        removeItemDataDir(oldUuid);
        saveToFile(m_dbFolder, text.toUtf8(), newUuid, newUuid);
        reclaimDiskSpace(false);
        m_starredCache.remove(oldUuid);
        if (wasStarred) {
            m_starredCache.insert(newUuid, true);
        }

        const QString preservedApp = item->sourceApp();
        const QString preservedTitle = item->sourceTitle();
        const QString preservedUrl = item->sourceUrl();
        const qreal preservedAdded = item->addedTime();
        const qreal nowTs = QDateTime::currentMSecsSinceEpoch() / 1000.0;
        item = std::make_shared<HistoryItem>(std::move(newUuid),
                                             mimetypes,
                                             std::move(text),
                                             preservedApp,
                                             preservedTitle,
                                             preservedUrl,
                                             preservedAdded > 0.0 ? preservedAdded : nowTs,
                                             nowTs);
        Q_EMIT dataChanged(index, index, {Qt::DisplayRole, UuidRole});
        moveToTop(item->uuid());
        return true;
    }
    case StarredRole: {
        const bool newValue = value.toBool();
        if (newValue == isItemStarred(item->uuid())) {
            return false;
        }
        QSqlQuery query(m_db);
        query.prepare(u"UPDATE main SET starred = ? WHERE uuid = ?"_s);
        query.addBindValue(newValue ? 1 : 0);
        query.addBindValue(item->uuid());
        if (query.exec()) {
            m_starredCache.insert(item->uuid(), newValue);
            m_starredCount += (newValue ? 1 : -1);
            Q_EMIT dataChanged(index, index, {StarredRole});
            Q_EMIT starredCountChanged(m_starredCount);
            return true;
        }
        break;
    }
    default:
        break;
    }
    return false;
}

bool HistoryModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row < 0 || count <= 0 || static_cast<qsizetype>(row + count) > m_items.size()) {
        return false;
    }

    int removedStarred = 0;
    QStringList uuidsToRemove;
    uuidsToRemove.reserve(count);
    for (int i = 0; i < count; ++i) {
        const QString &uuid = m_items[row + i]->uuid();
        uuidsToRemove.append(uuid);
        if (isItemStarred(uuid)) {
            ++removedStarred;
        }
    }

    {
        TransactionGuard tx(&m_db);
        for (const QString &uuid : std::as_const(uuidsToRemove)) {
            QSqlQuery q1(m_db);
            q1.prepare(u"DELETE FROM main WHERE uuid = ?"_s);
            q1.addBindValue(uuid);
            QSqlQuery q2(m_db);
            q2.prepare(u"DELETE FROM aux WHERE uuid = ?"_s);
            q2.addBindValue(uuid);
            if (!tx.exec(q1) || !tx.exec(q2)) {
                return false;
            }
        }
    }

    for (const QString &uuid : std::as_const(uuidsToRemove)) {
        removeItemDataDir(uuid);
        m_starredCache.remove(uuid);
    }

    reclaimDiskSpace(false);

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    m_items.remove(row, count);
    m_starredCount = qMax(0, m_starredCount - removedStarred);
    endRemoveRows();
    if (removedStarred > 0) {
        Q_EMIT starredCountChanged(m_starredCount);
    }
    return true;
}

bool HistoryModel::remove(const QString &uuid, bool askConfirmation, QWidget *dialogParent)
{
    const int idx = indexOf(uuid);
    if (idx < 0) {
        return false;
    }

    if (!isItemStarred(uuid) || !askConfirmation || !MklipperSettings::self()->confirmRemoveStarred()) {
        return removeRow(idx);
    }

    QMessageBox box(dialogParent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Eliminar elemento fijado"));
    box.setText(tr("Este elemento está fijado. ¿Seguro que quieres eliminarlo del historial?"));
    auto *dontAsk = new QCheckBox(tr("No volver a preguntar"), &box);
    box.setCheckBox(dontAsk);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    box.button(QMessageBox::Yes)->setText(tr("Eliminar"));
    box.button(QMessageBox::Cancel)->setText(tr("Cancelar"));
    box.setDefaultButton(QMessageBox::Cancel);

    if (box.exec() != QMessageBox::Yes) {
        return false;
    }
    if (dontAsk->isChecked()) {
        MklipperSettings::self()->setConfirmRemoveStarred(false);
        MklipperSettings::self()->save();
    }
    const int updatedIdx = indexOf(uuid);
    return updatedIdx >= 0 && removeRow(updatedIdx);
}

int HistoryModel::indexOf(const QString &uuid) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i]->uuid() == uuid) {
            return i;
        }
    }
    return -1;
}

int HistoryModel::indexOf(const HistoryItem *item) const
{
    return item ? indexOf(item->uuid()) : -1;
}

HistoryItemConstPtr HistoryModel::first() const
{
    return m_items.isEmpty() ? HistoryItemConstPtr{} : m_items.first();
}

bool HistoryModel::persistMimeData(const QString &uuid,
                                   const QString &text,
                                   const QMimeData *mimeData,
                                   qreal timestamp,
                                   const ClipboardSourceInfo &source)
{
    if (!m_db.isOpen()) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    std::list<StoredMimeEntry> entries;
    const QStringList formats = mimeData->formats();

    const int plainTextIdx = formats.indexOf(MimeTypes::plainText);
    const int plainUtf8Idx = formats.indexOf(MimeTypes::plainUtf8Text);
    const int uriListIdx = formats.indexOf(MimeTypes::uriList);
    const int pngIdx = formats.indexOf(MimeTypes::png);
    const int qtImageIdx = formats.indexOf(MimeTypes::qtImage);
    const int suggestedIdx = formats.indexOf(MimeTypes::kdeSuggestedFileName);

    bool isUriListSet = false;
    bool isPlainTextSet = false;
    bool isPngSet = false;
    const bool usePng = (pngIdx >= 0);

    for (int i = 0; i < formats.size(); ++i) {
        const QString &format = formats[i];
        if (!format.contains(u'/')) {
            continue;
        }

        if (i == uriListIdx) {
            if (isUriListSet) {
                continue;
            }
            isUriListSet = true;
            QByteArray bytes = mimeData->data(format);
            if (bytes.size() > kMaxClipboardPayloadBytes) {
                continue;
            }
            hash.reset();
            hash.addData(bytes);
            entries.push_back({format, std::move(bytes), QString::fromLatin1(hash.result().toHex())});
            continue;
        }

        if (i == plainTextIdx || i == plainUtf8Idx) {
            if (isPlainTextSet) {
                continue;
            }
            isPlainTextSet = true;
            QByteArray bytes = mimeData->text().toUtf8();
            if (bytes.size() > kMaxClipboardPayloadBytes) {
                continue;
            }
            hash.reset();
            hash.addData(bytes);
            const QString dataUuid = QString::fromLatin1(hash.result().toHex());
            entries.push_back({QString(MimeTypes::plainText), std::move(bytes), dataUuid});
            entries.push_back({QString(MimeTypes::plainUtf8Text), QByteArray(), dataUuid});
            continue;
        }

        if (i == qtImageIdx || format.startsWith(MimeTypes::imagePrefix)) {
            const int generatedMarker = (qtImageIdx >= 0) ? qtImageIdx : pngIdx;
            if (generatedMarker >= 0 && i > generatedMarker && i != pngIdx) {
                continue;
            }
            const bool isPngOrQt = (i == pngIdx || i == qtImageIdx);
            if (!isPngOrQt) {
                QByteArray bytes = mimeData->data(format);
                if (!bytes.isEmpty() && bytes.size() <= kMaxClipboardPayloadBytes) {
                    hash.reset();
                    hash.addData(bytes);
                    entries.push_back({format, std::move(bytes), QString::fromLatin1(hash.result().toHex())});
                }
            } else if (!isPngSet) {
                isPngSet = true;
                QByteArray bytes;
                QImage image;
                if (usePng) {
                    bytes = mimeData->data(MimeTypes::png);
                    if (bytes.size() <= kMaxClipboardPayloadBytes) {
                        image = QImage::fromData(bytes, "PNG");
                    }
                } else {
                    image = mimeData->imageData().value<QImage>();
                    if (!image.isNull()) {
                        QBuffer buffer(&bytes);
                        buffer.open(QIODevice::WriteOnly);
                        QImageWriter encoder(&buffer, "PNG");
                        encoder.write(image);
                    }
                }
                if (!image.isNull() && !bytes.isEmpty() && bytes.size() <= kMaxClipboardPayloadBytes) {
                    hash.reset();
                    hash.addData(QByteArrayView(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()));
                    entries.push_back({QString(MimeTypes::png), std::move(bytes), QString::fromLatin1(hash.result().toHex())});
                }
            }
            continue;
        }

        if (i != suggestedIdx && !hasAcceptableTextFormatPrefix(format)) {
            continue;
        }

        QByteArray bytes = mimeData->data(format);
        if (bytes.size() > kMaxClipboardPayloadBytes) {
            continue;
        }
        hash.reset();
        hash.addData(bytes);
        entries.push_back({format, std::move(bytes), QString::fromLatin1(hash.result().toHex())});
    }

    if (!isPlainTextSet && isUriListSet) {
        QByteArray bytes = mimeData->text().toUtf8();
        if (bytes.size() <= kMaxClipboardPayloadBytes) {
            hash.reset();
            hash.addData(bytes);
            const QString dataUuid = QString::fromLatin1(hash.result().toHex());
            entries.push_back({QString(MimeTypes::plainText), std::move(bytes), dataUuid});
            entries.push_back({QString(MimeTypes::plainUtf8Text), QByteArray(), dataUuid});
        }
    }

    if (entries.empty()) {
        return false;
    }

    QStringList savedMimeTypes;
    savedMimeTypes.reserve(static_cast<int>(entries.size()));
    for (const auto &e : entries) {
        savedMimeTypes.append(e.type);
    }

    const qreal ts = (timestamp == 0) ? (QDateTime::currentMSecsSinceEpoch() / 1000.0) : timestamp;
    {
        TransactionGuard tx(&m_db);
        QSqlQuery insMain(m_db);
        insMain.prepare(
            u"INSERT OR REPLACE INTO main (uuid, added_time, last_used_time, mimetypes, text, starred, source_app, source_title, source_url) "
            u"VALUES (?, ?, ?, ?, ?, 0, ?, ?, ?)"_s);
        insMain.addBindValue(uuid);
        insMain.addBindValue(ts);
        insMain.addBindValue(ts);
        insMain.addBindValue(savedMimeTypes.join(u','));
        insMain.addBindValue(text);
        insMain.addBindValue(source.app);
        insMain.addBindValue(source.title);
        insMain.addBindValue(source.url);
        if (!tx.exec(insMain)) {
            return false;
        }

        for (const auto &e : entries) {
            QSqlQuery insAux(m_db);
            insAux.prepare(u"INSERT OR REPLACE INTO aux (uuid, mimetype, data_uuid) VALUES (?, ?, ?)"_s);
            insAux.addBindValue(uuid);
            insAux.addBindValue(e.type);
            insAux.addBindValue(e.dataUuid);
            if (!tx.exec(insAux)) {
                return false;
            }
        }
    }

    QSet<QString> writtenFiles;
    for (const auto &e : entries) {
        if (!writtenFiles.contains(e.dataUuid)) {
            writtenFiles.insert(e.dataUuid);
            if (!saveToFile(m_dbFolder, e.data, uuid, e.dataUuid)) {
                {
                    TransactionGuard tx(&m_db);
                    QSqlQuery delMain(m_db);
                    delMain.prepare(u"DELETE FROM main WHERE uuid = ?"_s);
                    delMain.addBindValue(uuid);
                    QSqlQuery delAux(m_db);
                    delAux.prepare(u"DELETE FROM aux WHERE uuid = ?"_s);
                    delAux.addBindValue(uuid);
                    tx.exec(delMain);
                    tx.exec(delAux);
                }
                removeItemDataDir(uuid);
                return false;
            }
        }
    }

    return true;
}

bool HistoryModel::insert(const QMimeData *mimeData, qreal timestamp, const ClipboardSourceInfo &source)
{
    if (m_maxSize == 0 || !mimeData || !hasMeaningfulClipboardContent(mimeData)) {
        return false;
    }
    const QString uuid = computeUuid(mimeData);
    if (!HistoryItem::isValidUuid(uuid)) {
        return false;
    }
    if (const int existingIdx = indexOf(uuid); existingIdx >= 0) {
        if (!source.isEmpty()) {
            auto &existing = m_items[existingIdx];
            if (!existing->hasSourceInfo() || (!source.url.isEmpty() && existing->sourceUrl().isEmpty())) {
                QSqlQuery updSrc(m_db);
                updSrc.prepare(u"UPDATE main SET source_app = ?, source_title = ?, source_url = ? WHERE uuid = ?"_s);
                updSrc.addBindValue(source.app);
                updSrc.addBindValue(source.title);
                updSrc.addBindValue(source.url);
                updSrc.addBindValue(uuid);
                if (updSrc.exec()) {
                    existing->setSourceInfo(source.app, source.title, source.url);
                    const QModelIndex idx = this->index(existingIdx);
                    Q_EMIT dataChanged(idx, idx, {SourceAppRole, SourceTitleRole, SourceUrlRole});
                }
            }
        }
        if (existingIdx > 0) {
            moveToTop(existingIdx);
        }
        return true;
    }

    const QStringList formats = mimeData->formats();
    if (formats.isEmpty() || formats.size() > 64) {
        return false;
    }

    const QString text = textFromMimeData(mimeData);
    if (text.toUtf8().size() > kMaxClipboardPayloadBytes) {
        return false;
    }
    const qreal ts = (timestamp == 0) ? (QDateTime::currentMSecsSinceEpoch() / 1000.0) : timestamp;
    if (!persistMimeData(uuid, text, mimeData, ts, source)) {
        return false;
    }

    auto item = std::make_shared<HistoryItem>(uuid, formats, text, source.app, source.title, source.url, ts, ts);
    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(std::move(item));
    endInsertRows();

    while (m_items.size() > m_maxSize) {
        int itemToRemove = -1;
        for (qsizetype i = m_items.size() - 1; i >= 0; --i) {
            if (!isItemStarred(m_items[i]->uuid())) {
                itemToRemove = static_cast<int>(i);
                break;
            }
        }
        if (itemToRemove < 0) {
            break;
        }
        removeRow(itemToRemove);
    }

    return true;
}

bool HistoryModel::insert(const QString &text)
{
    auto data = std::make_unique<QMimeData>();
    data->setText(text);
    return insert(data.get());
}

bool HistoryModel::loadHistory()
{
    if (qEnvironmentVariableIsSet("MKLIPPER_DATABASE")) {
        m_dbFolder = QFileInfo(qEnvironmentVariable("MKLIPPER_DATABASE")).absoluteDir().absolutePath();
    } else {
        m_dbFolder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/mklipper"_s;
    }
    m_clip->setDataFolder(m_dbFolder);

    QDir dataDir(m_dbFolder + u"/data"_s);
    dataDir.mkpath(dataDir.absolutePath());

    m_db = QSqlDatabase::database(u"mklipper"_s);
    if (!m_db.isValid()) {
        m_db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"mklipper"_s);
        if (qEnvironmentVariableIsSet("MKLIPPER_DATABASE")) {
            m_db.setDatabaseName(qEnvironmentVariable("MKLIPPER_DATABASE"));
        } else {
            m_db.setDatabaseName(m_dbFolder + u"/history3.sqlite"_s);
        }
    }
    if (!m_db.isOpen() && !m_db.open()) {
        qWarning() << "Mklipper: Failed to open SQLite history:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    query.exec(u"PRAGMA secure_delete = ON"_s);
    query.exec(u"PRAGMA journal_mode = WAL"_s);
    if (query.exec(u"PRAGMA auto_vacuum"_s) && query.next() && query.value(0).toInt() != 1) {
        // Switch existing DBs to FULL auto-vacuum once so every deletion immediately reclaims disk space
        query.exec(u"PRAGMA auto_vacuum = FULL"_s);
        query.exec(u"VACUUM"_s);
        query.exec(u"PRAGMA wal_checkpoint(TRUNCATE)"_s);
    }
    enforcePrivateStoragePermissions();
    query.exec(u"CREATE TABLE IF NOT EXISTS main ("
               u"uuid char(40) PRIMARY KEY, "
               u"added_time REAL NOT NULL CHECK (added_time > 0), "
               u"last_used_time REAL CHECK (last_used_time > 0), "
               u"mimetypes TEXT NOT NULL, "
               u"text NTEXT, "
               u"starred BOOLEAN, "
               u"source_app TEXT DEFAULT '', "
               u"source_title TEXT DEFAULT '', "
               u"source_url TEXT DEFAULT '')"_s);

    // Backwards-compatible migration for databases created before v1.5.0
    {
        QSet<QString> existingCols;
        if (query.exec(u"PRAGMA table_info(main)"_s)) {
            while (query.next()) {
                existingCols.insert(query.value(1).toString());
            }
        }
        if (!existingCols.contains(u"source_app"_s)) {
            query.exec(u"ALTER TABLE main ADD COLUMN source_app TEXT DEFAULT ''"_s);
        }
        if (!existingCols.contains(u"source_title"_s)) {
            query.exec(u"ALTER TABLE main ADD COLUMN source_title TEXT DEFAULT ''"_s);
        }
        if (!existingCols.contains(u"source_url"_s)) {
            query.exec(u"ALTER TABLE main ADD COLUMN source_url TEXT DEFAULT ''"_s);
        }
    }
    query.exec(u"CREATE TABLE IF NOT EXISTS aux ("
               u"uuid char(40) NOT NULL, "
               u"mimetype TEXT NOT NULL, "
               u"data_uuid char(40) NOT NULL, "
               u"PRIMARY KEY (uuid, mimetype))"_s);
    query.exec(u"CREATE TABLE IF NOT EXISTS version (db_version INT NOT NULL)"_s);

    constexpr int currentDBVersion = 3;
    if (query.exec(u"SELECT db_version FROM version"_s) && query.next()) {
        if (query.value(0).toInt() != currentDBVersion) {
            qWarning() << "Mklipper: Unsupported DB version" << query.value(0).toInt();
            return false;
        }
    } else if (!query.exec(u"INSERT INTO version (db_version) VALUES (3)"_s)) {
        return false;
    }

    if (!m_bKeepContents) {
        query.exec(u"DELETE FROM main"_s);
        query.exec(u"DELETE FROM aux"_s);
        if (dataDir.exists()) {
            dataDir.removeRecursively();
            dataDir.mkpath(dataDir.absolutePath());
        }
        reclaimDiskSpace(true);
        enforcePrivateStoragePermissions();
        return true;
    }

    cleanupOrphanedStorage();

    QList<HistoryItemPtr> loadedItems;
    QHash<QString, bool> starredMap;
    int starred = 0;

    query.prepare(u"SELECT uuid, mimetypes, text, starred, source_app, source_title, source_url, added_time, last_used_time FROM main ORDER BY last_used_time DESC, added_time DESC LIMIT ?"_s);
    query.addBindValue(static_cast<int>(qMax<qsizetype>(m_maxSize, 2048)));
    if (query.exec()) {
        while (query.next()) {
            if (HistoryItemPtr item = HistoryItem::create(query)) {
                const bool isStar = query.value(u"starred"_s).toBool();
                if (isStar) {
                    starredMap.insert(item->uuid(), true);
                    ++starred;
                }
                loadedItems.append(std::move(item));
            }
        }
    }

    if (loadedItems.isEmpty()) {
        return true;
    }

    beginResetModel();
    m_items = std::move(loadedItems);
    m_starredCache = std::move(starredMap);
    m_starredCount = starred;
    endResetModel();
    Q_EMIT starredCountChanged(m_starredCount);

    // Trim any excess non-starred rows if maxSize was lowered since last session
    while (m_items.size() > m_maxSize) {
        int itemToRemove = -1;
        for (qsizetype i = m_items.size() - 1; i >= 0; --i) {
            if (!isItemStarred(m_items[i]->uuid())) {
                itemToRemove = static_cast<int>(i);
                break;
            }
        }
        if (itemToRemove < 0) {
            break;
        }
        removeRow(itemToRemove);
    }

    // Restore top clipboard entry on startup only if clipboard is currently empty
    const QClipboard *sysClip = QGuiApplication::clipboard();
    const bool clipEmpty = !sysClip || !sysClip->mimeData(QClipboard::Clipboard) || sysClip->mimeData(QClipboard::Clipboard)->formats().isEmpty();
    if (clipEmpty && !m_items.isEmpty()) {
        m_clip->setMimeData(m_items[0], SystemClipboard::SelectionMode(SystemClipboard::Clipboard | SystemClipboard::Selection));
    }

    return true;
}

bool HistoryModel::saveClipboardHistory()
{
    if (!m_db.isOpen()) {
        return false;
    }
    reclaimDiskSpace(false);
    enforcePrivateStoragePermissions();
    return true;
}

void HistoryModel::loadSettings()
{
    const auto *s = MklipperSettings::self();
    setMaxSize(s->maxClipItems());
    m_bSaveImages = s->saveImages();
    m_bNoNullClipboard = s->preventEmptyClipboard();
    m_bSaveSelection = s->saveSelection();
    m_bKeepContents = s->keepClipboardContents();
    m_bSynchronize = s->syncClipboards();
    m_bActionOnSelection = s->actionOnSelection();
    const bool prevPlainText = m_bPastePlainText;
    m_bPastePlainText = s->pastePlainText();
    m_bSaveSourceInfo = s->saveSourceInfo();

    if (m_bNoNullClipboard) {
        connect(m_clip.get(), &SystemClipboard::receivedEmptyClipboard, this, &HistoryModel::slotReceivedEmptyClipboard, Qt::UniqueConnection);
    } else {
        disconnect(m_clip.get(), &SystemClipboard::receivedEmptyClipboard, this, &HistoryModel::slotReceivedEmptyClipboard);
    }

    if (!m_bKeepContents && m_db.isOpen()) {
        clear();
    } else if (prevPlainText != m_bPastePlainText && !m_items.isEmpty() && !m_clipboardHasPassword) {
        const SystemClipboard::SelectionMode buffersWithoutPassword = buffersHolding(Holding::NoPassword);
        if (buffersWithoutPassword != 0) {
            m_clip->setMimeData(m_items[0], buffersWithoutPassword);
        }
    }
}

void HistoryModel::moveToTop(const QString &uuid, std::optional<bool> plainTextOverride)
{
    const int idx = indexOf(uuid);
    if (idx < 0) {
        return;
    }
    m_nextPlainTextOverride = plainTextOverride;
    clearHasPassword();
    moveToTop(static_cast<qsizetype>(idx));
}

void HistoryModel::setCapturePaused(bool paused)
{
    if (m_capturePaused == paused) {
        return;
    }
    m_capturePaused = paused;
    Q_EMIT capturePausedChanged(m_capturePaused);
}

bool HistoryModel::toggleCapturePaused()
{
    setCapturePaused(!m_capturePaused);
    return m_capturePaused;
}

bool HistoryModel::exportImageToFile(const QString &uuid, const QString &destPath) const
{
    if (!m_db.isOpen() || !HistoryItem::isValidUuid(uuid) || destPath.isEmpty()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(u"SELECT data_uuid FROM aux WHERE uuid = ? AND mimetype IN (%1) ORDER BY rowid ASC LIMIT 1"_s.arg(imageMimeTypesSqlList()));
    query.addBindValue(uuid);
    if (!query.exec() || !query.next()) {
        return false;
    }

    const QString dataUuid = query.value(0).toString();
    if (!HistoryItem::isValidUuid(dataUuid)) {
        return false;
    }

    const QString srcPath = m_dbFolder + u"/data/"_s + uuid + u'/' + dataUuid;
    QImageReader reader(srcPath);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        return false;
    }

    QSaveFile out(destPath);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    QImageWriter writer(&out, "PNG");
    if (!writer.write(img)) {
        out.cancelWriting();
        return false;
    }
    return out.commit();
}

bool HistoryModel::exportStarredToJson(const QString &filePath, int *exportedCount) const
{
    if (exportedCount) {
        *exportedCount = 0;
    }
    if (filePath.isEmpty()) {
        return false;
    }

    constexpr qsizetype kMaxExportImageBytes = 10 * 1024 * 1024;
    QJsonArray itemsArr;
    int count = 0;

    for (const auto &item : std::as_const(m_items)) {
        if (!isItemStarred(item->uuid())) {
            continue;
        }
        QJsonObject obj;
        if (item->type() == HistoryItemType::Image) {
            QSqlQuery query(m_db);
            query.prepare(u"SELECT data_uuid FROM aux WHERE uuid = ? AND mimetype IN (%1) ORDER BY rowid ASC LIMIT 1"_s.arg(imageMimeTypesSqlList()));
            query.addBindValue(item->uuid());
            if (!query.exec() || !query.next()) {
                continue;
            }
            const QString dataUuid = query.value(0).toString();
            if (!HistoryItem::isValidUuid(dataUuid)) {
                continue;
            }
            QFile imgFile(m_dbFolder + u"/data/"_s + item->uuid() + u'/' + dataUuid);
            if (!imgFile.open(QIODevice::ReadOnly) || imgFile.size() <= 0 || imgFile.size() > kMaxExportImageBytes) {
                continue;
            }
            obj.insert(u"type"_s, u"image"_s);
            obj.insert(u"imagePngBase64"_s, QString::fromLatin1(imgFile.readAll().toBase64()));
        } else {
            obj.insert(u"type"_s, item->type() == HistoryItemType::Url ? u"url"_s : u"text"_s);
            obj.insert(u"text"_s, item->text());
        }
        if (!item->sourceApp().isEmpty()) {
            obj.insert(u"sourceApp"_s, item->sourceApp());
        }
        if (!item->sourceTitle().isEmpty()) {
            obj.insert(u"sourceTitle"_s, item->sourceTitle());
        }
        if (!item->sourceUrl().isEmpty()) {
            obj.insert(u"sourceUrl"_s, item->sourceUrl());
        }
        itemsArr.append(obj);
        ++count;
    }

    QJsonObject root;
    root.insert(u"format"_s, u"mklipper-pinned"_s);
    root.insert(u"version"_s, 1);
    root.insert(u"exportedAt"_s, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(u"items"_s, itemsArr);

    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (out.write(payload) != payload.size() || !out.commit()) {
        return false;
    }
    if (exportedCount) {
        *exportedCount = count;
    }
    return true;
}

bool HistoryModel::importStarredFromJson(const QString &filePath, int *importedCount)
{
    if (importedCount) {
        *importedCount = 0;
    }
    if (filePath.isEmpty()) {
        return false;
    }

    constexpr qint64 kMaxBackupFileBytes = 50 * 1024 * 1024;
    QFile in(filePath);
    if (!in.open(QIODevice::ReadOnly) || in.size() > kMaxBackupFileBytes) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll());
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(u"format"_s).toString() != u"mklipper-pinned"_s || !root.value(u"items"_s).isArray()) {
        return false;
    }

    const QJsonArray itemsArr = root.value(u"items"_s).toArray();
    int imported = 0;

    // Import in reverse order so the top-most exported item remains on top
    for (qsizetype i = itemsArr.size() - 1; i >= 0; --i) {
        const QJsonObject obj = itemsArr.at(i).toObject();
        const QString type = obj.value(u"type"_s).toString(u"text"_s);
        ClipboardSourceInfo src;
        src.app = obj.value(u"sourceApp"_s).toString();
        src.title = obj.value(u"sourceTitle"_s).toString();
        src.url = obj.value(u"sourceUrl"_s).toString();

        auto mime = std::make_unique<QMimeData>();
        if (type == u"image"_s) {
            const QByteArray raw = QByteArray::fromBase64(obj.value(u"imagePngBase64"_s).toString().toLatin1());
            const QImage img = QImage::fromData(raw, "PNG");
            if (img.isNull()) {
                continue;
            }
            mime->setImageData(img);
            mime->setData(QString(MimeTypes::png), raw);
        } else {
            const QString text = obj.value(u"text"_s).toString();
            if (text.trimmed().isEmpty()) {
                continue;
            }
            mime->setText(text);
        }

        const QString uuid = computeUuid(mime.get());
        const bool existed = (indexOf(uuid) >= 0);
        const bool wasStarred = existed && isItemStarred(uuid);

        if (insert(mime.get(), 0, src)) {
            const int row = indexOf(uuid);
            if (row >= 0 && !isItemStarred(uuid)) {
                setData(index(row), true, StarredRole);
            }
            if (!existed || !wasStarred) {
                ++imported;
            }
        }
    }

    if (importedCount) {
        *importedCount = imported;
    }
    return true;
}

void HistoryModel::touchLastUsedTime(qsizetype row)
{
    if (!m_db.isOpen() || row < 0 || row >= m_items.size()) {
        return;
    }
    const qreal nowTs = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    QSqlQuery query(m_db);
    query.prepare(u"UPDATE main SET last_used_time = ? WHERE uuid = ?"_s);
    query.addBindValue(nowTs);
    query.addBindValue(m_items[row]->uuid());
    if (query.exec()) {
        m_items[row]->setLastUsedTime(nowTs);
    }
}

void HistoryModel::moveToTop(qsizetype row)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    if (row == 0) {
        Q_EMIT changed(true);
        return;
    }
    touchLastUsedTime(row);
    beginMoveRows(QModelIndex(), static_cast<int>(row), static_cast<int>(row), QModelIndex(), 0);
    m_items.move(row, 0);
    endMoveRows();
}

bool HistoryModel::saveToFile(const QString &dbFolder, const QByteArray &data, const QString &itemUuid, const QString &dataUuid)
{
    if (!HistoryItem::isValidUuid(itemUuid) || !HistoryItem::isValidUuid(dataUuid)) {
        return false;
    }
    const QString folderPath = dbFolder + u"/data/"_s + itemUuid;
    QDir().mkpath(folderPath);
    QFile::setPermissions(folderPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    QSaveFile file(folderPath + u'/' + dataUuid);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Mklipper: Cannot write clipboard data file:" << file.errorString() << folderPath;
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

void HistoryModel::removeItemDataDir(const QString &uuid) const
{
    if (!HistoryItem::isValidUuid(uuid)) {
        return;
    }
    QDir(m_dbFolder + u"/data/"_s + uuid).removeRecursively();
}

void HistoryModel::reclaimDiskSpace(bool fullVacuum)
{
    if (!m_db.isOpen()) {
        return;
    }
    QSqlQuery q(m_db);
    if (fullVacuum) {
        q.exec(u"VACUUM"_s);
    }
    q.exec(u"PRAGMA wal_checkpoint(TRUNCATE)"_s);
}

void HistoryModel::cleanupOrphanedStorage()
{
    if (!m_db.isOpen()) {
        return;
    }

    QSet<QString> validUuids;
    QStringList invalidDbUuids;
    {
        QSqlQuery q(m_db);
        if (q.exec(u"SELECT uuid FROM main"_s)) {
            while (q.next()) {
                const QString uuid = q.value(0).toString();
                if (HistoryItem::isValidUuid(uuid)) {
                    validUuids.insert(uuid);
                } else {
                    invalidDbUuids.append(uuid);
                }
            }
        }
    }

    bool dbCleaned = false;
    if (!invalidDbUuids.isEmpty()) {
        TransactionGuard tx(&m_db);
        for (const QString &badUuid : std::as_const(invalidDbUuids)) {
            QSqlQuery d1(m_db);
            d1.prepare(u"DELETE FROM main WHERE uuid = ?"_s);
            d1.addBindValue(badUuid);
            QSqlQuery d2(m_db);
            d2.prepare(u"DELETE FROM aux WHERE uuid = ?"_s);
            d2.addBindValue(badUuid);
            tx.exec(d1);
            tx.exec(d2);
        }
        dbCleaned = true;
    }

    {
        QSqlQuery delOrphanAux(m_db);
        if (delOrphanAux.exec(u"DELETE FROM aux WHERE uuid NOT IN (SELECT uuid FROM main)"_s)
            && delOrphanAux.numRowsAffected() > 0) {
            dbCleaned = true;
        }
    }

    QDir dataDir(m_dbFolder + u"/data"_s);
    if (dataDir.exists()) {
        const QFileInfoList entries = dataDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
        for (const QFileInfo &fi : entries) {
            if (!validUuids.contains(fi.fileName())) {
                if (fi.isDir() && !fi.isSymLink()) {
                    QDir(fi.absoluteFilePath()).removeRecursively();
                } else {
                    QFile::remove(fi.absoluteFilePath());
                }
            } else if (fi.isDir() && !fi.isSymLink()) {
                QFile::setPermissions(fi.absoluteFilePath(),
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            }
        }
    }

    if (dbCleaned) {
        reclaimDiskSpace(false);
    }
}

void HistoryModel::enforcePrivateStoragePermissions() const
{
    constexpr auto dirPerm = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
    constexpr auto filePerm = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

    if (!m_dbFolder.isEmpty() && QFile::exists(m_dbFolder)) {
        QFile::setPermissions(m_dbFolder, dirPerm);
    }
    const QString dataPath = m_dbFolder + u"/data"_s;
    if (QFile::exists(dataPath)) {
        QFile::setPermissions(dataPath, dirPerm);
    }
    const QString dbName = m_db.databaseName();
    if (!dbName.isEmpty()) {
        for (const QString &f : {dbName, dbName + u"-wal"_s, dbName + u"-shm"_s}) {
            if (QFile::exists(f)) {
                QFile::setPermissions(f, filePerm);
            }
        }
    }
}

void HistoryModel::moveTopToBack()
{
    if (m_items.size() < 2) {
        return;
    }
    clearHasPassword();
    beginMoveRows(QModelIndex(), 0, 0, QModelIndex(), static_cast<int>(m_items.size()));
    auto item = m_items.takeFirst();
    m_items.append(item);
    endMoveRows();
}

void HistoryModel::moveBackToTop()
{
    if (m_items.size() < 2) {
        return;
    }
    clearHasPassword();
    moveToTop(m_items.size() - 1);
}

QHash<int, QByteArray> HistoryModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {ImageUrlRole, "decoration"},
        {ImageSizeRole, "imageSize"},
        {UuidRole, "uuid"},
        {TypeIntRole, "type"},
        {StarredRole, "starred"},
        {HasActionRole, "hasAction"},
        {SourceAppRole, "sourceApp"},
        {SourceTitleRole, "sourceTitle"},
        {SourceUrlRole, "sourceUrl"},
        {AddedTimeRole, "addedTime"},
        {LastUsedTimeRole, "lastUsedTime"},
    };
}

void HistoryModel::checkClipData(QClipboard::Mode mode, const QMimeData *data)
{
    if (m_capturePaused) {
        return;
    }
    if (isIgnoredClipboardSource(mode, MklipperSettings::self()->ignoredApps())) {
        return;
    }

    const bool isPassword = isSecretMimeData(data);
    const bool selectionMode = (mode == QClipboard::Selection);
    if (isPassword) {
        setHasPassword(mode, true);
        if (m_bSynchronize) {
            m_clip->setMimeData(data,
                                selectionMode ? SystemClipboard::Clipboard : SystemClipboard::Selection,
                                SystemClipboard::ClipboardUpdateReason::SyncSelection);
            setHasPassword(selectionMode ? QClipboard::Clipboard : QClipboard::Selection, true);
        }
        return;
    }
    setHasPassword(mode, false);

    if (!hasMeaningfulClipboardContent(data)) {
        return;
    }

    if (selectionMode && !m_bSaveSelection) {
        if (m_bSynchronize) {
            m_clip->setMimeData(data, SystemClipboard::Clipboard, SystemClipboard::ClipboardUpdateReason::SyncSelection);
            clearHasPassword();
        }
        if (m_bActionOnSelection) {
            Q_EMIT selectionForAction(std::make_shared<HistoryItem>(computeUuid(data), data->formats(), textFromMimeData(data)));
        }
        return;
    }

    if (!m_bSaveImages && data->hasImage() && !data->hasText() && !data->hasFormat(MimeTypes::kdeForceImageCopy)) {
        return;
    }

    auto isRichTextToStrip = [](const QMimeData *mime) {
        if (!mime || !mime->hasText() || mime->hasUrls()) {
            return false;
        }
        if (mime->hasImage() && !mime->hasHtml()) {
            return false;
        }
        if (mime->hasHtml()) {
            return true;
        }
        const QStringList fmts = mime->formats();
        for (const QString &f : fmts) {
            if (f.compare(u"text/rtf"_s, Qt::CaseInsensitive) == 0
                || f.compare(u"application/rtf"_s, Qt::CaseInsensitive) == 0
                || f.compare(u"text/richtext"_s, Qt::CaseInsensitive) == 0
                || f.compare(u"application/x-qt-richtext"_s, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };

    const bool stripFormatting = m_bPastePlainText && isRichTextToStrip(data);
    std::unique_ptr<QMimeData> plainOnly;
    if (stripFormatting) {
        plainOnly = std::make_unique<QMimeData>();
        plainOnly->setText(data->text());
    }

    const ClipboardSourceInfo sourceInfo = m_bSaveSourceInfo ? captureClipboardSource(mode, data) : ClipboardSourceInfo{};

    if (insert(data, 0, sourceInfo)) {
        if (stripFormatting && !selectionMode) {
            // Replace active X11 clipboard with plain text so even direct Ctrl+V pastes without formatting
            m_clip->setMimeData(plainOnly.get(), SystemClipboard::Clipboard);
        }
        if (m_bSynchronize) {
            m_clip->setMimeData(stripFormatting ? plainOnly.get() : data,
                                selectionMode ? SystemClipboard::Clipboard : SystemClipboard::Selection,
                                SystemClipboard::ClipboardUpdateReason::SyncSelection);
            clearHasPassword();
        }
    }
}

void HistoryModel::slotReceivedEmptyClipboard(QClipboard::Mode mode)
{
    if (m_capturePaused || !m_bNoNullClipboard || isIgnoredClipboardSource(mode, MklipperSettings::self()->ignoredApps())) {
        return;
    }
    if (auto top = first()) {
        m_clip->setMimeData(top,
                            mode == QClipboard::Selection ? SystemClipboard::Selection : SystemClipboard::Clipboard,
                            SystemClipboard::ClipboardUpdateReason::PreventEmptyClipboard);
    }
    setHasPassword(mode, false);
}

bool HistoryModel::isItemStarred(const QString &uuid) const
{
    return m_starredCache.value(uuid, false);
}

void HistoryModel::setHasPassword(QClipboard::Mode mode, bool hasPassword)
{
    bool &flag = (mode == QClipboard::Selection) ? m_selectionHasPassword : m_clipboardHasPassword;
    if (flag == hasPassword) {
        return;
    }
    flag = hasPassword;
    Q_EMIT hasPasswordChanged();
}

void HistoryModel::setURLGrabber(URLGrabber *grabber)
{
    m_urlGrabber = grabber;
}
