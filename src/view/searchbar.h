/*
    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>

class QLineEdit;

namespace Kleo
{

class KeyFilter;

class SearchBar : public QWidget
{
    Q_OBJECT
public:
    explicit SearchBar(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~SearchBar() override;

    const std::shared_ptr<KeyFilter> &keyFilter() const;

    QLineEdit *lineEdit() const;

    void updateClickMessage(const QString &shortcutStr);
    void addCustomKeyFilter(const std::shared_ptr<KeyFilter> &keyFilter);

    /**
     * Switches between the normal layout, where the search field takes all available space,
     * and a compact layout for toolbars: the category filter comes first and the search field
     * has a limited width that doesn't depend on its content.
     */
    void setCompactLayout(bool compact);

    /**
     * Moves the child widgets behind \p widget in the tab order. Needs to be called after
     * the search bar was moved to another parent widget.
     */
    void setTabOrderAfter(QWidget *widget);

public Q_SLOTS:
    void setStringFilter(const QString &text);
    void setKeyFilter(const std::shared_ptr<Kleo::KeyFilter> &filter);

    void setChangeStringFilterEnabled(bool enable);
    void setChangeKeyFilterEnabled(bool enable);

Q_SIGNALS:
    void stringFilterChanged(const QString &text);
    void keyFilterChanged(const std::shared_ptr<Kleo::KeyFilter> &filter);

private:
    class Private;
    const std::unique_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void showOrHideCertifyButton())
};

}
