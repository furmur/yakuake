#pragma once

#include <QString>
#include <QHash>

class LayoutConfig
{
public:
    struct Tab {
        QString name;
        QString exec;
        Tab(const char *name)
          : name(QString::fromUtf8(name))
        { }
    };
    typedef QList<Tab> TabsList;

    struct TabsGroup {
        QString name;
        TabsList tabs;
        int selected_tab;
        bool locked;
        TabsGroup(const char *name)
          : name(QString::fromUtf8(name)),
            selected_tab(0)
        { }
    };
    typedef QList<TabsGroup> TabsGroupsList;

private:
    QString _path;
    TabsGroupsList _groups;
    int _selected_group;

public:
    LayoutConfig();
    bool read();

    const TabsGroupsList &groups() { return _groups; }
    int active_group() { return _selected_group; }
};
