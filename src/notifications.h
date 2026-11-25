/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>

namespace Notifications
{
void show(const QString &title, const QString &body, int timeoutMs = 3000);
}
