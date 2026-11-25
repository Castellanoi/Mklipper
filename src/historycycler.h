/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include <QByteArray>
#include <QObject>

#include "historyitem.h"

class HistoryModel;

class HistoryCycler : public QObject
{
    Q_OBJECT

public:
    explicit HistoryCycler(QObject *parent = nullptr);
    ~HistoryCycler() override = default;

    void cycleNext();
    void cyclePrev();

    HistoryItemConstPtr nextInCycle() const;
    HistoryItemConstPtr prevInCycle() const;

private:
    std::shared_ptr<HistoryModel> m_model;
    QByteArray m_cycleStartUuid;
};
