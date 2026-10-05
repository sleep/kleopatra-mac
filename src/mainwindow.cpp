/*
    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow.h"
#include "kleopatraapplication.h"
#include "settings.h"
#include <config-kleopatra.h>

#include <interfaces/focusfirstchild.h>

#include "view/keycacheoverlay.h"
#include "view/keylistcontroller.h"
#include "view/padwidget.h"
#include "view/searchbar.h"
#include "view/tabwidget.h"
#include "view/welcomewidget.h"

#include "commands/decryptverifyfilescommand.h"
#include "commands/importcertificatefromfilecommand.h"
#include "commands/importcrlcommand.h"
#include "commands/selftestcommand.h"
#include "commands/signencryptfilescommand.h"

#include "utils/action_data.h"
#include "utils/clipboardmenu.h"
#include "utils/detail_p.h"
#include "utils/distributiondata.h"
#include "utils/filedialog.h"
#include "utils/gui-helper.h"
#include "utils/keyexportdraghandler.h"
#include "utils/userinfo.h"

#include <Libkleo/ApplicationPaletteWatcher>

#include "dialogs/debugdialog.h"
#include "dialogs/padwindow.h"
#include "dialogs/updatenotification.h"

#include "kleopatra_debug.h"
#include <KAboutData>
#include <KActionCollection>
#include <KActionMenu>
#include <KColorScheme>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#ifdef Q_OS_MACOS
#include <KColorSchemeModel>
#endif
#include <KConfigDialog>
#include <KConfigGroup>
#include <KEditToolBar>
#include <KHelpMenu>
#include <KLocalizedString>
#include <KMessageBox>
#include <KShortcutsDialog>
#include <KStandardAction>
#include <KStandardGuiItem>
#include <KToolBar>
#include <KXMLGUIFactory>
#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QSize>

#include <QAbstractItemView>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPixmap>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyleHints>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetAction>

#ifdef Q_OS_MACOS
#include "macos/macosstyle.h"
#endif

#include <Libkleo/Classify>
#include <Libkleo/Compliance>
#include <Libkleo/DocAction>
#include <Libkleo/Formatting>
#include <Libkleo/GnuPG>
#include <Libkleo/KeyCache>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/Stl_Util>
#include <Libkleo/SystemInfo>

#include <KSharedConfig>

#include <chrono>
#include <vector>
using namespace std::chrono_literals;

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

using namespace Qt::Literals::StringLiterals;

static KGuiItem KStandardGuiItem_quit()
{
    static const QString app = KAboutData::applicationData().displayName();
    KGuiItem item = KStandardGuiItem::quit();
    item.setText(xi18nc("@action:button", "&Quit <application>%1</application>", app));
    return item;
}

static KGuiItem KStandardGuiItem_close()
{
    KGuiItem item = KStandardGuiItem::close();
    item.setText(i18nc("@action:button", "Only &Close Window"));
    return item;
}

static bool isQuitting = false;

namespace
{

class CertificateView : public QWidget, public FocusFirstChild
{
    Q_OBJECT
public:
    explicit CertificateView(QWidget *parent = nullptr)
        : QWidget{parent}
        , ui{this}
    {
    }

    SearchBar *searchBar() const
    {
        return ui.searchBar;
    }

    TabWidget *tabWidget() const
    {
        return ui.tabWidget;
    }

    void focusFirstChild(Qt::FocusReason reason) override
    {
        static bool firstCall = true; // there's only one CertificateView per app so that using a function static is okay
        if (firstCall) {
            firstCall = false;
            ui.searchBar->lineEdit()->setFocus(reason);
        }
    }

#ifdef Q_OS_MACOS
    // Puts the search bar back above the certificate list after it was shown in the toolbar
    void takeBackSearchBar()
    {
        ui.searchBar->setCompactLayout(false);
        static_cast<QVBoxLayout *>(layout())->insertWidget(0, ui.searchBar);
        ui.searchBar->setTabOrderAfter(this);
        ui.searchBar->show();
    }
#endif

private:
    struct UI {
        TabWidget *tabWidget = nullptr;
        SearchBar *searchBar = nullptr;
        explicit UI(CertificateView *q)
        {
            auto vbox = new QVBoxLayout{q};
            vbox->setSpacing(0);

            searchBar = new SearchBar{q};
            vbox->addWidget(searchBar);
            tabWidget = new TabWidget{KeyTreeView::Option::NoDefaultContextMenu, q};
            vbox->addWidget(tabWidget);

            tabWidget->connectSearchBar(searchBar);
        }
    } ui;
};

#ifdef Q_OS_MACOS
// The toolbar item of the search field. It shows the search bar of the certificate view
// as long as there is room for it in the toolbar. The search bar is only a guest: it's
// handed back to the certificate view before the item is destroyed with its toolbar.
class SearchFieldHost : public QWidget
{
    Q_OBJECT
public:
    explicit SearchFieldHost(CertificateView *view, QWidget *parent = nullptr)
        : QWidget{parent}
        , view{view}
    {
        auto layout = new QHBoxLayout{this};
        layout->setContentsMargins({});
        // keeps the search field and its focus ring off the edge of the window
        layout->addSpacing(8);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    ~SearchFieldHost() override
    {
        if (hasSearchBar()) {
            view->takeBackSearchBar();
        }
    }

    bool hasSearchBar() const
    {
        return view && view->searchBar()->parentWidget() == this;
    }

    void takeSearchBar()
    {
        auto searchBar = view->searchBar();
        setMinimumWidth(0);
        searchBar->setCompactLayout(true);
        static_cast<QHBoxLayout *>(layout())->insertWidget(0, searchBar);
        searchBar->setTabOrderAfter(this);
        searchBar->show();
    }

    // Keeps requesting the space for the search bar after it left. This way the toolbar
    // shows the item again only if the search bar fits.
    void reserveSpace()
    {
        setMinimumWidth(minimumSizeHint().width());
    }

Q_SIGNALS:
    void visibilityChanged();

protected:
    bool event(QEvent *e) override
    {
        switch (e->type()) {
        case QEvent::ShowToParent:
            // KXMLGUI makes all toolbar items focusable, but only the widgets of the search
            // bar shall get the keyboard focus
            setFocusPolicy(Qt::NoFocus);
            [[fallthrough]];
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::HideToParent:
            Q_EMIT visibilityChanged();
            break;
        default:
            break;
        }
        return QWidget::event(e);
    }

private:
    const QPointer<CertificateView> view;
};

class SearchFieldAction : public QWidgetAction
{
    Q_OBJECT
public:
    explicit SearchFieldAction(CertificateView *view, QObject *parent = nullptr)
        : QWidgetAction{parent}
        , view{view}
    {
    }

    // Returns the toolbar item in \p window that can show the search bar, i.e. an item that
    // is neither hidden with its toolbar nor hidden because it doesn't fit in the toolbar
    SearchFieldHost *usableHost(const QWidget *window) const
    {
        const auto hosts = createdWidgets();
        for (auto host : hosts) {
            if (window->isAncestorOf(host) && host->isVisibleTo(window)) {
                return static_cast<SearchFieldHost *>(host);
            }
        }
        return nullptr;
    }

Q_SIGNALS:
    void hostsChanged();

protected:
    QWidget *createWidget(QWidget *parent) override
    {
        // there is only one search bar; it isn't shown in menus like the menu of the
        // toolbar extension
        if (!qobject_cast<QToolBar *>(parent)) {
            return nullptr;
        }
        auto host = new SearchFieldHost{view, parent};
        host->setEnabled(isEnabled());
        connect(host, &SearchFieldHost::visibilityChanged, this, &SearchFieldAction::hostsChanged);
        return host;
    }

private:
    CertificateView *const view;
};
#endif
}

class MainWindow::Private
{
    friend class ::MainWindow;
    MainWindow *const q;

public:
    explicit Private(MainWindow *qq);
    ~Private();

    template<typename T>
    void createAndStart()
    {
        (new T(this->currentView(), &this->controller))->start();
    }
    template<typename T>
    void createAndStart(QAbstractItemView *view)
    {
        (new T(view, &this->controller))->start();
    }
    template<typename T>
    void createAndStart(const QStringList &a)
    {
        (new T(a, this->currentView(), &this->controller))->start();
    }
    template<typename T>
    void createAndStart(const QStringList &a, QAbstractItemView *view)
    {
        (new T(a, view, &this->controller))->start();
    }

    void closeAndQuit()
    {
        if (qApp->quitOnLastWindowClosed()) {
            qApp->quit();
        }

        const QString app = KAboutData::applicationData().displayName();
        const int rc = KMessageBox::questionTwoActionsCancel(q,
                                                             xi18nc("@info",
                                                                    "<application>%1</application> may be used by other applications as a service.<nl/>"
                                                                    "You may instead want to close this window without exiting <application>%1</application>.",
                                                                    app),
                                                             i18nc("@title:window", "Really Quit?"),
                                                             KStandardGuiItem_close(),
                                                             KStandardGuiItem_quit(),
                                                             KStandardGuiItem::cancel(),
                                                             QLatin1StringView("really-quit-") + app.toLower());
        if (rc == KMessageBox::Cancel) {
            return;
        }
        isQuitting = rc == KMessageBox::ButtonCode::SecondaryAction;
        if (!q->close()) {
            return;
        }
        // WARNING: 'this' might be deleted at this point!
        if (rc == KMessageBox::ButtonCode::SecondaryAction) {
            qApp->quit();
        }
    }
    void configureToolbars()
    {
        KEditToolBar dlg(q->factory());
        dlg.exec();
#ifdef Q_OS_MACOS
        // the toolbar may have been rebuilt
        setUpMacOSToolBar();
        updateSearchBarPlacement();
#endif
    }
    void editKeybindings()
    {
        KShortcutsDialog::showDialog(q->actionCollection(), KShortcutsEditor::LetterShortcutsAllowed, q);
        updateSearchBarClickMessage();
    }

    void updateSearchBarClickMessage()
    {
        const QString shortcutStr = focusToClickSearchAction->shortcut().toString(QKeySequence::NativeText);
        ui.searchTab->searchBar()->updateClickMessage(shortcutStr);
    }

    void updateStatusBar()
    {
        if (const auto distributionData = KleopatraApplication::instance()->distributionData()) {
            auto statusBar = std::make_unique<QStatusBar>();
            if (distributionData->isValid) {
                const QString statusline = distributionData->statusLine.value_or(QString{});
                if (!statusline.isEmpty()) {
                    auto customStatusLbl = new QLabel(statusline);
                    statusBar->insertWidget(0, customStatusLbl);
                }
                if (DeVSCompliance::isActive()) {
                    auto statusLbl = std::make_unique<QLabel>(DeVSCompliance::name());
                    {
                        auto statusPalette = qApp->palette();
                        KColorScheme::adjustForeground(statusPalette,
                                                       DeVSCompliance::isCompliant() ? KColorScheme::NormalText : KColorScheme::NegativeText,
                                                       statusLbl->foregroundRole(),
                                                       KColorScheme::View);
                        statusLbl->setAutoFillBackground(true);
                        KColorScheme::adjustBackground(statusPalette,
                                                       DeVSCompliance::isCompliant() ? KColorScheme::PositiveBackground : KColorScheme::NegativeBackground,
                                                       QPalette::Window,
                                                       KColorScheme::View);
                        statusLbl->setPalette(statusPalette);
                    }
                    statusBar->insertPermanentWidget(0, statusLbl.release());
                }
            } else {
                statusBar->insertWidget(0, new QLabel{KAboutData::applicationData().version()});
                auto statusLbl = std::make_unique<QLabel>(i18nc("@info:status", "Corrupt installation"));
                statusLbl->setToolTip(distributionData->detailedError);
                statusLbl->setAutoFillBackground(true);
                auto statusPalette = qApp->palette();
                KColorScheme::adjustBackground(statusPalette, KColorScheme::NegativeBackground, QPalette::Window, KColorScheme::View);
                statusLbl->setPalette(statusPalette);
                statusBar->insertPermanentWidget(0, statusLbl.release());
            }
            q->setStatusBar(statusBar.release()); // QMainWindow takes ownership
        } else {
            q->setStatusBar(nullptr);
        }
    }

    void selfTest()
    {
        createAndStart<SelfTestCommand>();
    }

    void configureGroups()
    {
        // open groups config dialog as independent top-level window
        KleopatraApplication::instance()->openOrRaiseGroupsConfigDialog(nullptr);
    }

    void showHandbook();

    void gnupgLogViewer()
    {
        // Warning: Don't assume that the program needs to be in PATH. On Windows, it will also be found next to the calling process.
        const QString kwatchgnupgPath = QStandardPaths::findExecutable(u"kwatchgnupg"_s);
        qCDebug(KLEOPATRA_LOG) << "Starting" << kwatchgnupgPath;
        if (!QProcess::startDetached(kwatchgnupgPath))
            KMessageBox::error(q,
                               i18n("Could not start the GnuPG Log Viewer (kwatchgnupg). "
                                    "Please check your installation."),
                               i18n("Error Starting KWatchGnuPG"));
    }

    void forceUpdateCheck()
    {
        UpdateNotification::forceUpdateCheck(q);
    }

    void slotConfigCommitted();
    void slotContextMenuRequested(QAbstractItemView *, const QPoint &p)
    {
        if (auto const menu = qobject_cast<QMenu *>(q->factory()->container(QStringLiteral("listview_popup"), q))) {
            menu->exec(p);
        } else {
            qCDebug(KLEOPATRA_LOG) << "no \"listview_popup\" <Menu> in kleopatra's ui.rc file";
        }
    }

    void slotFocusQuickSearch()
    {
        if (padViewIsShown()) {
            showCertificateView();
        }
        if (ui.stackWidget->currentWidget() != ui.searchTab) {
            // there's nothing to search
            return;
        }
#ifdef Q_OS_MACOS
        updateSearchBarPlacement();
#endif
        ui.searchTab->searchBar()->lineEdit()->setFocus();
    }

    void showView(QWidget *widget)
    {
        ui.stackWidget->setCurrentWidget(widget);
        updateViewActions();
        if (auto ffci = dynamic_cast<Kleo::FocusFirstChild *>(widget)) {
            ffci->focusFirstChild(Qt::TabFocusReason);
        }
    }

    void showCertificateView()
    {
        if (KeyCache::instance()->initialized() && KeyCache::instance()->keys().empty()) {
            showView(ui.welcomeWidget);
        } else {
            showView(ui.searchTab);
        }
    }

    void showPadView()
    {
        if (Settings{}.showNotepadInMainWindow()) {
            if (!ui.padWidget) {
                ui.padWidget = new PadWidget;
                ui.stackWidget->addWidget(ui.padWidget);
            }
            showView(ui.padWidget);
            return;
        }
        auto padWindow = new PadWindow();
        padWindow->setAttribute(Qt::WA_DeleteOnClose);
        padWindow->show();
    }

    bool padViewIsShown() const
    {
        return ui.padWidget && ui.stackWidget->currentWidget() == ui.padWidget;
    }

    void updateViewActions()
    {
        // in the main window the certificate view and the notepad behave like exclusive views
        const bool padShown = padViewIsShown();
        if (auto action = q->actionCollection()->action(u"view_certificate_overview"_s)) {
            action->setChecked(!padShown);
        }
        if (auto action = q->actionCollection()->action(u"pad_view"_s)) {
            action->setChecked(padShown);
        }
        // the search field in the toolbar is only useful for the certificate list
        if (auto action = q->actionCollection()->action(u"search_field"_s)) {
            action->setEnabled(ui.stackWidget->currentWidget() == ui.searchTab);
        }
    }

    void applyNotepadSetting()
    {
        const bool inMainWindow = Settings{}.showNotepadInMainWindow();
        if (auto action = q->actionCollection()->action(u"view_certificate_overview"_s)) {
            action->setVisible(inMainWindow);
        }
        if (auto action = q->actionCollection()->action(u"pad_view"_s)) {
            action->setCheckable(inMainWindow);
        }
        if (!inMainWindow && ui.padWidget) {
            // the notepad in the main window can't be reached anymore; discard it like
            // a closed notepad window
            if (padViewIsShown()) {
                showCertificateView();
            }
            ui.stackWidget->removeWidget(ui.padWidget);
            ui.padWidget->deleteLater();
            ui.padWidget = nullptr;
        }
        updateViewActions();
    }

#ifdef Q_OS_MACOS
    // Creates the toolbar item for the search field, so that the search bar of the
    // certificate view can be shown in the toolbar like the search fields of macOS applications
    QWidgetAction *createSearchFieldAction()
    {
        ui.searchTab->searchBar()->lineEdit()->addAction(Kleo::MacOS::symbolIcon(u"magnifyingglass"_s), QLineEdit::LeadingPosition);

        searchFieldAction = new SearchFieldAction{ui.searchTab, q};
        searchFieldAction->setText(i18nc("@action:intoolbar", "Search"));
        // moving the search bar changes the layout of the toolbar; therefore, this isn't done
        // while the toolbar is busy showing or hiding its items
        connect(
            searchFieldAction,
            &SearchFieldAction::hostsChanged,
            q,
            [this]() {
                updateSearchBarPlacement();
            },
            Qt::QueuedConnection);
        return searchFieldAction;
    }

    // Shows the search bar in the toolbar if its toolbar item is visible. Otherwise, e.g. if
    // the toolbar is hidden, if the item doesn't fit in the toolbar, or if it was removed from
    // the toolbar, the search bar is shown above the certificate list.
    void updateSearchBarPlacement()
    {
        if (updatingSearchBarPlacement) {
            return;
        }
        auto searchBar = ui.searchTab->searchBar();
        const auto host = searchFieldAction->usableHost(q);
        const auto currentParent = searchBar->parentWidget();
        if (currentParent == (host ? static_cast<QWidget *>(host) : ui.searchTab)) {
            return;
        }
        updatingSearchBarPlacement = true;
        // reparenting takes the focus away
        const QPointer<QWidget> focusWidget = q->focusWidget();
        const bool restoreFocus = focusWidget && searchBar->isAncestorOf(focusWidget);
        if (auto oldHost = qobject_cast<SearchFieldHost *>(currentParent)) {
            oldHost->reserveSpace();
        }
        if (host) {
            host->takeSearchBar();
        } else {
            ui.searchTab->takeBackSearchBar();
        }
        if (restoreFocus && focusWidget) {
            focusWidget->setFocus();
        }
        updatingSearchBarPlacement = false;
    }

    // Makes the color scheme and the appearance of the application fit the widget style
    void updateColorScheme()
    {
        if (updatingColorScheme) {
            return;
        }
        updatingColorScheme = true;
        auto manager = KColorSchemeManager::instance();
        // The native style follows the appearance of the system. A color scheme would only change
        // the colors of some parts of the user interface. Therefore, color schemes are only
        // offered for other styles. The color scheme chosen for other styles is kept in the
        // configuration, so that it's used again when the user switches back to another style.
        const bool nativeStyle = Kleo::MacOS::isNativeStyleActive();
        colorSchemeMenu->menuAction()->setVisible(!nativeStyle);
        QString schemeId;
        if (!nativeStyle) {
            schemeId = KConfigGroup(KSharedConfig::openConfig(), u"UiSettings"_s).readEntry("ColorScheme", QString{});
            if (!manager->indexForSchemeId(schemeId).isValid()) {
                schemeId.clear();
            }
        }
        manager->setAutosaveChanges(false);
        if (schemeId.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            qApp->styleHints()->unsetColorScheme();
#endif
            // KColorSchemeManager uses the default palette instead of the color scheme matching
            // the appearance of the system if a color scheme has been activated before
            qApp->setProperty("KDE_COLOR_SCHEME_PATH", QVariant{});
            manager->activateSchemeId(QString{});
        } else {
            if (manager->activeSchemeId() != schemeId) {
                manager->activateSchemeId(schemeId);
            }
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            // make the parts of the user interface that are drawn by the system, e.g. the title
            // bar, match the color scheme
            const bool isDark = qApp->palette().color(QPalette::Window).lightness() < 128;
            qApp->styleHints()->setColorScheme(isDark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif
        }
        manager->setAutosaveChanges(true);
        // make sure that the active color scheme is checked in the menu
        const QString schemePath = manager->indexForSchemeId(manager->activeSchemeId()).data(KColorSchemeModel::PathRole).toString();
        const auto schemeActions = colorSchemeMenu->actions();
        for (auto action : schemeActions) {
            if (action->data().toString() == schemePath) {
                action->setChecked(true);
            }
        }
        updatingColorScheme = false;
    }

    void setUpMacOSToolBar()
    {
        q->setUnifiedTitleAndToolBarOnMac(true);
        if (auto toolBar = q->toolBar(u"mainToolBar"_s)) {
            toolBar->setMovable(false);
            const auto actions = toolBar->actions();
            for (auto action : actions) {
                action->setIcon(Kleo::MacOS::symbolIconFor(action->icon()));
            }
        }
    }
#endif

    void restartDaemons()
    {
        Kleo::restartGpgAgent();
    }

private:
    void setupActions();

    QAbstractItemView *currentView() const
    {
        return ui.searchTab->tabWidget()->currentView();
    }

    void keyListingDone()
    {
        // don't take the notepad away from the user if it's shown in the main window
        if (padViewIsShown()) {
            return;
        }
        if (KeyCache::instance()->initialized()) {
            showCertificateView();
        }
    }

    void updateColumnMenus()
    {
        auto treeView = dynamic_cast<TreeView *>(ui.searchTab->tabWidget()->currentView());
        if (!treeView) {
            qCDebug(KLEOPATRA_LOG) << __func__ << "treeView is NULL";
            return;
        }
        ui.columnsVisibilityMenuAction->setMenu(treeView->columnVisibilityMenu());
        ui.columnsSortingMenuAction->setMenu(treeView->columnSortingMenu());
    }

private:
    ApplicationPaletteWatcher appPaletteWatcher;
    Kleo::KeyListController controller;
    bool firstShow : 1;
    struct UI {
        CertificateView *searchTab = nullptr;
        WelcomeWidget *welcomeWidget = nullptr;
        PadWidget *padWidget = nullptr;
        QStackedWidget *stackWidget = nullptr;
        KActionMenu *columnsVisibilityMenuAction = nullptr;
        KActionMenu *columnsSortingMenuAction = nullptr;

        explicit UI(MainWindow *q);
    } ui;
    QAction *focusToClickSearchAction = nullptr;
    ClipboardMenu *clipboadMenu = nullptr;
#ifdef Q_OS_MACOS
    SearchFieldAction *searchFieldAction = nullptr;
    bool updatingSearchBarPlacement = false;
    QMenu *colorSchemeMenu = nullptr;
    bool updatingColorScheme = false;
#endif
};

MainWindow::Private::UI::UI(MainWindow *q)
{
    auto mainWidget = new QWidget{q};
    auto mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins({});
    stackWidget = new QStackedWidget{q};

    searchTab = new CertificateView{q};
    stackWidget->addWidget(searchTab);

    new KeyCacheOverlay(mainWidget, q);

    welcomeWidget = new WelcomeWidget{q};
    stackWidget->addWidget(welcomeWidget);

    mainLayout->addWidget(stackWidget);

    q->setCentralWidget(mainWidget);
}

MainWindow::Private::Private(MainWindow *qq)
    : q(qq)
    , controller(q)
    , firstShow(true)
    , ui(q)
{
    Q_SET_OBJECT_NAME(controller);

    AbstractKeyListModel *flatModel = AbstractKeyListModel::createFlatKeyListModel(q);
    AbstractKeyListModel *hierarchicalModel = AbstractKeyListModel::createHierarchicalKeyListModel(q);

    Q_SET_OBJECT_NAME(flatModel);
    Q_SET_OBJECT_NAME(hierarchicalModel);

#if !defined(Q_OS_WIN)
    auto keyExportDragHandler = std::make_shared<KeyExportDragHandler>();
    flatModel->setDragHandler(keyExportDragHandler);
    hierarchicalModel->setDragHandler(keyExportDragHandler);
#endif

    controller.setFlatModel(flatModel);
    controller.setHierarchicalModel(hierarchicalModel);
    controller.setTabWidget(ui.searchTab->tabWidget());

    ui.searchTab->tabWidget()->setFlatModel(flatModel);
    ui.searchTab->tabWidget()->setHierarchicalModel(hierarchicalModel);

    setupActions();

    ui.stackWidget->setCurrentWidget(ui.searchTab);
    applyNotepadSetting();

    connect(&controller, SIGNAL(contextMenuRequested(QAbstractItemView *, QPoint)), q, SLOT(slotContextMenuRequested(QAbstractItemView *, QPoint)));
    connect(KeyCache::instance().get(), &KeyCache::keyListingDone, q, [this]() {
        keyListingDone();
    });
    connect(KeyCache::instance().get(), &KeyCache::keysMayHaveChanged, q, [this]() {
        keyListingDone();
    });

    q->createGUI(QStringLiteral("kleopatra.rc"));

    if (auto helpMenu = q->findChild<KHelpMenu *>()) {
        qCDebug(KLEOPATRA_LOG) << "Hook into the help menu to show the About dialog ourselves";
        connect(helpMenu, &KHelpMenu::showAboutApplication, KleopatraApplication::instance(), &KleopatraApplication::showAboutDialog);
    }

#ifdef Q_OS_MACOS
    setUpMacOSToolBar();
#endif

    // make toolbar buttons accessible by keyboard
    auto toolbar = q->findChild<KToolBar *>();
    if (toolbar) {
        const auto toolbarButtons = toolbar->findChildren<QToolButton *>();
        for (auto b : toolbarButtons) {
            b->setFocusPolicy(Qt::TabFocus);
        }
        // move toolbar and its child widgets before the central widget in the tab order;
        // this is necessary to make Shift+Tab work as expected
        forceSetTabOrder(q, toolbar);
        auto toolbarChildren = toolbar->findChildren<QWidget *>();
        std::for_each(std::rbegin(toolbarChildren), std::rend(toolbarChildren), [toolbar](auto w) {
            forceSetTabOrder(toolbar, w);
        });
    }

    if (auto action = q->actionCollection()->action(QStringLiteral("help_whats_this"))) {
        delete action;
    }

    q->setAcceptDrops(true);

    // set default window size
#ifdef Q_OS_MACOS
    // start with the search bar in the toolbar, so that the default size has room for it
    if (auto host = q->findChild<SearchFieldHost *>()) {
        host->takeSearchBar();
    }
    q->resize(QSize(qMax(1024, q->sizeHint().width()), 500));
#else
    q->resize(QSize(1024, 500));
#endif
    q->setAutoSaveSettings();

    updateSearchBarClickMessage();
    connect(&appPaletteWatcher, &ApplicationPaletteWatcher::paletteChanged, q, [this]() {
        updateStatusBar();
    });
    connect(KleopatraApplication::instance(), &KleopatraApplication::distributionDataChanged, q, [this]() {
        updateStatusBar();
    });
    updateStatusBar();

    if (KeyCache::instance()->initialized()) {
        keyListingDone();
    }

    // delay setting the models to use the key cache so that the UI (including
    // the "Loading certificate cache..." overlay) is shown before the
    // blocking key cache initialization happens
    QMetaObject::invokeMethod(
        q,
        [flatModel, hierarchicalModel]() {
            flatModel->useKeyCache(true, KeyList::AllKeys);
            hierarchicalModel->useKeyCache(true, KeyList::AllKeys);
        },
        Qt::QueuedConnection);
}

MainWindow::Private::~Private()
{
}

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : KXmlGuiWindow(parent, flags)
    , d(new Private(this))
{
}

MainWindow::~MainWindow()
{
}

void MainWindow::Private::setupActions()
{
    KActionCollection *const coll = q->actionCollection();

    const std::vector<action_data> action_data = {
        // see keylistcontroller.cpp for more actions
        // Tools menu
        {
            "tools_start_kwatchgnupg",
            i18n("GnuPG Log Viewer"),
            QString(),
            "org.kde.kwatchgnupg",
            q,
            [this](bool) {
                gnupgLogViewer();
            },
            QString(),
        },
        {
            "tools_debug_view",
            i18n("GnuPG Command Line"),
            QString(),
            "",
            q,
            [this](bool) {
                auto dialog = new DebugDialog(q);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->open();
            },
            QString(),
        },
        {
            "tools_restart_backend",
            i18nc("@action:inmenu", "Restart Background Processes"),
            i18nc("@info:tooltip", "Restart the background processes, e.g. after making changes to the configuration."),
            "view-refresh",
            q,
            [this](bool) {
                restartDaemons();
            },
            {},
        },
    // Help menu
#ifdef Q_OS_WIN
        {
            "help_check_updates",
            i18n("Check for updates"),
            QString(),
            "gpg4win-compact",
            q,
            [this](bool) {
                forceUpdateCheck();
            },
            QString(),
        },
#endif
        // View menu
        {
            "view_certificate_overview",
            i18nc("@action show certificate overview", "Certificates"),
            i18n("Show certificate overview"),
            "view-certificate",
            q,
            [this](bool) {
                showCertificateView();
            },
            QString(),
        },
        {
            "pad_view",
            i18nc("@action show input / output area for encrypting/signing resp. decrypting/verifying text", "Notepad"),
            i18n("Show pad for encrypting/decrypting and signing/verifying text"),
            "note",
            q,
            [this](bool) {
                showPadView();
            },
            QString(),
        },
        {
            "manage_smartcard",
            i18nc("@action show smartcard management view", "Smartcards"),
            i18n("Show smartcard management"),
            "auth-sim-locked",
            q,
            [](bool) {
                KleopatraApplication::instance()->openOrRaiseSmartCardWindow();
            },
            QString(),
        },
        // Settings menu
        {
            "settings_self_test",
            i18n("Perform Self-Test"),
            QString(),
            nullptr,
            q,
            [this](bool) {
                selfTest();
            },
            QString(),
        },
        {
            "configure_groups",
            i18n("Manage Groups..."),
            QString(),
            "group",
            q,
            [this](bool) {
                configureGroups();
            },
            QString(),
        },
        // Toolbar
        {
            "configure_groups_toolbar",
            i18nc("@action:intoolbar", "Groups"),
            QString(),
            "group",
            q,
            [this](bool) {
                configureGroups();
            },
            QString(),
        }};

    make_actions_from_data(action_data, coll);

    if (auto action = coll->action(u"view_certificate_overview"_s)) {
        action->setCheckable(true);
    }

    if (QStandardPaths::findExecutable(u"kwatchgnupg"_s).isEmpty()) {
        if (auto action = coll->action(u"tools_start_kwatchgnupg"_s)) {
            delete action;
        }
    }

    if (!Settings().groupsEnabled()) {
        if (auto action = coll->action(QStringLiteral("configure_groups"))) {
            delete action;
        }
        if (auto action = coll->action(QStringLiteral("configure_groups_toolbar"))) {
            delete action;
        }
    }

    KStandardAction::close(q, SLOT(close()), coll);
    KStandardAction::quit(q, SLOT(closeAndQuit()), coll);
    KStandardAction::configureToolbars(q, SLOT(configureToolbars()), coll);
    KStandardAction::keyBindings(q, SLOT(editKeybindings()), coll);
    KStandardAction::preferences(qApp, SLOT(openOrRaiseConfigDialog()), coll);

    auto manager = KColorSchemeManager::instance();
    KActionMenu *schemeMenu = KColorSchemeMenu::createMenu(manager, q);
    coll->addAction(QStringLiteral("colorscheme_menu"), schemeMenu->menu()->menuAction());
#ifdef Q_OS_MACOS
    colorSchemeMenu = schemeMenu->menu();
    updateColorScheme();
    connect(KleopatraApplication::instance(), &KleopatraApplication::widgetStyleChanged, q, [this]() {
        updateColorScheme();
    });
    // the following connections are made after KColorSchemeManager and KColorSchemeMenu made
    // theirs, so that the color scheme is updated after they have reacted
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, q, [this]() {
        updateColorScheme();
    });
    const auto schemeActions = colorSchemeMenu->actions();
    if (auto group = schemeActions.empty() ? nullptr : schemeActions.front()->actionGroup()) {
        connect(group, &QActionGroup::triggered, q, [this]() {
            updateColorScheme();
        });
    }
    coll->addAction(u"configure_style"_s, KleopatraApplication::instance()->createConfigureStyleAction(q));
    // the toolbar item of the search field can't be triggered
    KActionCollection::setShortcutsConfigurable(coll->addAction(u"search_field"_s, createSearchFieldAction()), false);
#endif

    focusToClickSearchAction = new QAction(i18nc("@action", "Set Focus to Quick Search"), q);
    coll->addAction(QStringLiteral("focus_to_quickseach"), focusToClickSearchAction);
    coll->setDefaultShortcut(focusToClickSearchAction, QKeySequence(Qt::ALT | Qt::Key_Q));
    connect(focusToClickSearchAction, SIGNAL(triggered(bool)), q, SLOT(slotFocusQuickSearch()));
    clipboadMenu = new ClipboardMenu(q);
    clipboadMenu->setMainWindow(q);
    clipboadMenu->clipboardMenu()->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
    clipboadMenu->clipboardMenu()->setPopupMode(QToolButton::InstantPopup);
    coll->addAction(QStringLiteral("clipboard_menu"), clipboadMenu->clipboardMenu());

    ui.columnsVisibilityMenuAction = new KActionMenu(i18nc("@action:inmenu", "Configure columns"), q);
    ui.columnsVisibilityMenuAction->setPopupMode(QToolButton::InstantPopup);
    ui.columnsVisibilityMenuAction->setIcon(QIcon::fromTheme(u"show_table_column"_s));
    ui.columnsVisibilityMenuAction->setMenuRole(QAction::NoRole); // avoid misdetection on MacOS
    coll->addAction(QStringLiteral("columns_menu"), ui.columnsVisibilityMenuAction);

    ui.columnsSortingMenuAction = new KActionMenu(i18nc("@action:inmenu", "Configure sorting"), q);
    ui.columnsSortingMenuAction->setPopupMode(QToolButton::InstantPopup);
    ui.columnsSortingMenuAction->setIcon(QIcon::fromTheme(u"view-sort"_s));
    ui.columnsSortingMenuAction->setMenuRole(QAction::NoRole); // avoid misdetection on MacOS
    coll->addAction(QStringLiteral("columns_sort_menu"), ui.columnsSortingMenuAction);

    connect(
        ui.searchTab->tabWidget(),
        &TabWidget::viewAdded,
        q,
        [this]() {
            connect(ui.searchTab->tabWidget(), &TabWidget::currentViewChanged, q, [this]() {
                updateColumnMenus();
            });
            updateColumnMenus();
        },
        (Qt::ConnectionType)(Qt::SingleShotConnection | Qt::QueuedConnection));

    /* Add additional help actions for documentation */
    const auto compendium = new DocAction(QIcon{u":/gpg4win/gpg4win-compact"_s},
                                          i18n("Gpg4win Compendium"),
                                          i18nc("The Gpg4win compendium is only available"
                                                "at this point (24.7.2017) in german and english."
                                                "Please check with Gpg4win before translating this filename.",
                                                "gpg4win-compendium-en.pdf"),
                                          QStringLiteral("../share/gpg4win"),
                                          QUrl(),
                                          coll);
    coll->addAction(QStringLiteral("help_doc_compendium"), compendium);

    /* Documentation centered around the german approved VS-NfD mode for official
     * RESTRICTED communication. This is only available in some distributions with
     * the focus on official communications. */
    const auto quickguide =
        new DocAction(QIcon::fromTheme(QStringLiteral("help-contextual")),
                      i18n("&Quick Guide Encrypt and Sign"),
                      i18nc("Only available in German and English. Leave to English for other languages.", "encrypt_and_sign_gnupgvsd_en.pdf"),
                      QStringLiteral("../share/doc/gnupg-vsd"),
                      QUrl(),
                      coll);
    coll->addAction(QStringLiteral("help_doc_quickguide"), quickguide);

    coll->addAction(u"help_doc_user_manual"_s,
                    new DocAction(QIcon::fromTheme(u"help-contextual"_s),
                                  i18nc("@action:inmenu", "User Manual"),
                                  i18nc("Only available in German and English. Leave to English for other languages.", "user-manual-en.pdf"),
                                  u"../share/doc/gnupg-vsd"_s,
                                  QUrl(),
                                  coll));

    coll->addAction(u"help_doc_administrator_manual"_s,
                    new DocAction(QIcon::fromTheme(u"help-contextual"_s),
                                  i18nc("@action:inmenu", "Administrator Manual"),
                                  i18nc("Only available in German and English. Leave to English for other languages.", "admin-manual-en.pdf"),
                                  u"../share/doc/gnupg-vsd"_s,
                                  QUrl(),
                                  coll));

    coll->addAction(QStringLiteral("help_doc_symenc"), createSymmetricGuideAction(coll).release());

    const auto groups = new DocAction(QIcon::fromTheme(QStringLiteral("help-contextual")),
                                      i18n("Certificate &Groups"),
                                      i18nc("Only available in German and English. Leave to English for other languages.", "groupfeature_gnupgvsd_en.pdf"),
                                      QStringLiteral("../share/doc/gnupg-vsd"),
                                      QUrl(),
                                      coll);
    coll->addAction(QStringLiteral("help_doc_groups"), groups);

#ifdef Q_OS_WIN
    const auto gpgol =
        new DocAction(QIcon::fromTheme(QStringLiteral("help-contextual")),
                      i18n("&Mail Encryption in Outlook"),
                      i18nc("Only available in German and English. Leave to English for other languages. Only shown on Windows.", "gpgol_outlook_addin_en.pdf"),
                      QStringLiteral("../share/doc/gnupg-vsd"),
                      QUrl(),
                      coll);
    coll->addAction(QStringLiteral("help_doc_gpgol"), gpgol);
#endif

    /* The submenu with advanced topics */
    const auto certmngmnt =
        new DocAction(QIcon::fromTheme(QStringLiteral("help-contextual")),
                      i18n("&Certification Management"),
                      i18nc("Only available in German and English. Leave to English for other languages.", "certification_management_gnupgvsd_en.pdf"),
                      QStringLiteral("../share/doc/gnupg-vsd"),
                      QUrl(),
                      coll);
    coll->addAction(QStringLiteral("help_doc_cert_management"), certmngmnt);

    const auto smartcard =
        new DocAction(QIcon::fromTheme(QStringLiteral("help-contextual")),
                      i18n("&Smartcard Setup"),
                      i18nc("Only available in German and English. Leave to English for other languages.", "smartcard_setup_gnupgvsd_en.pdf"),
                      QStringLiteral("../share/doc/gnupg-vsd"),
                      QUrl(),
                      coll);
    coll->addAction(QStringLiteral("help_doc_smartcard"), smartcard);

    const auto man_gnupg = new DocAction(QIcon::fromTheme(QStringLiteral("help-contextual")),
                                         i18n("GnuPG Command&line"),
                                         QStringLiteral("gnupg_manual_en.pdf"),
                                         QStringLiteral("../share/doc/gnupg-vsd"),
                                         QUrl(QStringLiteral("https://gnupg.org/documentation/manuals/gnupg/")),
                                         coll);
    coll->addAction(QStringLiteral("help_doc_gnupg"), man_gnupg);

    /* The secops */
    const auto approvalmanual =
        new DocAction(QIcon::fromTheme(QStringLiteral("dvipdf")),
                      i18n("Manual for VS-NfD Approval"),
                      i18nc("Only available in German and English. Keep the English file name for other languages.", "handbuch_zulassung_gnupgvsd_en.pdf"),
                      QStringLiteral("../share/doc/gnupg-vsd"),
                      QUrl(),
                      coll);
    coll->addAction(QStringLiteral("help_doc_approval_manual"), approvalmanual);

    const auto vsa =
        new DocAction(QIcon::fromTheme(QStringLiteral("dvipdf")),
                      i18n("SecOps for VS-NfD Approval"),
                      i18nc("Only available in German and English. Keep the English file name for other languages.", "BSI-VSA-10867_ENG_secops.pdf"),
                      QStringLiteral("../share/doc/gnupg-vsd"),
                      QUrl(),
                      coll);
    coll->addAction(QStringLiteral("help_doc_vsa"), vsa);

    q->setStandardToolBarMenuEnabled(true);

    controller.createActions(coll);

    ui.searchTab->tabWidget()->createActions(coll);
}

void MainWindow::Private::slotConfigCommitted()
{
    controller.updateConfig();
    applyNotepadSetting();
    updateStatusBar();
}

bool MainWindow::queryClose()
{
    qCDebug(KLEOPATRA_LOG) << __func__;
    if (d->controller.hasRunningCommands()) {
        if (d->controller.shutdownWarningRequired()) {
            const int ret = KMessageBox::warningTwoActions(this,
                                                           i18n("There are still some background operations ongoing. "
                                                                "These will be terminated when closing the window. "
                                                                "Proceed?"),
                                                           i18n("Ongoing Background Tasks"),
                                                           KGuiItem(i18nc("@action:button", "Quit Now")),
                                                           KGuiItem(i18nc("@action:button", "Do Not Quit")));
            if (ret != KMessageBox::PrimaryAction) {
                return false;
            }
        }
        d->controller.cancelCommands();
        if (d->controller.hasRunningCommands()) {
            // wait for them to be finished:
            setEnabled(false);
            QEventLoop ev;
            QTimer::singleShot(100ms, &ev, &QEventLoop::quit);
            connect(&d->controller, &KeyListController::commandsExecuting, &ev, &QEventLoop::quit);
            ev.exec();
            if (d->controller.hasRunningCommands()) {
                qCWarning(KLEOPATRA_LOG) << "controller still has commands running, this may crash now...";
            }
            setEnabled(true);
        }
    }
    if (isQuitting || qApp->isSavingSession() || qApp->quitOnLastWindowClosed()) {
        d->ui.searchTab->tabWidget()->saveViews();
        return true;
    } else {
        hide();
        return false;
    }
}

void MainWindow::showEvent(QShowEvent *e)
{
    KXmlGuiWindow::showEvent(e);
#ifdef Q_OS_MACOS
    d->updateSearchBarPlacement();
#endif
    if (d->firstShow) {
        d->ui.searchTab->tabWidget()->loadViews(KSharedConfig::openStateConfig(), QStringLiteral("KeyList"));
        d->firstShow = false;
    }

    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }
}

void MainWindow::hideEvent(QHideEvent *e)
{
    savedGeometry = saveGeometry();
    KXmlGuiWindow::hideEvent(e);
}

void MainWindow::importCertificatesFromFile(const QStringList &files)
{
    if (!files.empty()) {
        d->createAndStart<ImportCertificateFromFileCommand>(files);
    }
}

static QStringList extract_local_files(const QMimeData *data)
{
    const QList<QUrl> urls = data->urls();
    // begin workaround KDE/Qt misinterpretation of text/uri-list
    QList<QUrl>::const_iterator end = urls.end();
    if (urls.size() > 1 && !urls.back().isValid()) {
        --end;
    }
    // end workaround
    QStringList result;
    std::transform(urls.begin(), end, std::back_inserter(result), std::mem_fn(&QUrl::toLocalFile));
    result.erase(std::remove_if(result.begin(), result.end(), std::mem_fn(&QString::isEmpty)), result.end());
    return result;
}

static bool can_decode_local_files(const QMimeData *data)
{
    if (!data) {
        return false;
    }
    return !extract_local_files(data).empty();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    qCDebug(KLEOPATRA_LOG);

    if (can_decode_local_files(e->mimeData())) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    qCDebug(KLEOPATRA_LOG);

    if (e->source()) {
        // The event comes from kleopatra itself; we don't want to import in this case.
        return;
    }

    if (!can_decode_local_files(e->mimeData())) {
        return;
    }

    e->setDropAction(Qt::CopyAction);

    const QStringList files = extract_local_files(e->mimeData());

    KleopatraApplication::instance()->handleFiles(files);

    e->accept();
}

void MainWindow::readProperties(const KConfigGroup &cg)
{
    qCDebug(KLEOPATRA_LOG);
    KXmlGuiWindow::readProperties(cg);
    setHidden(cg.readEntry("hidden", false));
}

void MainWindow::saveProperties(KConfigGroup &cg)
{
    qCDebug(KLEOPATRA_LOG);
    KXmlGuiWindow::saveProperties(cg);
    cg.writeEntry("hidden", isHidden());
}

KeyListController *MainWindow::keyListController()
{
    return &d->controller;
}

std::unique_ptr<DocAction> MainWindow::createSymmetricGuideAction(QObject *parent)
{
    return std::make_unique<DocAction>(
        QIcon::fromTheme(QStringLiteral("help-contextual")),
        i18n("&Password-based Encryption"),
        i18nc("Only available in German and English. Leave to English for other languages.", "symmetric_encryption_gnupgvsd_en.pdf"),
        QStringLiteral("../share/doc/gnupg-vsd"),
        QUrl(),
        parent);
}

#include "mainwindow.moc"
#include "moc_mainwindow.cpp"
