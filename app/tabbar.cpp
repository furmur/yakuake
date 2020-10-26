/*
  Copyright (C) 2008-2014 by Eike Hein <hein@kde.org>
  Copyright (C) 2009 by Juan Carlos Torres <carlosdgtorres@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License or (at your option) version 3 or any later version
  accepted by the membership of KDE e.V. (or its successor appro-
  ved by the membership of KDE e.V.), which shall act as a proxy
  defined in Section 14 of version 3 of the license.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see http://www.gnu.org/licenses/.
*/


#include "tabbar.h"
#include "mainwindow.h"
#include "skin.h"
#include "session.h"
#include "sessionstack.h"
#include "settings.h"
#include "layoutconfig.h"

#include <KActionCollection>
#include <KLocalizedString>

#include <QApplication>
#include <QBitmap>
#include <QFontDatabase>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QtDBus/QtDBus>
#include <QToolButton>
#include <QWheelEvent>
#include <QWhatsThis>

#include <QMimeData>
#include <QDrag>
#include <QLabel>

#define DECLARE_CURRENT_GROUP QList<int> &m_tabs = m_groups[active_group].tabs;

#define dbg_QRect(name) dbgQRect(#name,name);

TabGroup::TabGroup(const QString &t, bool is_locked):
    title(t), locked(is_locked), selected_tab(0)
{}

TabBar::TabBar(MainWindow* mainWindow) : QWidget(mainWindow)
{
    //QDBusConnection::sessionBus(.registerObject(QStringLiteral("/yakuake/tabs"), this, QDBusConnection::ExportScriptableSlots);

    setWhatsThis(xi18nc("@info:whatsthis",
                       "<title>Tab Bar</title>"
                       "<para>The tab bar allows you to switch between sessions. You can double-click a tab to edit its label.</para>"));

    m_selectedSessionId = -1;
    m_renamingSessionId = -1;
    m_renamingIndex = -1;

    m_mousePressed = false;
    m_mousePressedIndex = -1;

    m_dropIndicator = 0;

    disable_groups_cfg_update = false;

    m_mainWindow = mainWindow;

    m_skin = mainWindow->skin();
    connect(m_skin, SIGNAL(iconChanged()), this, SLOT(repaint()));

    m_tabContextMenu = new QMenu(this);
    connect(m_tabContextMenu, SIGNAL(hovered(QAction*)), this, SLOT(contextMenuActionHovered(QAction*)));

    m_groupContextMenu = new QMenu(this);
    connect(m_groupContextMenu, SIGNAL(hovered(QAction*)), this, SLOT(contextMenuActionHovered(QAction*)));

    m_toggleKeyboardInputMenu = new QMenu(xi18nc("@title:menu", "Disable Keyboard Input"), this);
    m_toggleMonitorActivityMenu = new QMenu(xi18nc("@title:menu", "Monitor for Activity"), this);
    m_toggleMonitorSilenceMenu = new QMenu(xi18nc("@title:menu", "Monitor for Silence"), this);

    m_sessionMenu = new QMenu(this);
    connect(m_sessionMenu, SIGNAL(aboutToShow()), this, SLOT(readySessionMenu()));

    m_newTabButton = new QToolButton(this);
    m_newTabButton->setFocusPolicy(Qt::NoFocus);
    m_newTabButton->setMenu(m_sessionMenu);
    m_newTabButton->setPopupMode(QToolButton::DelayedPopup);
    m_newTabButton->setToolTip(xi18nc("@info:tooltip", "New Session"));
    m_newTabButton->setWhatsThis(xi18nc("@info:whatsthis", "Adds a new session. Press and hold to select session type from menu."));
    connect(m_newTabButton, SIGNAL(clicked()), this, SIGNAL(newTabRequested()));

    m_newGroupButton = new QToolButton(this);
    m_newGroupButton->setFocusPolicy(Qt::NoFocus);
    m_newGroupButton->setToolTip(i18nc("@info:tooltip", "New Group"));
    m_newGroupButton->setWhatsThis(i18nc("@info:whatsthis", "Adds a new group."));
    connect(m_newGroupButton, SIGNAL(clicked()), this, SLOT(addGroup()));

    m_closeTabButton = new QPushButton(this);
    m_closeTabButton->setFocusPolicy(Qt::NoFocus);
    m_closeTabButton->setToolTip(xi18nc("@info:tooltip", "Close Session"));
    m_closeTabButton->setWhatsThis(xi18nc("@info:whatsthis", "Closes the active session."));
    connect(m_closeTabButton, SIGNAL(clicked()), this, SLOT(closeTabButtonClicked()));

    m_closeGroupButton = new QPushButton(this);
    m_closeGroupButton->setFocusPolicy(Qt::NoFocus);
    m_closeGroupButton->setToolTip(i18nc("@info:tooltip", "Close Group"));
    m_closeGroupButton->setWhatsThis(i18nc("@info:whatsthis", "Closes the active group."));
    connect(m_closeGroupButton, SIGNAL(clicked()), this, SLOT(closeGroup()));

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setFrame(false);
    m_lineEdit->setClearButtonEnabled(false);
    m_lineEdit->setAlignment(Qt::AlignHCenter);
    m_lineEdit->hide();

    connect(m_lineEdit, SIGNAL(editingFinished()), m_lineEdit, SLOT(hide()));
    connect(m_lineEdit, SIGNAL(returnPressed()), this, SLOT(interactiveRenameDone()));

    setAcceptDrops(true);
}

TabBar::~TabBar()
{
}

void TabBar::applySkin()
{
    resize(width(), m_skin->tabBarBackgroundImage().height()*2);

    m_newTabButton->setStyleSheet(m_skin->tabBarNewTabButtonStyleSheet());
    m_newGroupButton->setStyleSheet(m_skin->tabBarNewTabButtonStyleSheet());

    m_closeTabButton->setStyleSheet(m_skin->tabBarCloseTabButtonStyleSheet());
    m_closeGroupButton->setStyleSheet(m_skin->tabBarCloseTabButtonStyleSheet());

    m_newTabButton->move( m_skin->tabBarNewTabButtonPosition().x(),
                          m_skin->tabBarNewTabButtonPosition().y());
    m_closeTabButton->move(width() - m_skin->tabBarCloseTabButtonPosition().x(),
                           m_skin->tabBarCloseTabButtonPosition().y());

    m_newGroupButton->move( m_skin->tabBarNewTabButtonPosition().x(),
                          m_skin->tabBarNewTabButtonPosition().y()+height()/2);
    m_closeGroupButton->move(width() - m_skin->tabBarCloseTabButtonPosition().x(),
                           m_skin->tabBarCloseTabButtonPosition().y()+height()/2);

    repaint();
}

void TabBar::setGroupLocked(int group_id, bool locked)
{
    if(group_id == -1) group_id = active_group;
    if(group_id < 0 || group_id > m_groups.count() -1) return;

    m_groups[group_id].locked = locked;
}

void TabBar::restoreGroupsFromSettings()
{
    LayoutConfig l;
    if(!l.read()) {
        emit newTabRequested();
        return;
    }

    for(const auto &group : l.groups()) {
        //create group
        m_groups.append(TabGroup(group.name,group.locked));
        active_group = m_groups.size()-1;

        //create tabs
        TabGroup &g = m_groups.back();
        if(group.tabs.empty()) {
            emit newTabRequested();
            continue;
        }
        SessionStack* sessionStack = m_mainWindow->sessionStack();
        for(const auto &tab: group.tabs) {
            emit newTabRequested();
            if(tab.name!=QStringLiteral("auto")) {
                setTabTitleInteractive(m_selectedSessionId, tab.name);
            }
            if(!tab.exec.isEmpty())
                sessionStack->runCommand(tab.exec);
        }
        g.selected_tab = group.selected_tab;
    }

    if(m_groups.count())
        selectGroup(l.active_group());
    else
        emit newTabRequested();
}

void TabBar::readyTabContextMenu()
{
    if (m_tabContextMenu->isEmpty())
    {
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("split-left-right")));
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("split-top-bottom")));
        m_tabContextMenu->addSeparator();
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("edit-profile")));
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("rename-session")));
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("toggle-session-prevent-closing")));
        m_tabContextMenu->addMenu(m_toggleKeyboardInputMenu);
        m_tabContextMenu->addMenu(m_toggleMonitorActivityMenu);
        m_tabContextMenu->addMenu(m_toggleMonitorSilenceMenu);
        m_tabContextMenu->addSeparator();
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("move-session-left")));
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("move-session-right")));
        m_tabContextMenu->addSeparator();
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("move-session-left-group")));
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("move-session-right-group")));
        m_tabContextMenu->addSeparator();
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("close-active-terminal")));
        m_tabContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("close-session")));
    }
}

void TabBar::readyGroupContextMenu()
{
    if(m_groupContextMenu->isEmpty())
    {
        m_groupContextMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("toggle-group-prevent-closing")));
    }
}


void TabBar::readySessionMenu()
{
    if (m_sessionMenu->isEmpty())
    {
        m_sessionMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("new-session")));
        m_sessionMenu->addSeparator();
        m_sessionMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("new-session-two-horizontal")));
        m_sessionMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("new-session-two-vertical")));
        m_sessionMenu->addAction(m_mainWindow->actionCollection()->action(QStringLiteral("new-session-quad")));
    }
}

void TabBar::updateMoveActions(int index)
{
    if (index == -1) return;

    DECLARE_CURRENT_GROUP

    m_mainWindow->actionCollection()->action(QStringLiteral("move-session-left"))->setEnabled(false);
    m_mainWindow->actionCollection()->action(QStringLiteral("move-session-right"))->setEnabled(false);

    if (index != m_tabs.indexOf(m_tabs.first()))
        m_mainWindow->actionCollection()->action(QStringLiteral("move-session-left"))->setEnabled(true);

    if (index != m_tabs.indexOf(m_tabs.last()))
        m_mainWindow->actionCollection()->action(QStringLiteral("move-session-right"))->setEnabled(true);
}

void TabBar::updateToggleActions(int sessionId)
{
    if (sessionId == -1) return;

    KActionCollection* actionCollection = m_mainWindow->actionCollection();
    SessionStack* sessionStack = m_mainWindow->sessionStack();

    QAction* toggleAction = actionCollection->action(QStringLiteral("toggle-session-prevent-closing"));
    toggleAction->setChecked(!sessionStack->isSessionClosable(sessionId));

    toggleAction = actionCollection->action(QStringLiteral("toggle-session-keyboard-input"));
    toggleAction->setChecked(!sessionStack->hasTerminalsWithKeyboardInputEnabled(sessionId));

    toggleAction = actionCollection->action(QStringLiteral("toggle-session-monitor-activity"));
    toggleAction->setChecked(!sessionStack->hasTerminalsWithMonitorActivityDisabled(sessionId));

    toggleAction = actionCollection->action(QStringLiteral("toggle-session-monitor-silence"));
    toggleAction->setChecked(!sessionStack->hasTerminalsWithMonitorSilenceDisabled(sessionId));
}

void TabBar::updateGroupToggleActions(int group_id)
{
    if (group_id == -1) return;

    KActionCollection* actionCollection = m_mainWindow->actionCollection();

    QAction* toggleAction = actionCollection->action(QStringLiteral("toggle-group-prevent-closing"));
    toggleAction->setChecked(m_groups[group_id].locked);
}

void TabBar::updateToggleKeyboardInputMenu(int sessionId)
{
    DECLARE_CURRENT_GROUP

    if (!m_tabs.contains(sessionId)) return;

    QAction* toggleKeyboardInputAction = m_mainWindow->actionCollection()->action(QStringLiteral("toggle-session-keyboard-input"));
    QAction* anchor = m_toggleKeyboardInputMenu->menuAction();

    SessionStack* sessionStack = m_mainWindow->sessionStack();

    QStringList terminalIds = sessionStack->terminalIdsForSessionId(sessionId).split(QStringLiteral(","), QString::SkipEmptyParts);

    m_toggleKeyboardInputMenu->clear();

    if (terminalIds.count() <= 1)
    {
        toggleKeyboardInputAction->setText(xi18nc("@action", "Disable Keyboard Input"));
        m_tabContextMenu->insertAction(anchor, toggleKeyboardInputAction);
        m_toggleKeyboardInputMenu->menuAction()->setVisible(false);
    }
    else
    {
        toggleKeyboardInputAction->setText(xi18nc("@action", "For This Session"));
        m_toggleKeyboardInputMenu->menuAction()->setVisible(true);

        m_tabContextMenu->removeAction(toggleKeyboardInputAction);
        m_toggleKeyboardInputMenu->addAction(toggleKeyboardInputAction);

        m_toggleKeyboardInputMenu->addSeparator();

        int count = 0;

        QStringListIterator i(terminalIds);

        while (i.hasNext())
        {
            int terminalId = i.next().toInt();

            ++count;

            QAction* action = m_toggleKeyboardInputMenu->addAction(xi18nc("@action", "For Terminal %1", count));
            action->setCheckable(true);
            action->setChecked(!sessionStack->isTerminalKeyboardInputEnabled(terminalId));
            action->setData(terminalId);
            connect(action, SIGNAL(triggered(bool)), m_mainWindow, SLOT(handleToggleTerminalKeyboardInput(bool)));
        }
    }
}

void TabBar::updateToggleMonitorActivityMenu(int sessionId)
{
    DECLARE_CURRENT_GROUP

    if (!m_tabs.contains(sessionId)) return;

    QAction* toggleMonitorActivityAction = m_mainWindow->actionCollection()->action(QStringLiteral("toggle-session-monitor-activity"));
    QAction* anchor = m_toggleMonitorActivityMenu->menuAction();

    SessionStack* sessionStack = m_mainWindow->sessionStack();

    QStringList terminalIds = sessionStack->terminalIdsForSessionId(sessionId).split(QStringLiteral(","), QString::SkipEmptyParts);

    m_toggleMonitorActivityMenu->clear();

    if (terminalIds.count() <= 1)
    {
        toggleMonitorActivityAction->setText(xi18nc("@action", "Monitor for Activity"));
        m_tabContextMenu->insertAction(anchor, toggleMonitorActivityAction);
        m_toggleMonitorActivityMenu->menuAction()->setVisible(false);
    }
    else
    {
        toggleMonitorActivityAction->setText(xi18nc("@action", "In This Session"));
        m_toggleMonitorActivityMenu->menuAction()->setVisible(true);

        m_tabContextMenu->removeAction(toggleMonitorActivityAction);
        m_toggleMonitorActivityMenu->addAction(toggleMonitorActivityAction);

        m_toggleMonitorActivityMenu->addSeparator();

        int count = 0;

        QStringListIterator i(terminalIds);

        while (i.hasNext())
        {
            int terminalId = i.next().toInt();

            ++count;

            QAction* action = m_toggleMonitorActivityMenu->addAction(xi18nc("@action", "In Terminal %1", count));
            action->setCheckable(true);
            action->setChecked(sessionStack->isTerminalMonitorActivityEnabled(terminalId));
            action->setData(terminalId);
            connect(action, SIGNAL(triggered(bool)), m_mainWindow, SLOT(handleToggleTerminalMonitorActivity(bool)));
        }
    }
}

void TabBar::updateToggleMonitorSilenceMenu(int sessionId)
{
    DECLARE_CURRENT_GROUP

    if (!m_tabs.contains(sessionId)) return;

    QAction* toggleMonitorSilenceAction = m_mainWindow->actionCollection()->action(QStringLiteral("toggle-session-monitor-silence"));
    QAction* anchor = m_toggleMonitorSilenceMenu->menuAction();

    SessionStack* sessionStack = m_mainWindow->sessionStack();

    QStringList terminalIds = sessionStack->terminalIdsForSessionId(sessionId).split(QStringLiteral(","), QString::SkipEmptyParts);

    m_toggleMonitorSilenceMenu->clear();

    if (terminalIds.count() <= 1)
    {
        toggleMonitorSilenceAction->setText(xi18nc("@action", "Monitor for Silence"));
        m_tabContextMenu->insertAction(anchor, toggleMonitorSilenceAction);
        m_toggleMonitorSilenceMenu->menuAction()->setVisible(false);
    }
    else
    {
        toggleMonitorSilenceAction->setText(xi18nc("@action", "In This Session"));
        m_toggleMonitorSilenceMenu->menuAction()->setVisible(true);

        m_tabContextMenu->removeAction(toggleMonitorSilenceAction);
        m_toggleMonitorSilenceMenu->addAction(toggleMonitorSilenceAction);

        m_toggleMonitorSilenceMenu->addSeparator();

        int count = 0;

        QStringListIterator i(terminalIds);

        while (i.hasNext())
        {
            int terminalId = i.next().toInt();

            ++count;

            QAction* action = m_toggleMonitorSilenceMenu->addAction(xi18nc("@action", "In Terminal %1", count));
            action->setCheckable(true);
            action->setChecked(sessionStack->isTerminalMonitorSilenceEnabled(terminalId));
            action->setData(terminalId);
            connect(action, SIGNAL(triggered(bool)), m_mainWindow, SLOT(handleToggleTerminalMonitorSilence(bool)));
        }
    }
}

void TabBar::contextMenuActionHovered(QAction* action)
{
    bool ok = false;

    if (!action->data().isNull())
    {
        int terminalId = action->data().toInt(&ok);

        if (ok) emit requestTerminalHighlight(terminalId);
    }
    else if (!ok)
        emit requestRemoveTerminalHighlight();
}

void TabBar::contextMenuEvent(QContextMenuEvent* event)
{
    if (event->x() < 0) return;

    qDebug() << "contextMenuEvent";
    bool is4group = event->y() > height()/2;

    int index = is4group ? groupAt(event->x()) : tabAt(event->x());

    if (index == -1)
        m_sessionMenu->exec(QCursor::pos());
    else
    {
        if(!is4group){
            readyTabContextMenu();

            updateMoveActions(index);

            int sessionId = sessionAtTab(index);
            updateToggleActions(sessionId);
            updateToggleKeyboardInputMenu(sessionId);
            updateToggleMonitorActivityMenu(sessionId);
            updateToggleMonitorSilenceMenu(sessionId);

            m_mainWindow->setContextDependentActionsQuiet(true);

            QAction* action = m_tabContextMenu->exec(QCursor::pos());

            emit tabContextMenuClosed();

            if (action)
            {
                if (action->isCheckable())
                    m_mainWindow->handleContextDependentToggleAction(action->isChecked(), action, sessionId);
                else
                    m_mainWindow->handleContextDependentAction(action, sessionId);
            }

            m_mainWindow->setContextDependentActionsQuiet(false);
            DECLARE_CURRENT_GROUP
            updateMoveActions(m_tabs.indexOf(m_selectedSessionId));
            updateToggleActions(m_selectedSessionId);
            updateToggleKeyboardInputMenu(m_selectedSessionId);
            updateToggleMonitorActivityMenu(m_selectedSessionId);
            updateToggleMonitorSilenceMenu(m_selectedSessionId);
        } else { //if(!is4group)
            readyGroupContextMenu();

            updateGroupToggleActions(index);

            m_mainWindow->setContextDependentActionsQuiet(true);

            QAction* action = m_groupContextMenu->exec(QCursor::pos());

            emit groupContextMenuClosed();
            if (action)
            {
                if (action->isCheckable())
                    m_mainWindow->handleContextDependentGroupToggleAction(action->isChecked(), action, index);
                else
                    m_mainWindow->handleContextDependentGroupAction(action, index);
            }
            m_mainWindow->setContextDependentActionsQuiet(false);

            updateGroupToggleActions(index);
        }
    }

    QWidget::contextMenuEvent(event);
}

void TabBar::resizeEvent(QResizeEvent* event)
{
    int half_height = height()/2;
    m_newTabButton->move(m_skin->tabBarNewTabButtonPosition().x(), m_skin->tabBarNewTabButtonPosition().y());
    m_closeTabButton->move(width() - m_skin->tabBarCloseTabButtonPosition().x(), m_skin->tabBarCloseTabButtonPosition().y());
    m_newGroupButton->move(m_skin->tabBarNewTabButtonPosition().x(), m_skin->tabBarNewTabButtonPosition().y()+half_height);
    m_closeGroupButton->move(width() - m_skin->tabBarCloseTabButtonPosition().x(), m_skin->tabBarCloseTabButtonPosition().y()+half_height);

    QWidget::resizeEvent(event);
}

void TabBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setPen(m_skin->tabBarTextColor());
    DECLARE_CURRENT_GROUP
    int x_start = m_skin->tabBarPosition().x();
    int x = x_start;
    int y = m_skin->tabBarPosition().y();

    int half_height = height()/2;

    QRect groupsClipRect(x, y + half_height, m_closeTabButton->x() - x, height() - y - half_height);
    painter.setClipRect(groupsClipRect);
    //dbg_QRect(groupsClipRect);
    m_groupWidths.clear();
    for (int index = 0; index < m_groups.count(); ++index)
    {
        x = drawGroupButton(x, y + half_height, index, painter);
        m_groupWidths << x;
    }
    //x = x > tabsClipRect.right() ? tabsClipRect.right() + 1 : x;

    x = x_start;

    QRect tabsClipRect(x, y, m_closeTabButton->x() - x, height() - y - half_height);
    painter.setClipRect(tabsClipRect);
    //dbg_QRect(tabsClipRect);
    m_tabWidths.clear();
    for (int index = 0; index < m_tabs.count(); ++index)
    {
        x = drawButton(x, y, index, painter);
        m_tabWidths << x;
    }

    //const QPixmap& backgroundImage = m_skin->tabBarBackgroundImage();
    const QPixmap& leftCornerImage = m_skin->tabBarLeftCornerImage();
    const QPixmap& rightCornerImage = m_skin->tabBarRightCornerImage();

    x = x > tabsClipRect.right() ? tabsClipRect.right() + 1 : x;

    QRegion backgroundClipRegion(rect());
    backgroundClipRegion = backgroundClipRegion.subtracted(m_newTabButton->geometry());
    backgroundClipRegion = backgroundClipRegion.subtracted(m_closeTabButton->geometry());
    QRect tabsRect(m_skin->tabBarPosition().x(), y, x - m_skin->tabBarPosition().x(),
        height() - m_skin->tabBarPosition().y());
    backgroundClipRegion = backgroundClipRegion.subtracted(tabsRect);
    painter.setClipRegion(backgroundClipRegion);

    painter.drawImage(0, 0, leftCornerImage.toImage());
    QRect leftCornerImageRect(0, 0, leftCornerImage.width(), height());
    backgroundClipRegion = backgroundClipRegion.subtracted(leftCornerImageRect);

    painter.drawImage(width() - rightCornerImage.width(), 0, rightCornerImage.toImage());
    QRect rightCornerImageRect(width() - rightCornerImage.width(), 0, rightCornerImage.width(), height());
    backgroundClipRegion = backgroundClipRegion.subtracted(rightCornerImageRect);

    painter.setClipRegion(backgroundClipRegion);

    //painter.drawTiledPixmap(0, 0, width(), height(), backgroundImage);

    painter.end();
}

int TabBar::drawButton(int x, int y, int index, QPainter& painter)
{
    QString title;
    int sessionId;
    bool selected;
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    int textWidth = 0;

    DECLARE_CURRENT_GROUP
    sessionId = m_tabs.at(index);
    selected = (sessionId == m_selectedSessionId);
    title = m_tabTitles[sessionId];

    if (selected)
    {
        painter.drawPixmap(x, y, m_skin->tabBarSelectedLeftCornerImage());
        x += m_skin->tabBarSelectedLeftCornerImage().width();
    }
    else if (!m_skin->tabBarUnselectedLeftCornerImage().isNull())
    {
        painter.drawPixmap(x, y, m_skin->tabBarUnselectedLeftCornerImage());
        x += m_skin->tabBarUnselectedLeftCornerImage().width();
    }
    else if (index != m_tabs.indexOf(m_selectedSessionId) + 1)
    {
        painter.drawPixmap(x, y, m_skin->tabBarSeparatorImage());
        x += m_skin->tabBarSeparatorImage().width();
    }

    if (selected) font.setBold(true);
    else font.setBold(false);

    painter.setFont(font);

    QFontMetrics fontMetrics(font);
    textWidth = fontMetrics.width(title) + 10;

    int h = height()/2;
    // Draw the Prevent Closing image in the tab button.
    if (m_mainWindow->sessionStack()->isSessionClosable(sessionId) == false)
    {
        if (selected)
            painter.drawTiledPixmap(x, y,
                    m_skin->tabBarPreventClosingImagePosition().x() +
                    m_skin->tabBarPreventClosingImage().width(), h,
                    m_skin->tabBarSelectedBackgroundImage());
        else
            painter.drawTiledPixmap(x, y,
                    m_skin->tabBarPreventClosingImagePosition().x() +
                    m_skin->tabBarPreventClosingImage().width(), h,
                    m_skin->tabBarUnselectedBackgroundImage());

        painter.drawPixmap(x + m_skin->tabBarPreventClosingImagePosition().x(),
                           m_skin->tabBarPreventClosingImagePosition().y(),
                           m_skin->tabBarPreventClosingImage());

        x += m_skin->tabBarPreventClosingImagePosition().x();
        x += m_skin->tabBarPreventClosingImage().width();
    }

    if (selected)
        painter.drawTiledPixmap(x, y, textWidth, h, m_skin->tabBarSelectedBackgroundImage());
    else
        painter.drawTiledPixmap(x, y, textWidth, h, m_skin->tabBarUnselectedBackgroundImage());

    painter.drawText(x, y, textWidth + 1, h + 2, Qt::AlignHCenter | Qt::AlignVCenter, title);

    x += textWidth;

    if (selected)
    {
        painter.drawPixmap(x, m_skin->tabBarPosition().y(), m_skin->tabBarSelectedRightCornerImage());
        x += m_skin->tabBarSelectedRightCornerImage().width();
    }
    else if (!m_skin->tabBarUnselectedRightCornerImage().isNull())
    {
        painter.drawPixmap(x, m_skin->tabBarPosition().y(), m_skin->tabBarUnselectedRightCornerImage());
        x += m_skin->tabBarUnselectedRightCornerImage().width();
    }
    else if (index != m_tabs.indexOf(m_selectedSessionId) - 1)
    {
        painter.drawPixmap(x, m_skin->tabBarPosition().y(), m_skin->tabBarSeparatorImage());
        x += m_skin->tabBarSeparatorImage().width();
    }

    return x;
}


int TabBar::drawGroupButton(int x, int y, int index, QPainter& painter)
{
    QString title;
    bool selected;
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    int textWidth = 0;

    selected = (active_group == index);
    const TabGroup &group = m_groups.at(index);
    title = QString(QStringLiteral("%1 (%2)")).arg(group.title,QString::number(group.tabs.count()));

    if (selected)
    {
        painter.drawPixmap(x, y, m_skin->tabBarSelectedLeftCornerImage());
        x += m_skin->tabBarSelectedLeftCornerImage().width();
    }
    else if (index != active_group + 1)
    {
        painter.drawPixmap(x, y, m_skin->tabBarSeparatorImage());
        x += m_skin->tabBarSeparatorImage().width();
    }

    if (selected) font.setBold(true);
    else font.setBold(false);

    painter.setFont(font);

    QFontMetrics fontMetrics(font);
    textWidth = fontMetrics.width(title) + 10;

    int h = height()/2;

    if (group.locked == true)
    {
        if (selected)
            painter.drawTiledPixmap(x, y,
                    m_skin->tabBarPreventClosingImagePosition().x() +
                    m_skin->tabBarPreventClosingImage().width(), h,
                    m_skin->tabBarSelectedBackgroundImage());
        else
            painter.drawTiledPixmap(x, y,
                    m_skin->tabBarPreventClosingImagePosition().x() +
                    m_skin->tabBarPreventClosingImage().width(), h,
                    m_skin->tabBarUnselectedBackgroundImage());

        painter.drawPixmap(x + m_skin->tabBarPreventClosingImagePosition().x(),
                           h +m_skin->tabBarPreventClosingImagePosition().y(),
                           m_skin->tabBarPreventClosingImage());

        x += m_skin->tabBarPreventClosingImagePosition().x();
        x += m_skin->tabBarPreventClosingImage().width();
    }

    if (selected)
        painter.drawTiledPixmap(x, y, textWidth, h, m_skin->tabBarSelectedBackgroundImage());
    else
        painter.drawTiledPixmap(x, y, textWidth, h, m_skin->tabBarUnselectedBackgroundImage());

    painter.drawText(x, y, textWidth + 1, h + 2, Qt::AlignHCenter | Qt::AlignVCenter, title);

    x += textWidth;

    if (selected)
    {
        painter.drawPixmap(x, m_skin->tabBarPosition().y(), m_skin->tabBarSelectedRightCornerImage());
        x += m_skin->tabBarSelectedRightCornerImage().width();
    }
    else if (index != active_group + 1)
    {
        painter.drawPixmap(x, m_skin->tabBarPosition().y(), m_skin->tabBarSeparatorImage());
        x += m_skin->tabBarSeparatorImage().width();
    }

    return x;
}

int TabBar::tabAt(int x)
{
    for (int index = 0; index < m_tabWidths.count(); ++index)
    {
        if (x >  m_skin->tabBarPosition().x() && x < m_tabWidths.at(index))
            return index;
    }

    return -1;
}

int TabBar::groupAt(int x)
{
    for (int index = 0; index < m_groupWidths.count(); ++index)
    {
        if (x >  m_skin->tabBarPosition().x() && x < m_groupWidths.at(index))
            return index;
    }

    return -1;
}

void TabBar::wheelEvent(QWheelEvent* event)
{
    if (event->delta() < 0)
        selectNextTab();
    else
        selectPreviousTab();
}

void TabBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_lineEdit->isVisible())
        m_lineEdit->hide();

    QWidget::keyPressEvent(event);
}

void TabBar::mousePressEvent(QMouseEvent* event)
{
    if (QWhatsThis::inWhatsThisMode()) return;

    if (event->x() < m_skin->tabBarPosition().x()) return;

    m_mousePressed4Group = event->y() > height()/2;

    int index = m_mousePressed4Group ? groupAt(event->x()) : tabAt(event->x());
    if (index == -1) return;

    if (event->button() == Qt::LeftButton || event->button() == Qt::MidButton)
    {
        m_startPos = event->pos();
        if(!m_mousePressed4Group)
        {
            DECLARE_CURRENT_GROUP
            if (index != m_tabs.indexOf(m_selectedSessionId) || event->button() == Qt::MidButton)
            {
                m_mousePressed = true;
                m_mousePressedIndex = index;
            }
        } else {
            if (index != active_group || event->button() == Qt::MidButton)
            {
                m_mousePressed = true;
                m_mousePressedIndex = index;
            }
        }
        return;
    }

    QWidget::mousePressEvent(event);
}

void TabBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (QWhatsThis::inWhatsThisMode()) return;

    if (event->x() < m_skin->tabBarPosition().x()) return;
    bool is_group = event->y() > height()/2;
    if(is_group != m_mousePressed4Group) return;

    int index = m_mousePressed4Group ? groupAt(event->x()) : tabAt(event->x());

    if (m_mousePressed && m_mousePressedIndex == index)
    {
        if(!m_mousePressed4Group) {
            DECLARE_CURRENT_GROUP
            if (event->button() == Qt::LeftButton && index != m_tabs.indexOf(m_selectedSessionId))
                emit tabSelected(m_tabs.at(index));

            if (event->button() == Qt::MidButton)
                emit tabClosed(m_tabs.at(index));
        } else {
            if (event->button() == Qt::LeftButton && index != active_group){
                selectGroup(index);
                repaint();
            }
        }
    }

    m_mousePressed = false;

    m_startPos.setX(0);
    m_startPos.setY(0);

    QWidget::mouseReleaseEvent(event);
}

void TabBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_startPos.isNull() && ((event->buttons() & Qt::LeftButton) || (event->buttons() & Qt::MidButton)))
    {
        int distance = (event->pos() - m_startPos).manhattanLength();

        if (distance >= QApplication::startDragDistance())
        {
            int index = tabAt(m_startPos.x());

            if (index >= 0 && !m_lineEdit->isVisible())
                startDrag(index);
        }
    }

    QWidget::mouseMoveEvent(event);
}

void TabBar::dragEnterEvent(QDragEnterEvent* event)
{
    TabBar* eventSource = qobject_cast<TabBar*>(event->source());

    if (eventSource)
    {
        event->setDropAction(Qt::MoveAction);
        event->acceptProposedAction();
    }
    else
    {
        drawDropIndicator(-1);
        event->ignore();
    }

    return;
}

void TabBar::dragMoveEvent(QDragMoveEvent* event)
{
    TabBar* eventSource = qobject_cast<TabBar*>(event->source());

    if (eventSource && event->pos().x() > m_skin->tabBarPosition().x() && event->pos().x() < m_closeTabButton->x())
    {
        int index = dropIndex(event->pos());
        DECLARE_CURRENT_GROUP
        if (index == -1)
            index = m_tabs.count();

        drawDropIndicator(index, isSameTab(event));

        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else
    {
        drawDropIndicator(-1);
        event->ignore();
    }

    return;
}

void TabBar::dragLeaveEvent(QDragLeaveEvent* event)
{
    drawDropIndicator(-1);
    event->ignore();

    return;
}

void TabBar::dropEvent(QDropEvent* event)
{
    drawDropIndicator(-1);

    int x = event->pos().x();

    if (isSameTab(event) || x < m_skin->tabBarPosition().x() || x > m_closeTabButton->x())
        event->ignore();
    else
    {
        DECLARE_CURRENT_GROUP
        int targetIndex = dropIndex(event->pos());
        int sourceSessionId = event->mimeData()->text().toInt();
        int sourceIndex = m_tabs.indexOf(sourceSessionId);

        if (targetIndex == -1)
            targetIndex = m_tabs.count() - 1;
        else if (targetIndex < 0)
            targetIndex = 0;
        else if (sourceIndex < targetIndex)
            --targetIndex;

        m_tabs.move(sourceIndex, targetIndex);
        emit tabSelected(m_tabs.at(targetIndex));

        event->accept();
    }

    return;
}

void TabBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    m_mousePressed4Group = event->y() > height()/2;
    if (QWhatsThis::inWhatsThisMode()) return;

    m_lineEdit->hide();

    if (event->x() < 0) return;

    int index = m_mousePressed4Group ? groupAt(event->x()) : tabAt(event->x());

    if (event->button() == Qt::LeftButton)
    {
        if(m_mousePressed4Group){
            if (event->x() <= m_groupWidths.last())
                interactiveGroupRename(index);
            else if (event->x() > m_groupWidths.last())
                addGroup();
        } else {
            DECLARE_CURRENT_GROUP
            if (event->x() <= m_tabWidths.last())
                interactiveRename(m_tabs.at(index));
            else if (event->x() > m_tabWidths.last())
                emit newTabRequested();
        }
    }

    QWidget::mouseDoubleClickEvent(event);
}

void TabBar::leaveEvent(QEvent* event)
{
    m_mousePressed = false;
    drawDropIndicator(-1);
    event->ignore();

    QWidget::leaveEvent(event);
}

void TabBar::addTab(int sessionId, const QString& title)
{
    if(m_groups.empty()){
        _addGroup();
    }

    DECLARE_CURRENT_GROUP
    m_tabs.append(sessionId);

    if (title.isEmpty())
        m_tabTitles.insert(sessionId, standardTabTitle());
    else
        m_tabTitles.insert(sessionId, title);

    emit tabSelected(sessionId);
}

void TabBar::removeTab(int sessionId)
{
    DECLARE_CURRENT_GROUP
    if (sessionId == -1) sessionId = m_selectedSessionId;
    if (sessionId == -1) return;
    if (!m_tabs.contains(sessionId)) return;

    int index = m_tabs.indexOf(sessionId);

    if (m_lineEdit->isVisible() && sessionId == m_renamingIndex)
        m_lineEdit->hide();

    m_tabs.removeAt(index);
    m_tabTitles.remove(sessionId);

    if ((m_tabs.count() == 0))
        if(m_groups.count()==1 || m_groups[active_group].locked)
            emit lastTabClosed();
        else
            closeGroup();
    else
        emit tabSelected(m_tabs.last());
}

void TabBar::interactiveRename(int sessionId)
{
    DECLARE_CURRENT_GROUP

    if (sessionId == -1) return;
    if (!m_tabs.contains(sessionId)) return;

    m_renamingIndex = sessionId;

    int index = m_tabs.indexOf(sessionId);
    int x = index ? m_tabWidths.at(index - 1) : m_skin->tabBarPosition().x();
    int y = m_skin->tabBarPosition().y();
    int width = m_tabWidths.at(index) - x;

    interactiveRename4Group = false;
    m_lineEdit->setText(m_tabTitles[sessionId]);
    m_lineEdit->setGeometry(x-1, y-1, width+3, height()/2+2);
    m_lineEdit->selectAll();
    m_lineEdit->setFocus();
    m_lineEdit->show();
}

void TabBar::interactiveGroupRename(int group_id)
{
    if (group_id < 0 || group_id > m_groups.count()) return;

    m_renamingIndex = group_id;

    m_renamingSessionId = -1;
    int half_height = height()/2;
    int x = group_id ? m_groupWidths.at(group_id - 1) : m_skin->tabBarPosition().x();
    int y = m_skin->tabBarPosition().y() + half_height;
    int width = m_groupWidths.at(group_id) - x;

    interactiveRename4Group = true;
    m_lineEdit->setText(m_groups[group_id].title);
    m_lineEdit->setGeometry(x-1, y-1, width+3, half_height+2);
    m_lineEdit->selectAll();
    m_lineEdit->setFocus();
    m_lineEdit->show();
}

void TabBar::interactiveRenameDone()
{
    if(!interactiveRename4Group)
        setTabTitleInteractive(m_renamingIndex, m_lineEdit->text().trimmed());
    else
        setGroupTitle(m_renamingIndex, m_lineEdit->text().trimmed());
    m_renamingIndex = -1;
}

void TabBar::selectTab(int sessionId)
{
    DECLARE_CURRENT_GROUP

    if (!m_tabs.contains(sessionId)) return;

    m_selectedSessionId = sessionId;

    int tab_index = m_tabs.indexOf(sessionId);
    m_groups[active_group].selected_tab = tab_index;
    updateMoveActions(tab_index);

    updateToggleActions(sessionId);

    repaint();
}

void TabBar::selectNextTab()
{
    DECLARE_CURRENT_GROUP
    int index = m_tabs.indexOf(m_selectedSessionId);
    int newSelectedSessionId = m_selectedSessionId;

    if (index == -1)
        return;
    else if (index == m_tabs.count() - 1)
        newSelectedSessionId = m_tabs.at(0);
    else
        newSelectedSessionId = m_tabs.at(index + 1);

    emit tabSelected(newSelectedSessionId);
}

void TabBar::selectPreviousTab()
{
    DECLARE_CURRENT_GROUP
    int index = m_tabs.indexOf(m_selectedSessionId);
    int newSelectedSessionId = m_selectedSessionId;

    if (index == -1)
        return;
    else if (index == 0)
        newSelectedSessionId = m_tabs.at(m_tabs.count() - 1);
    else
        newSelectedSessionId = m_tabs.at(index - 1);

    emit tabSelected(newSelectedSessionId);
}

void TabBar::selectNextGroup()
{
    if (active_group == m_groups.count() - 1)
        selectGroup(0);
    else
        selectGroup(active_group+1);
}

void TabBar::selectPreviousGroup()
{
    if (active_group == 0)
        selectGroup(m_groups.count() -1);
    else
        selectGroup(active_group -1);
}

void TabBar::moveTabLeft(int sessionId)
{
    DECLARE_CURRENT_GROUP
    if (sessionId == -1) sessionId = m_selectedSessionId;

    int index = m_tabs.indexOf(sessionId);

    if (index < 1) return;

    m_tabs.swap(index, index - 1);

    repaint();

    updateMoveActions(index - 1);
}

void TabBar::moveTabRight(int sessionId)
{
    if (sessionId == -1) sessionId = m_selectedSessionId;
    DECLARE_CURRENT_GROUP
    int index = m_tabs.indexOf(sessionId);

    if (index == -1 || index == m_tabs.count() - 1) return;

    m_tabs.swap(index, index + 1);

    repaint();

    updateMoveActions(index + 1);
}

void TabBar::moveTabLeftGroup(int sessionId)
{
    if(m_groups.size() == 1) return;
    if (sessionId == -1) sessionId = m_selectedSessionId;

    auto &active_tab_group = m_groups[active_group];
    auto dst_group_index = active_group==0 ? m_groups.size()-1 : active_group-1;
    auto &dst_tab_group = m_groups[dst_group_index];
    auto active_tab_index = active_tab_group.tabs.indexOf(sessionId);

    dst_tab_group.tabs.push_back(active_tab_group.tabs[active_tab_index]);
    dst_tab_group.selected_tab = dst_tab_group.tabs.size()-1;

    active_tab_group.tabs.removeAt(active_tab_index);
    if(active_tab_group.selected_tab > active_tab_group.tabs.size() - 1)
        active_tab_group.selected_tab = active_tab_group.tabs.size() - 1;

    selectGroup(dst_group_index);

    repaint();
}

void TabBar::moveTabRightGroup(int sessionId)
{
    if(m_groups.size() == 1) return;
    if (sessionId == -1) sessionId = m_selectedSessionId;

    auto &active_tab_group = m_groups[active_group];
    auto dst_group_index = active_group >= (m_groups.size()-1) ? 0 : active_group + 1;
    auto &dst_tab_group = m_groups[dst_group_index];
    auto active_tab_index = active_tab_group.tabs.indexOf(sessionId);

    dst_tab_group.tabs.push_back(active_tab_group.tabs[active_tab_index]);
    dst_tab_group.selected_tab = dst_tab_group.tabs.size()-1;

    active_tab_group.tabs.removeAt(active_tab_index);
    if(active_tab_group.selected_tab > active_tab_group.tabs.size() - 1)
        active_tab_group.selected_tab = active_tab_group.tabs.size() - 1;

    selectGroup(dst_group_index);

    repaint();
}

void TabBar::moveGroupLeft(int group_id)
{
    if(group_id == -1 ) group_id = active_group;
    if(group_id < 1) return;

    m_groups.swap(group_id,group_id-1);
    selectGroup(group_id-1);

    repaint();
}

void TabBar::moveGroupRight(int group_id)
{
    if(group_id == -1 ) group_id = active_group;
    if(group_id < 0 || group_id == m_groups.count() - 1) return;

    m_groups.swap(group_id,group_id+1);
    selectGroup(group_id+1);

    repaint();
}

void TabBar::closeTabButtonClicked()
{
    emit tabClosed(m_selectedSessionId);
}

QString TabBar::tabTitle(int sessionId)
{
    if (m_tabTitles.contains(sessionId))
        return m_tabTitles[sessionId];
    else
        return QString();
}

void TabBar::setTabTitle(int sessionId, const QString& newTitle)
{
    if (sessionId == -1) return;
    if (!m_tabTitles.contains(sessionId)) return;
    if (m_tabTitlesSetInteractive.value(sessionId, false)) return;

    if (!newTitle.isEmpty())
        m_tabTitles[sessionId] = newTitle;

    update();
}

void TabBar::setTabTitleInteractive(int sessionId, const QString& newTitle)
{
    if (sessionId == -1) return;
    if (!m_tabTitles.contains(sessionId)) return;

    if (!newTitle.isEmpty())
    {
        m_tabTitles[sessionId] = newTitle;
        m_tabTitlesSetInteractive[sessionId] = true;
    }
    else
        m_tabTitlesSetInteractive.remove(sessionId);

    update();
}

void TabBar::setGroupTitle(int group_id, const QString& newTitle)
{
    if (group_id < 0 || group_id > m_groups.count()) return;

    if (!newTitle.isEmpty())
        m_groups[group_id].title = newTitle;

    update();
}

int TabBar::sessionAtTab(int index)
{
    DECLARE_CURRENT_GROUP
    if (index > m_tabs.count() - 1)
        return -1;
    else
        return m_tabs.at(index);
}

void TabBar::_addGroup(const QString& title, bool locked)
{
    if(title.isEmpty())
        m_groups.append(TabGroup(standardGroupTitle(),locked));
    else
        m_groups.append(TabGroup(title,locked));

    active_group = m_groups.size()-1;
    updateGroupToggleActions(active_group);
}

void TabBar::addGroup(const QString& title, bool locked)
{
    _addGroup(title,locked);
    emit newTabRequested();
}

void TabBar::closeGroup()
{
    int next_group;

    if(m_groups.count() < 2) return;

    //QList<int> *grp_ptr = &m_groups[active_group];
    TabGroup &group = m_groups[active_group];
    if(group.locked) return;

    QList<int> m_tabs(group.tabs);

    if(active_group == m_groups.count()-1){
        next_group = active_group-1;
    } else {
        next_group = active_group;
    }

    setUpdatesEnabled(false);
    m_groups.removeAt(active_group);
    m_groupWidths.removeAt(active_group);
    selectGroup(next_group);
    setUpdatesEnabled(true);

    //close sessions
    for(int index = 0; index < m_tabs.size(); index++){
        emit tabClosed(m_tabs.at(index));
    }
}

void TabBar::selectGroup(int group_id){
    active_group = group_id;
    updateGroupToggleActions(group_id);
    DECLARE_CURRENT_GROUP
    if(!m_tabs.count())
        emit newTabRequested();
    else
        emit tabSelected(m_tabs.at(m_groups[active_group].selected_tab));
}

QString TabBar::standardTabTitle()
{
    QString newTitle = makeTabTitle(0);

    bool nameOk;
    int count = 0;

    do
    {
        nameOk = true;

        QHashIterator<int, QString> it(m_tabTitles);

        while (it.hasNext())
        {
            it.next();

            if (newTitle == it.value())
            {
                nameOk = false;
                break;
            }
        }

        if (!nameOk)
        {
            count++;
            newTitle = makeTabTitle(count);
        }
    }
    while (!nameOk);

    return newTitle;
}

QString TabBar::makeTabTitle(int id)
{
    if (id == 0)
    {
        return xi18nc("@title:tab", "Shell");
    }
    else
    {
        return xi18nc("@title:tab", "Shell No. %1", id+1);
    }
}

QString TabBar::standardGroupTitle()
{
    QString newTitle = makeGroupTitle(0);

    bool nameOk;
    int count = 0;

    do
    {
        nameOk = true;
        QListIterator<TabGroup> it(m_groups);
        for(int index = 0; index < m_groups.count();index++)
        {
            if (newTitle == m_groups[index].title)
            {
                nameOk = false;
                break;
            }
        }

        if (!nameOk)
        {
            count++;
            newTitle = makeGroupTitle(count);
        }
    }
    while (!nameOk);

    return newTitle;
}

QString TabBar::makeGroupTitle(int id)
{
    if (id == 0)
    {
        return i18nc("@title:group", "Group");
    }
    else
    {
        return i18nc("@title:group", "Group #%1", id+1);
    }
}

void TabBar::startDrag(int index)
{
    int sessionId = sessionAtTab(index);

    m_startPos.setX(0);
    m_startPos.setY(0);

    int x = index ? m_tabWidths.at(index - 1) : m_skin->tabBarPosition().x();
    int tabWidth = m_tabWidths.at(index) - x;
    QString title = tabTitle(sessionId);

    QPixmap tab(tabWidth, height());
    QColor fillColor(Settings::backgroundColor());

    if (m_mainWindow->useTranslucency())
        fillColor.setAlphaF(qreal(Settings::backgroundColorOpacity()) / 100);

    tab.fill(fillColor);

    QPainter painter(&tab);
    painter.initFrom(this);
    painter.setPen(m_skin->tabBarTextColor());

    drawButton(0, 0, index, painter);
    painter.end();

    QMimeData* mimeData = new QMimeData;
    mimeData->setText(QVariant(sessionId).toString());

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(tab);
    drag->exec(Qt::MoveAction);

    return;
}

void TabBar::drawDropIndicator(int index, bool disabled)
{
    const int arrowSize = 16;
    DECLARE_CURRENT_GROUP
    if (!m_dropIndicator)
    {
        m_dropIndicator = new QLabel(parentWidget());
        m_dropIndicator->resize(arrowSize, arrowSize);
    }

    QIcon::Mode drawMode = disabled ? QIcon::Disabled : QIcon::Normal;
    m_dropIndicator->setPixmap(QIcon(QStringLiteral("arrow-down")).pixmap(arrowSize, arrowSize, drawMode));

    if (index < 0)
    {
        m_dropIndicator->hide();
        return;
    }

    int temp_index;
    if (index == m_tabs.count())
        temp_index = index - 1;
    else
        temp_index = index;

    int x = temp_index ? m_tabWidths.at(temp_index - 1) : m_skin->tabBarPosition().x();
    int tabWidth = m_tabWidths.at(temp_index) - x;
    int y = m_skin->tabBarPosition().y();

    m_dropRect = QRect(x, y - height(), tabWidth, height() - y);
    QPoint pos;

    if (index < m_tabs.count())
        pos = m_dropRect.topLeft();
    else
        pos = m_dropRect.topRight();

    pos.rx() -= arrowSize/2;

    m_dropIndicator->move(mapTo(parentWidget(),pos));
    m_dropIndicator->show();

    return;
}

int TabBar::dropIndex(const QPoint pos)
{
    int index = tabAt(pos.x());
    if (index < 0)
        return index;

    int x = index ? m_tabWidths.at(index - 1) : m_skin->tabBarPosition().x();
    int tabWidth = m_tabWidths.at(index) - x;
    int y = m_skin->tabBarPosition().y();
    m_dropRect = QRect(x, y - height(), tabWidth, height() - y);

    if ((pos.x()-m_dropRect.left()) > (m_dropRect.width()/2))
        ++index;

    DECLARE_CURRENT_GROUP
    if (index == m_tabs.count())
        return -1;

    return index;
}

bool TabBar::isSameTab(const QDropEvent* event)
{
    DECLARE_CURRENT_GROUP
    int index = dropIndex(event->pos());
    int sourceSessionId = event->mimeData()->text().toInt();
    int sourceIndex = m_tabs.indexOf(sourceSessionId);

    bool isLastTab = (sourceIndex == m_tabs.count()-1) && (index == -1);

    if ((sourceIndex == index) || (sourceIndex == index-1) || isLastTab)
        return true;
    else
        return false;
}

void TabBar::dbgQRect(const QString &name, const QRect &r){
    qDebug() << "QRect(" << name << "): " <<
                "x = " << r.x() << ", y = " << r.y() <<
                ", w = " << r.width() << ", h = " << r.height();
}
