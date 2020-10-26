#include "layoutconfig.h"

#include <confuse.h>

#include <QDir>
#include <QDebug>
#include <QString>
#include <QTextStream>

const char *section_name_group = "group";
const char *section_name_tab = "tab";
const char *opt_name_selected = "selected";
const char *opt_name_exec = "exec";
const char *opt_name_locked = "locked";

static cfg_opt_t tab_opts[] = {
    CFG_BOOL((char*)opt_name_selected,cfg_false,CFGF_NONE),
    CFG_STR((char*)opt_name_exec,NULL,CFGF_NONE),
    CFG_END()
};

static cfg_opt_t group_opts[] = {
    CFG_BOOL((char*)opt_name_selected,cfg_false,CFGF_NONE),
    CFG_BOOL((char*)opt_name_locked,cfg_true,CFGF_NONE),
    CFG_SEC((char*)section_name_tab,tab_opts, CFGF_MULTI | CFGF_TITLE),
    CFG_END()
};

static cfg_opt_t layout_opts[] = {
    CFG_SEC((char*)section_name_group,group_opts, CFGF_MULTI | CFGF_TITLE),
    CFG_END()
};

#define LOG_BUF_SIZE 2048
void cfg_reader_error(cfg_t *cfg, const char *fmt, va_list ap)
{
    QString s;
    char buf[LOG_BUF_SIZE];
    vsnprintf(buf,LOG_BUF_SIZE,fmt,ap);
    if(cfg->opts->flags & CFGF_TITLE) {
        s.sprintf("%s:%d section %s(%s): %s",
            cfg->filename,
            cfg->line,
            cfg->name,
            cfg->title,
            buf);
    } else {
        s.sprintf("%s:%d section %s: %s",
            cfg->filename,
            cfg->line,
            cfg->name,
            buf);
    }
    qDebug() << s;
}

LayoutConfig::LayoutConfig()
  : _path(QDir::homePath()+QStringLiteral("/.config/.yakuake_layout")),
    _selected_group(0)
{ }

bool LayoutConfig::read()
{
    cfg_t *cfg =  cfg_init(layout_opts, CFGF_NONE);
    cfg_set_error_function(cfg,cfg_reader_error);

    switch(cfg_parse(cfg, _path.toStdString().c_str())) {
    case CFG_SUCCESS:
        break;
    case CFG_FILE_ERROR:
        //qInfo() << "configuration file: " << _path << "could not be read: " << strerror(errno);
        return false;
    case CFG_PARSE_ERROR:
        qDebug() << "configuration file" << _path << "parse error";
        return false;
    default:
        qDebug() << "unexpected error on configuration file" << _path << "processing";
        return false;
    }

    //iterate groups
    for(unsigned int i = 0; i < cfg_size(cfg, section_name_group); i++) {
        cfg_t *group_cfg = cfg_getnsec(cfg,section_name_group,i);
        _groups.push_back(TabsGroup(cfg_title(group_cfg)));

        TabsGroup &g = _groups.back();
        if(cfg_getbool(group_cfg,opt_name_selected))
            _selected_group = i;
        g.locked = cfg_getbool(group_cfg,opt_name_locked);

        //iterate tabs
        for(unsigned int j = 0; j < cfg_size(group_cfg, section_name_tab); j++) {
            cfg_t *tab_cfg = cfg_getnsec(group_cfg,section_name_tab,j);
            g.tabs.push_back(Tab(cfg_title(tab_cfg)));

            Tab &t = g.tabs.back();
            if(cfg_getbool(tab_cfg,opt_name_selected))
                g.selected_tab = j;
            t.exec = QString::fromUtf8(cfg_getstr(tab_cfg,opt_name_exec));
        }
    }

    return true;
}

