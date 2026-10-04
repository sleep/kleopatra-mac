/*
    This file is part of Kleopatra
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-only
*/

#pragma once

#include <KPageDialog>
#include <QHash>
#include <QList>

class KPageWidgetItem;
class QAction;
class QActionGroup;
class QToolBar;

namespace Kleo
{
namespace Config
{
class KleoConfigModule;
}
}

/**
 * KPageDialog based config dialog to be used when
 * KCMUtils are not available. */
class KleoPageConfigDialog : public KPageDialog
{
    Q_OBJECT
public:
    explicit KleoPageConfigDialog(QWidget *parent = nullptr);

    // macOSSymbolName is the SF Symbol shown for the module in the toolbar on macOS
    void
    addModule(const QString &name, const QString &docPath, const QString &icon, Kleo::Config::KleoConfigModule *module, const QString &macOSSymbolName = {});

Q_SIGNALS:
    void configCommitted();

protected Q_SLOTS:
    void slotDefaultClicked();
    void slotUser1Clicked();
    void slotApplyClicked();
    void slotOkClicked();
    void slotHelpClicked();
    void slotCurrentPageChanged(KPageWidgetItem *current, KPageWidgetItem *previous);
    void moduleChanged(bool value);

protected:
    void initButtons();
#ifdef Q_OS_MACOS
    void showEvent(QShowEvent *event) override;
#endif

private:
    void clientChanged();
    void apply();

    QSet<Kleo::Config::KleoConfigModule *> mChangedModules;
    QMap<QString, QString> mHelpUrls;
#ifdef Q_OS_MACOS
    // like the settings windows of macOS applications, the pages are selected with a toolbar
    void updateToolBar();
    QToolBar *mToolBar = nullptr;
    QActionGroup *mPageActions = nullptr;
    QHash<KPageWidgetItem *, QAction *> mPageActionForItem;
#endif
};
