/*
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "historycycler.h"

#include "historymodel.h"

namespace
{
class CycleBlocker
{
public:
    CycleBlocker() { ++s_blocker; }
    ~CycleBlocker() { --s_blocker; }
    static bool isBlocked() { return s_blocker > 0; }

private:
    static int s_blocker;
};

int CycleBlocker::s_blocker = 0;
}

HistoryCycler::HistoryCycler(QObject *parent)
    : QObject(parent)
    , m_model(HistoryModel::self())
{
    connect(m_model.get(), &HistoryModel::changed, this, [this](bool isTop) {
        if (isTop && !CycleBlocker::isBlocked()) {
            m_cycleStartUuid.clear();
        }
    });
}

void HistoryCycler::cycleNext()
{
    if (m_model->rowCount() < 2) {
        return;
    }

    if (m_cycleStartUuid.isEmpty()) {
        m_cycleStartUuid = m_model->index(0).data(HistoryModel::UuidRole).toByteArray();
    } else if (m_cycleStartUuid == m_model->index(1).data(HistoryModel::UuidRole).toByteArray()) {
        return;
    }

    CycleBlocker blocker;
    m_model->moveTopToBack();
}

void HistoryCycler::cyclePrev()
{
    if (m_cycleStartUuid.isEmpty() || m_model->rowCount() < 2) {
        return;
    }

    CycleBlocker blocker;
    m_model->moveBackToTop();
    if (m_cycleStartUuid == m_model->index(0).data(HistoryModel::UuidRole).toByteArray()) {
        m_cycleStartUuid.clear();
    }
}

HistoryItemConstPtr HistoryCycler::nextInCycle() const
{
    if (m_model->hasIndex(1, 0)) {
        if (!m_cycleStartUuid.isEmpty()
            && m_cycleStartUuid == m_model->index(1).data(HistoryModel::UuidRole).toByteArray()) {
            return {};
        }
        return m_model->index(1).data(HistoryModel::HistoryItemConstPtrRole).value<HistoryItemConstPtr>();
    }
    return {};
}

HistoryItemConstPtr HistoryCycler::prevInCycle() const
{
    if (m_cycleStartUuid.isEmpty() || m_model->rowCount() < 1) {
        return {};
    }
    return m_model->index(m_model->rowCount() - 1).data(HistoryModel::HistoryItemConstPtrRole).value<HistoryItemConstPtr>();
}
