/*
    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2026 Kleopatra contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QIcon>

class QAbstractItemView;
class QString;

namespace Kleo::MacOS
{
/**
 * Returns an icon showing the SF Symbol @p symbolName, tinted with the text color of the
 * current palette, so that it follows the light and dark appearance. Returns @p fallback
 * if the symbol isn't available.
 */
QIcon symbolIcon(const QString &symbolName, const QIcon &fallback = {});

/**
 * Returns an SF Symbol icon matching the theme icon of @p icon, or @p icon if there is
 * no matching SF Symbol.
 */
QIcon symbolIconFor(const QIcon &icon);

/**
 * Makes @p view look like a list in macOS applications.
 */
void styleItemView(QAbstractItemView *view);
}
