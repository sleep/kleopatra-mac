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
 * Sets whether the native widget style of macOS is used. The look of macOS applications is
 * only imitated together with this style.
 */
void setNativeStyleActive(bool active);

/**
 * Returns whether the native widget style of macOS is used.
 */
bool isNativeStyleActive();

/**
 * Returns an icon showing the SF Symbol @p symbolName, tinted with the text color of the
 * current palette, so that it follows the light and dark appearance. Returns @p fallback
 * if the symbol isn't available. If @p fallback isn't null, then the icon shows @p fallback
 * instead of the symbol while the native widget style of macOS isn't used.
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
