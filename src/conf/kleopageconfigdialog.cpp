/*
    This file is part of Kleopatra
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-only

    It is derived from KCMultidialog which is:

    SPDX-FileCopyrightText: 2000 Matthias Elter <elter@kde.org>
    SPDX-FileCopyrightText: 2003 Daniel Molkentin <molkentin@kde.org>
    SPDX-FileCopyrightText: 2003, 2006 Matthias Kretz <kretz@kde.org>
    SPDX-FileCopyrightText: 2004 Frans Englich <frans.englich@telia.com>
    SPDX-FileCopyrightText: 2006 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "kleopageconfigdialog.h"

#include "kleoconfigmodule.h"

#include <QActionGroup>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QEvent>
#include <QKeySequence>
#include <QLocale>
#include <QProcess>
#include <QPushButton>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KMessageBox>
#include <KPageWidget>
#include <KStandardGuiItem>

#include "kleopatra_debug.h"

#ifdef Q_OS_MACOS
#include "macos/macosstyle.h"
#include "utils/gui-helper.h"
#endif

using namespace Kleo::Config;

KleoPageConfigDialog::KleoPageConfigDialog(QWidget *parent)
    : KPageDialog(parent)
{
    setModal(false);
#ifdef Q_OS_MACOS
    setFaceType(KPageDialog::Plain);
    mToolBar = new QToolBar{this};
    mToolBar->setAccessibleName(i18nc("@label", "Settings"));
    mToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mToolBar->setIconSize({24, 24});
    mToolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mPageActions = new QActionGroup{this};
    mPageActions->setExclusive(true);
    connect(this, &KPageDialog::currentPageChanged, this, &KleoPageConfigDialog::updateToolBar, Qt::QueuedConnection);
#endif
}

#ifdef Q_OS_MACOS
void KleoPageConfigDialog::showEvent(QShowEvent *event)
{
    // KPageDialog recreates its layout whenever the button box is set, so make sure that
    // the toolbar is (still) part of the layout
    addToolBarToLayout();
    KPageDialog::showEvent(event);
}

void KleoPageConfigDialog::changeEvent(QEvent *event)
{
    KPageDialog::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange) {
        // the buttons of the toolbar are updated after the dialog
        QMetaObject::invokeMethod(this, &KleoPageConfigDialog::updateToolBarWidth, Qt::QueuedConnection);
    }
}

void KleoPageConfigDialog::addToolBarToLayout()
{
    auto dialogLayout = qobject_cast<QBoxLayout *>(layout());
    if (!dialogLayout || dialogLayout->indexOf(mToolBar) >= 0) {
        return;
    }
    dialogLayout->insertWidget(0, mToolBar);
    updateToolBarWidth();

    // the buttons of the toolbar come first in the tab order; the pages and the dialog buttons
    // follow them. The dialog itself doesn't accept focus, so the order has to be forced.
    QWidget *previous = this;
    const auto actions = mPageActions->actions();
    for (auto action : actions) {
        if (auto button = mToolBar->widgetForAction(action)) {
            Kleo::forceSetTabOrder(previous, button);
            previous = button;
        }
    }
}

void KleoPageConfigDialog::updateToolBarWidth()
{
    // never make the toolbar move page buttons to its extension menu
    mToolBar->ensurePolished();
    mToolBar->setMinimumWidth(mToolBar->sizeHint().width());
}

void KleoPageConfigDialog::updateToolBar()
{
    if (auto action = mPageActionForItem.value(currentPage())) {
        action->setChecked(true);
        setWindowTitle(currentPage()->name());
    }
}
#endif

void KleoPageConfigDialog::initButtons()
{
    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Help //
                                  | QDialogButtonBox::RestoreDefaults //
                                  | QDialogButtonBox::Cancel //
                                  | QDialogButtonBox::Apply //
                                  | QDialogButtonBox::Ok //
                                  | QDialogButtonBox::Reset);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Ok), KStandardGuiItem::ok());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::RestoreDefaults), KStandardGuiItem::defaults());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Apply), KStandardGuiItem::apply());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Reset), KStandardGuiItem::reset());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Help), KStandardGuiItem::help());
    buttonBox->button(QDialogButtonBox::Reset)->setEnabled(false);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &KleoPageConfigDialog::slotApplyClicked);
    connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &KleoPageConfigDialog::slotOkClicked);
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, this, &KleoPageConfigDialog::slotDefaultClicked);
    connect(buttonBox->button(QDialogButtonBox::Help), &QAbstractButton::clicked, this, &KleoPageConfigDialog::slotHelpClicked);
    connect(buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this, &KleoPageConfigDialog::slotUser1Clicked);

    setButtonBox(buttonBox);

    // KPageDialog forwards the signal of the page widget once more each time the button box
    // is set; forward it only once, so that a change of the page is handled only once
    disconnect(pageWidget(), &KPageWidget::currentPageChanged, this, &KPageDialog::currentPageChanged);
    connect(pageWidget(), &KPageWidget::currentPageChanged, this, &KPageDialog::currentPageChanged);
#ifdef Q_OS_MACOS
    // add the toolbar before the dialog is shown, so that it's considered for the initial size
    addToolBarToLayout();
#endif

    connect(this, &KPageDialog::currentPageChanged, this, &KleoPageConfigDialog::slotCurrentPageChanged);
}

void KleoPageConfigDialog::slotCurrentPageChanged(KPageWidgetItem *current, KPageWidgetItem *previous)
{
    if (!previous || currentPage() != current) {
        return;
    }
    blockSignals(true);
    setCurrentPage(previous);

    auto previousModule = qobject_cast<KleoConfigModule *>(previous->widget());
    bool canceled = false;
    if (previousModule && mChangedModules.contains(previousModule)) {
        const int queryUser = KMessageBox::warningTwoActionsCancel(this,
                                                                   i18n("The settings of the current module have changed.\n"
                                                                        "Do you want to apply the changes or discard them?"),
                                                                   i18nc("@title:window", "Apply Settings"),
                                                                   KStandardGuiItem::apply(),
                                                                   KStandardGuiItem::discard(),
                                                                   KStandardGuiItem::cancel());
        if (queryUser == KMessageBox::ButtonCode::PrimaryAction) {
            previousModule->save();
        } else if (queryUser == KMessageBox::ButtonCode::SecondaryAction) {
            previousModule->load();
        }
        canceled = queryUser == KMessageBox::Cancel;
    }
    if (!canceled) {
        mChangedModules.remove(previousModule);
        setCurrentPage(current);
    }
    blockSignals(false);

    clientChanged();
}

void KleoPageConfigDialog::apply()
{
    QPushButton *applyButton = buttonBox()->button(QDialogButtonBox::Apply);
    applyButton->setFocus();
    for (const auto &module : mChangedModules) {
        module->save();
    }
    mChangedModules.clear();
    Q_EMIT configCommitted();
    clientChanged();
}

void KleoPageConfigDialog::slotDefaultClicked()
{
    const KPageWidgetItem *item = currentPage();
    if (!item) {
        return;
    }

    auto module = qobject_cast<KleoConfigModule *>(item->widget());
    if (!module) {
        return;
    }
    module->defaults();
    clientChanged();
}

void KleoPageConfigDialog::slotUser1Clicked()
{
    const KPageWidgetItem *item = currentPage();
    if (!item) {
        return;
    }

    auto module = qobject_cast<KleoConfigModule *>(item->widget());
    if (!module) {
        return;
    }
    module->load();
    mChangedModules.remove(module);
    clientChanged();
}

void KleoPageConfigDialog::slotApplyClicked()
{
    apply();
}

void KleoPageConfigDialog::slotOkClicked()
{
    apply();
    accept();
}

void KleoPageConfigDialog::slotHelpClicked()
{
    const KPageWidgetItem *item = currentPage();
    if (!item) {
        return;
    }

    const QString docPath = mHelpUrls.value(item->name());
    QUrl docUrl;

#ifdef Q_OS_WIN
    docUrl =
        QUrl(QLatin1StringView("https://docs.kde.org/index.php?branch=stable5&language=") + QLocale().name() + QLatin1StringView("&application=kleopatra"));
#elif defined(Q_OS_MACOS)
    // there's no help center on macOS, so the online documentation is opened in the browser
    Q_UNUSED(docPath)
    docUrl = QUrl(QStringLiteral("https://docs.kde.org/?application=kleopatra&branch=stable6"));
#else
    docUrl = QUrl(QStringLiteral("help:/")).resolved(QUrl(docPath)); // same code as in KHelpClient::invokeHelp
#endif
    if (docUrl.scheme() == QLatin1StringView("help") || docUrl.scheme() == QLatin1StringView("man") || docUrl.scheme() == QLatin1StringView("info")) {
        // Warning: Don't assume that the program needs to be in PATH. On Windows, it will also be found next to the calling process.
        QProcess::startDetached(QStringLiteral("khelpcenter"), QStringList() << docUrl.toString());
    } else {
        QDesktopServices::openUrl(docUrl);
    }
}

void KleoPageConfigDialog::addModule(const QString &name, const QString &docPath, const QString &icon, KleoConfigModule *module, const QString &macOSSymbolName)
{
    module->load();
    auto item = addPage(module, name);
    item->setIcon(QIcon::fromTheme(icon));
#ifdef Q_OS_MACOS
    // the name of the page is shown as window title
    item->setHeaderVisible(false);
    auto action = mToolBar->addAction(Kleo::MacOS::symbolIcon(macOSSymbolName, item->icon()), name);
    action->setCheckable(true);
    action->setActionGroup(mPageActions);
    // QToolBar creates buttons that cannot be reached with the keyboard
    if (auto button = mToolBar->widgetForAction(action)) {
        button->setFocusPolicy(Qt::TabFocus);
    }
    // the tab key skips buttons unless keyboard navigation is enabled in the system settings,
    // so the pages can also be selected with shortcuts
    if (const int number = mPageActions->actions().size(); number <= 9) {
        action->setShortcut(QKeySequence{Qt::CTRL | static_cast<Qt::Key>(Qt::Key_0 + number)});
    }
    connect(action, &QAction::triggered, this, [this, item]() {
        setCurrentPage(item);
        // switching the page may have been canceled
        updateToolBar();
    });
    mPageActionForItem.insert(item, action);
    updateToolBar();
#else
    Q_UNUSED(macOSSymbolName)
#endif
    connect(module, &KleoConfigModule::changed, this, [this, module]() {
        moduleChanged(true);
        mChangedModules.insert(module);
    });
    mHelpUrls.insert(name, docPath);
}

void KleoPageConfigDialog::moduleChanged(bool state)
{
    auto module = qobject_cast<KleoConfigModule *>(sender());
    qCDebug(KLEOPATRA_LOG) << "Module changed: " << state << " mod " << module;
    if (mChangedModules.contains(module)) {
        if (!state) {
            mChangedModules.remove(module);
        } else {
            return;
        }
    }
    if (state) {
        mChangedModules << module;
    }
    clientChanged();
}

void KleoPageConfigDialog::clientChanged()
{
    const KPageWidgetItem *item = currentPage();
    if (!item) {
        return;
    }
    auto module = qobject_cast<KleoConfigModule *>(currentPage()->widget());

    if (!module) {
        return;
    }
    qCDebug(KLEOPATRA_LOG) << "Client changed: "
                           << " mod " << module;

    bool change = mChangedModules.contains(module);

    QPushButton *resetButton = buttonBox()->button(QDialogButtonBox::Reset);
    if (resetButton) {
        resetButton->setEnabled(change);
    }

    QPushButton *applyButton = buttonBox()->button(QDialogButtonBox::Apply);
    if (applyButton) {
        applyButton->setEnabled(change);
    }
}

#include "moc_kleopageconfigdialog.cpp"
