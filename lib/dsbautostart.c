
/*-
 * Copyright (c) 2016 Marcel Kaiser. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "dsbautostart.h"

#define N_XDG_DIRS		8
#define PATH_USER_CONFIG_DIR	".config"
#define PATH_USER_AUTOSTART_DIR ".config/autostart"
#define PATH_XDG_AUTOSTART_DIR	"/usr/local/etc/xdg/autostart"

#define ERROR(ret, fmt, ...) do { \
	seterr(fmt, ##__VA_ARGS__); \
	return (ret); \
} while (0)

enum { TYPE_STR, TYPE_BOOL };
static struct df_var_s {
	const char *name;
	char	   type;
	df_key_t   key;
	union {
		char **strval;
		bool *boolval;
	} val;
	bool set;
} df_vars[] = {
	{ "Name",	TYPE_STR,  DF_KEY_NAME,		{ NULL }, false },
	{ "Comment",	TYPE_STR,  DF_KEY_COMMENT,	{ NULL }, false },
	{ "Exec",	TYPE_STR,  DF_KEY_EXEC,	  	{ NULL }, false },
	{ "Hidden",	TYPE_BOOL, DF_KEY_HIDDEN,	{ NULL }, false },
	{ "Terminal",	TYPE_BOOL, DF_KEY_TERMINAL,	{ NULL }, false },
	{ "NotShowIn",	TYPE_STR, DF_KEY_NOT_SHOW_IN,	{ NULL }, false },
	{ "OnlyShowIn",	TYPE_STR, DF_KEY_ONLY_SHOW_IN,	{ NULL }, false }
};

#define N_DF_VARS (sizeof(df_vars) / sizeof(df_vars[0]))

static struct xdg_dir_s {
	int   prio;
	char *path;
} xdg_dirs[N_XDG_DIRS];

static int		cmp(const char *, const char *);
static int		cmp_basenames(const char *path1, const char *path2);
static int		create_xdg_dir_list(void);
static int		create_autostart_dir(void);
static int		set_xdg_config_dirs(void);
static int		df_del(const char *);
static int		df_prio(const char *);
static int		df_save(desktop_file_t *);
static int		df_count_paths(const char *);
static bool		df_str_to_bool(const char *);
static bool		df_exclude(const desktop_file_t *);
static bool		cmp_entries(const entry_t *, const entry_t *);
static bool		entry_changed(const dsbautostart_t *, const entry_t *);
static char		*readln(FILE *fp);
static char		*change_string(char **, char *);
static char		*df_get_val(char *, const char *);
static char		*df_create(desktop_file_t *);
static char		*user_autostart_path(const char *);
static void		init_var_tbl(desktop_file_t *);
static void		get_current_desktop(void);
static void		skip_spaces(char **);
static void		_clearerr(void);
static void		seterr(const char *msg, ...);
static void		free_entries(entry_t *);
static void		df_free(desktop_file_t *);
static entry_t		*copy_entries(const entry_t *);
static entry_t		*entry_add(dsbautostart_t *, desktop_file_t *);
static hist_entry_t	*hist_add(change_history_t *);
static hist_entry_t	*undo(change_history_t *);
static hist_entry_t	*redo(change_history_t *);
static desktop_file_t	*df_new(void);
static desktop_file_t	*df_dup(const desktop_file_t *);
static desktop_file_t	*df_read(const char *);
static desktop_file_t	*df_replace(dsbautostart_t *, entry_t *,
			    desktop_file_t *);
static desktop_file_t	**df_readdir(const char *, desktop_file_t ***);
static desktop_file_t	*extend_desktop_file_list(desktop_file_t ***,
			    desktop_file_t *);

static int  entry_id;
static bool _error = false;
static char errbuf[1024];
static char *xdg_config_home;
static char *xdg_autostart_home;
static char *current_desktop = "";

bool
dsbautostart_error()
{
	return (_error);
}

const char *
dsbautostart_strerror()
{
	return (errbuf);
}

int
dsbautostart_read_desktop_files(dsbautostart_t *as)
{
	int	       i;
	desktop_file_t **list = NULL;

	_clearerr();

	for (i = 0; xdg_dirs[i].path != NULL; i++) {
		if (df_readdir(xdg_dirs[i].path, &list) == NULL) {
			if (errno != ENOENT)
				goto error;
		}
	}
	for (i = 0; list != NULL && list[i] != NULL; i++) {
		if (list[i]->hidden)
			continue;
		if (entry_add(as, list[i]) == NULL) {
			seterr("entry_add()");
			goto error;
		}
	}
	return (0);
error:
	for (i = 0; list != NULL && list[i] != NULL; i++)
		df_free(list[i]);
	if (list != NULL) {
		free(list[i]);
		free(list);
	}
	return (-1);
}

dsbautostart_t *
dsbautostart_init()
{
	dsbautostart_t *as;

	_clearerr();
	get_current_desktop();
	if ((as = malloc(sizeof(dsbautostart_t))) == NULL)
		ERROR(NULL, "malloc()");
	as->cur_entries = NULL;
	as->hist  = malloc(sizeof(change_history_t));
	if (as->hist == NULL)
		ERROR(NULL, "malloc()");
	as->hist->head = malloc(sizeof(hist_entry_t));
	as->hist->tail = malloc(sizeof(hist_entry_t));
	if (as->hist->head == NULL || as->hist->tail == NULL)
		ERROR(NULL, "malloc()");
	as->hist->head->next = as->hist->tail;
	as->hist->tail->prev = as->hist->head;
	as->hist->head->prev = as->hist->tail->next = NULL;
	as->hist->idx = as->hist->head;

	if (xdg_autostart_home == NULL || xdg_config_home == NULL) {
		if (set_xdg_config_dirs() == -1) {
			free(as);
			ERROR(NULL, "set_xdg_config_dirs()");
		}
	}
	if (xdg_dirs[0].path == NULL) {
		if (create_xdg_dir_list() == -1) {
			free(as);
			ERROR(NULL, "create_xdg_dir_list()");
		}
	}
	if (dsbautostart_read_desktop_files(as) == -1)
		return (NULL);
	as->prev_entries = copy_entries(as->cur_entries);
	if (as->prev_entries == NULL)
		return (NULL);
	return (as);
}

int
dsbautostart_entry_set(dsbautostart_t *as, entry_t *entry, const char *cmd,
    const char *name, const char *comment, const char *not_show_in,
    const char *only_show_in, bool terminal)
{
	hist_entry_t   *hentry;
	desktop_file_t *df;
	
	_clearerr();
	if ((hentry = hist_add(as->hist)) == NULL)
		return (-1);
	if ((df = df_new()) == NULL)
		return (-1);
	if (entry->df->path != NULL) {
		if ((df->path = strdup(entry->df->path)) == NULL) {
			seterr("strdup()");
			df_free(df);
			return (-1);
		}
	}
	dsbautostart_df_set_key(df, DF_KEY_EXEC, cmd);
	dsbautostart_df_set_key(df, DF_KEY_NAME, name);
	dsbautostart_df_set_key(df, DF_KEY_COMMENT, comment);
	dsbautostart_df_set_key(df, DF_KEY_TERMINAL, &terminal);
	dsbautostart_df_set_key(df, DF_KEY_NOT_SHOW_IN, not_show_in);
	dsbautostart_df_set_key(df, DF_KEY_ONLY_SHOW_IN, only_show_in);
	entry->exclude = df_exclude(df);
	hentry->action = CHANGE;
	hentry->entry  = entry;
	hentry->df1    = df;
	hentry->df0    = entry->df;

	entry->df = df;

	return (0);
}

entry_t *
dsbautostart_entry_add(dsbautostart_t *as, const char *cmd, const char *name,
	const char *comment, const char *not_show_in, const char *only_show_in,
	bool terminal)
{
	entry_t	       *entry;
	hist_entry_t   *hentry;
	desktop_file_t *df;

	_clearerr();
	if ((df = df_new()) == NULL)
		return (NULL);
	dsbautostart_df_set_key(df, DF_KEY_EXEC, cmd);
	dsbautostart_df_set_key(df, DF_KEY_NAME, name);
	dsbautostart_df_set_key(df, DF_KEY_COMMENT, comment);
	dsbautostart_df_set_key(df, DF_KEY_TERMINAL, &terminal);
	dsbautostart_df_set_key(df, DF_KEY_NOT_SHOW_IN, not_show_in);
	dsbautostart_df_set_key(df, DF_KEY_ONLY_SHOW_IN, only_show_in);

	if ((entry = entry_add(as, df)) == NULL)
		return (NULL);
	if ((hentry = hist_add(as->hist)) == NULL)
		return (NULL);
	hentry->action = ADD;
	hentry->entry  = entry;

	return (entry);
}

entry_t *
dsbautostart_entry_del(dsbautostart_t *as, entry_t *entry)
{
	hist_entry_t *hentry;

	_clearerr();
	if ((hentry = hist_add(as->hist)) == NULL)
		return (NULL);
	hentry->entry  = entry;
	hentry->action = DELETE;
	entry->deleted = true;
	return (entry);
}

entry_t *
dsbautostart_df_add(dsbautostart_t *as, const char *path)
{
	entry_t	       *entry, *ep;
	hist_entry_t   *hentry;
	desktop_file_t *df;

	_clearerr();
	if ((df = df_read(path)) == NULL)
		return (NULL);
	if (df->hidden)
		return (NULL);
	if (df->path != NULL) {
		for (ep = as->cur_entries; ep != NULL; ep = ep->next) {
			if (ep->df->path == NULL)
				continue;
			if (strcmp(ep->df->path, df->path) == 0)
				return (NULL);
			if (cmp_basenames(ep->df->path, df->path) != 0)
				continue;
			if (df->prio > ep->df->prio || ep->deleted)
				df_replace(as, ep, df);
			return (ep);
		}
	}
	if ((entry = entry_add(as, df)) == NULL)
		return (NULL);
	if ((hentry = hist_add(as->hist)) == NULL)
		return (NULL);
	hentry->action = ADD;
	hentry->entry  = entry;

	return (entry);
}

bool
dsbautostart_can_undo(const dsbautostart_t *as)
{
	return (as->hist->idx != as->hist->head);
}

bool
dsbautostart_can_redo(const dsbautostart_t *as)
{
	if (as->hist->idx->next == as->hist->tail)
		return (false);
	return (true);
}

void
dsbautostart_free(dsbautostart_t *as)
{

	free_entries(as->cur_entries);
	free(as);
}

void
dsbautostart_undo(dsbautostart_t *as)
{
	hist_entry_t *hentry;

	if ((hentry = undo(as->hist)) == NULL)
		return;
	switch (hentry->action) {
	case CHANGE:
		hentry->entry->df = hentry->df0;
		break;
	case DELETE:
		hentry->entry->deleted = false;
		break;
	case ADD:
		hentry->entry->deleted = true;
		break;
	}
}

void
dsbautostart_redo(dsbautostart_t *as)
{
	hist_entry_t *hentry;

	if ((hentry = redo(as->hist)) == NULL)
		return;
	switch (hentry->action) {
	case CHANGE:
		hentry->entry->df = hentry->df1;
		break;
	case DELETE:
		hentry->entry->deleted = true;
		break;
	case ADD:
		hentry->entry->deleted = false;
		break;
	}
}

bool
dsbautostart_changed(const dsbautostart_t *as)
{
	size_t	cur_entries_cnt, prev_entries_cnt;
	entry_t *ep0, *ep1;

	cur_entries_cnt = prev_entries_cnt = 0;
	for (ep0 = as->prev_entries; ep0 != NULL; ep0 = ep0->next) {
		prev_entries_cnt++;
		for (ep1 = as->cur_entries; ep1 != NULL; ep1 = ep1->next) {
			if (ep0->id != ep1->id)
				continue;
			cur_entries_cnt++;
			if (!cmp_entries(ep0, ep1))
				return (true);
		}
	}
	if (cur_entries_cnt != prev_entries_cnt)
		return (true);
	cur_entries_cnt = 0;
	for (ep1 = as->cur_entries; ep1 != NULL; ep1 = ep1->next) {
		if (ep1->deleted)
			continue;
		cur_entries_cnt++;
	}
	if (cur_entries_cnt != prev_entries_cnt)
		return (true);
	return (false);
}

int
dsbautostart_save(dsbautostart_t *as)
{
	bool	       saved, hidden;
	char	       *path;
	entry_t	       *ep;
	desktop_file_t *df;

	_clearerr();

	saved = false;
	for (ep = as->cur_entries; ep != NULL; ep = ep->next) {
		if (ep->deleted) {
			if (ep->df->path != NULL) {
				if (df_del(ep->df->path) == -1)
					return (-1);
			}
			continue;
		}
		/*
		 * If there is a /usr/local/etc/xdg/autostart/foo.desktop
		 * which was "deleted" by creating $XDG_CONFIG_HOME/autostart/
		 * foo.desktop with the "Hidden" field set to true, but then
		 * /usr/local/etc/xdg/autostart/foo.desktop was re-added, we
		 * have to delete $XDG_CONFIG_HOME/autostart/foo.desktop.
		 */
		if (ep->df->path != NULL) {
			if ((path = user_autostart_path(ep->df->path)) == NULL)
				return (-1);
			if ((df = df_read(path)) != NULL) {
				hidden = df->hidden;
				df_free(df);
				if (hidden && df_del(path) == -1) {
					free(path);
					return (-1);
				}
			} else if (errno != ENOENT) {
				seterr("df_read(%s)", path);
				free(path);
				return (-1);
			}
			free(path);
		}
		if (entry_changed(as, ep) || ep->df->path == NULL) {
			if (df_save(ep->df) == -1)
				return (-1);
			saved = true;
		}
	}
	if (saved) {
		free_entries(as->prev_entries);
		as->prev_entries = copy_entries(as->cur_entries);
		if (as->prev_entries == NULL && _error)
			return (-1);
	}
	return (0);
}

int
dsbautostart_df_set_key(desktop_file_t *df, df_key_t key, const void *val)
{

	_clearerr();
	if (val == NULL)
		return (-1);
	switch (key) {
	case DF_KEY_NAME:
		return (change_string(&df->name, (char *)val) != NULL ? 0 : -1);
	case DF_KEY_EXEC:
		return (change_string(&df->exec, (char *)val) != NULL ? 0 : -1);
	case DF_KEY_COMMENT:
		return (change_string(&df->comment, (char *)val) != NULL ? 0 : -1);
	case DF_KEY_HIDDEN:
		df->hidden = *(bool *)val;
		return (0);
	case DF_KEY_TERMINAL:
		df->terminal = *(bool *)val;
		return (0);
	case DF_KEY_NOT_SHOW_IN:
		return (change_string(&df->not_show_in, (char *)val) != NULL ? 0 : -1);
	case DF_KEY_ONLY_SHOW_IN:
		return (change_string(&df->only_show_in, (char *)val) != NULL ? 0 : -1);
	default:
		return (-1);
	}
	return (-1);
}

static desktop_file_t *
df_read(const char *path)
{
	FILE	       *fp;
	char	       *val, *ln, *_path;
	bool	       found_desktop_entry;
	size_t	       i;
	desktop_file_t *df;

	_clearerr();
	if ((_path = realpath(path, NULL)) == NULL)
		return (NULL);
	if ((fp = fopen(_path, "r")) == NULL && errno != ENOENT)
		ERROR(NULL, "fopen(%s)", _path);
	if ((df = df_new()) == NULL)
		return (NULL);
	init_var_tbl(df);
	found_desktop_entry = false;
	while ((ln = readln(fp)) != NULL) {
		skip_spaces(&ln);
		if (*ln == '\0' || *ln == '#')
			continue;
		if (!found_desktop_entry) {
			if (strcmp(ln, "[Desktop Entry]") == 0)
				found_desktop_entry = true;
			continue;
		}
		for (i = 0; i < N_DF_VARS; i++) {
			if (df_vars[i].type == TYPE_STR) {
				val = df_get_val(ln, df_vars[i].name);
				if (val == NULL)
					continue;
				*df_vars[i].val.strval = strdup(val);
				if (*df_vars[i].val.strval == NULL) {
					seterr("strdup()");
					(void)fclose(fp);
					df_free(df);
					return (NULL);
				}
			} else {
				val = df_get_val(ln, df_vars[i].name);
				if (val == NULL)
					continue;
				*df_vars[i].val.boolval = df_str_to_bool(val);
			}
		}
	}
	(void)fclose(fp);
	if (!found_desktop_entry) {
		errno = 0;
		df_free(df);
		return (NULL);
	}
	if ((df->path = strdup(_path)) == NULL) {
		seterr("strdup()");
		df_free(df);
		return (NULL);
	}
	df->prio = df_prio(df->path);

	return (df);
}

static char *
df_create(desktop_file_t *df)
{
	int	   fd;
	FILE	   *fp;
	char	   *tmp, name[_POSIX_PATH_MAX];
	size_t	   len, i;
	const char template[] = "XXXXXX";

	_clearerr();

	init_var_tbl(df);
	if (create_autostart_dir() == -1)
		return (NULL);
	(void)snprintf(name, sizeof(name), "%s-%s", PROGRAM, template);
	if ((tmp = user_autostart_path(name)) == NULL)
		return (NULL);
	len = strlen(tmp) + sizeof(".desktop");
	if ((df->path = malloc(len)) == NULL) {
		seterr("malloc()");
		goto error;
	}
	if ((fd = mkstemp(tmp)) == -1) {
		seterr("mkstemp(%s)", tmp);
		goto error;
	}
	(void)snprintf(df->path, len, "%s.desktop", tmp);
	if ((fp = fdopen(fd, "r+")) == NULL) {
		seterr("fdopen()");
		goto error;
	}
	(void)fprintf(fp, "[Desktop Entry]\nType=Application\n");
	for (i = 0; i < N_DF_VARS; i++) {
		if (df_vars[i].type == TYPE_STR) {
			if (*df_vars[i].val.strval == NULL)
				continue;
			(void)fprintf(fp, "%s=%s\n", 
			    df_vars[i].name, *df_vars[i].val.strval);
		} else {
			(void)fprintf(fp, "%s=%s\n", df_vars[i].name,
			    *df_vars[i].val.boolval ? "true" : "false");
		}
	}
	(void)fclose(fp);
	if (rename(tmp, df->path) == -1) {
		seterr("rename(%s, %s)", tmp, df->path);
		goto error;
	}
	free(tmp);

	return (df->path);
error:
	free(tmp);
	free(df->path);

	return (NULL);
}

static void
_clearerr()
{
	_error = false;
}

static void
seterr(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);

	_error = true;
	(void)vsnprintf(errbuf, sizeof(errbuf), msg, ap);
	
	if (errno == 0)
		return;
	(void)snprintf(errbuf + strlen(errbuf), sizeof(errbuf) - strlen(errbuf),
	    ": %s", strerror(errno));
}

static char *
readln(FILE *fp)
{
	static char   *p, *buf = NULL;
	static size_t bsize = 0, slen = 0, rd, len = 0;
	
	for (errno = 0;;) {
		if (bsize == 0 || len == bsize - 1) {
			buf = realloc(buf, bsize + _POSIX2_LINE_MAX);
			bsize += _POSIX2_LINE_MAX;
		}
		if (slen > 0)
			(void)memmove(buf, buf + slen, len + 1), slen = 0;
		if (len > 0 && (p = strchr(buf, '\n')) != NULL) {
			slen = p - buf + 1; buf[slen - 1] = '\0'; len -= slen;
			return (buf);
		}
		if ((rd = fread(buf + len, 1, bsize - len - 1, fp)) == 0) {
			if (!ferror(fp) && len > 0) {
				len = 0;
				return (buf);
			}
			return (NULL);
		}
		len += rd; buf[len] = '\0';
	}
}

static void
free_entries(entry_t *entries)
{
	entry_t *next;

	for (; entries != NULL; entries = next) {
		df_free(entries->df);
		next = entries->next;
		free(entries);
	}
}

/*
 * Add a new hist_entry_t object to the given history, and
 * return a pointer to it. 
 */
static hist_entry_t *
hist_add(change_history_t *hist)
{
	hist_entry_t *hentry;

	if ((hentry = malloc(sizeof(hist_entry_t))) == NULL)
		ERROR(NULL, "malloc()");
	hentry->prev = hentry->next = NULL;
	hist->idx->next->prev = hentry;
	hentry->prev = hist->idx;
	hentry->next = hist->idx->next;
	hist->idx->next = hentry;
	hist->idx = hentry;

	return (hentry);
}

static hist_entry_t *
undo(change_history_t *hist)
{
	hist_entry_t *hentry;

	if (hist->idx == hist->head)
		return (NULL);
	hentry = hist->idx;
	hist->idx = hist->idx->prev;

	return (hentry);
}

static hist_entry_t *
redo(change_history_t *hist)
{
	hist_entry_t *hentry;

	if (hist->idx->next == hist->tail)
		return (NULL);
	hentry = hist->idx->next;
	hist->idx = hist->idx->next;

	return (hentry);
}

static entry_t *
entry_add(dsbautostart_t *as, desktop_file_t *df)
{
	entry_t *entry, *tail;

	for (tail = as->cur_entries; tail != NULL && tail->next != NULL;
	    tail = tail->next)
		;
	if ((entry = malloc(sizeof(entry_t))) == NULL)
		ERROR(NULL, "malloc()");
	if (tail == NULL) {
		as->cur_entries = entry;
		entry->next = entry->prev = NULL;
	} else {
		entry->next = NULL;
		entry->prev = tail;
		tail->next = entry;
	}
	entry->exclude = df_exclude(df);
	entry->df = df;
	entry->id = entry_id++;
	entry->deleted = false;

	return (entry);
}

static bool
cmp_entries(const entry_t *e0, const entry_t *e1)
{

	if (cmp(e0->df->exec, e1->df->exec) != 0		||
	    cmp(e0->df->name, e1->df->name) != 0		||
	    cmp(e0->df->comment, e1->df->comment) != 0		||
	    cmp(e0->df->not_show_in, e1->df->not_show_in) != 0	||
	    cmp(e0->df->only_show_in, e1->df->only_show_in) != 0)
		return (false);
	if ((e0->df->terminal && !e1->df->terminal) ||
	    (!e0->df->terminal && e1->df->terminal))
		return (false);
	if ((e0->deleted && !e1->deleted) || (!e0->deleted && e1->deleted))
		return (false);
	return (true);
}

static entry_t *
copy_entries(const entry_t *entries)
{
	entry_t *cur, *new, *head;

	if (entries == NULL)
		return (NULL);
	head = cur = new = NULL;
	for (; entries != NULL; entries = entries->next) {
		if ((new = malloc(sizeof(entry_t))) == NULL) {
			free_entries(head);
			ERROR(NULL, "malloc()");
		}
		if (head == NULL)
			cur = head = new;
		cur->next = new;
		new->prev = cur;
		new->next = NULL;
		if ((new->df = df_dup(entries->df)) == NULL) {
			free_entries(head);
			ERROR(NULL, "df_dup()");
		}
		new->id = entries->id;
		new->deleted = entries->deleted;
		cur = new;
	}
	return (head);
}

static void
init_var_tbl(desktop_file_t *df)
{
	size_t i;

	for (i = 0; i < N_DF_VARS; i++) {
		assert(df_vars[i].key == i);
		df_vars[i].set = false;
	}
	df_vars[DF_KEY_NAME].val.strval		 = &df->name;
	df_vars[DF_KEY_COMMENT].val.strval	 = &df->comment;
	df_vars[DF_KEY_EXEC].val.strval		 = &df->exec;
	df_vars[DF_KEY_HIDDEN].val.boolval	 = &df->hidden;
	df_vars[DF_KEY_TERMINAL].val.boolval	 = &df->terminal;
	df_vars[DF_KEY_NOT_SHOW_IN].val.strval	 = &df->not_show_in;
	df_vars[DF_KEY_ONLY_SHOW_IN].val.strval  = &df->only_show_in;
}

static void
skip_spaces(char **s)
{
	size_t n;
	n = strspn(*s, "\t ");
	(*s) += n;
}

static char *
df_get_val(char *s, const char *varname)
{
	size_t len = strlen(varname);

	if (strncmp(s, varname, len) != 0)
		return (NULL);
	s += len;
	skip_spaces(&s);
	if (*s++ != '=')
		return (NULL);
	skip_spaces(&s);

	return (s);
}

static bool
df_str_to_bool(const char *s)
{
	if (strcmp(s, "true") == 0)
		return (true);
	return (false);
}

static char *
change_string(char **str, char *val)
{
	char *newstr;

	if ((newstr = strdup(val)) == NULL)
		ERROR(NULL, "strdup()");
	free(*str); *str = newstr;

	return (*str);
}

static int
cmp(const char *str1, const char *str2)
{
	if (str1 == str2)
		return (0);
	if (str1 == NULL || str2 == NULL)
		return (-1);

	return (strcmp(str1, str2));
}

static int
cmp_basenames(const char *path1, const char *path2)
{
	const char *p1, *p2;
	
	if ((p1 = strrchr(path1, '/')) != NULL)
		p1++;
	else
		p1 = path1;
	if ((p2 = strrchr(path2, '/')) != NULL)
		p2++;
	else
		p2 = path2;
	return (strcmp(p1, p2));
}

static void
get_current_desktop()
{
	char *p;

	if ((p = getenv("XDG_CURRENT_DESKTOP")) != NULL)
		current_desktop = p;
}

/*
 * Create a full path of the given filename under $XDG_CONFIG_HOME/autostart.
 * If the filename is a full path, its basename is used. The returned path
 * must be free()'d by the caller if not needed anymore.
 */
static char *
user_autostart_path(const char *dfname)
{
	char	   *path;
	size_t	   len;
	const char *fname;
	
	if ((fname = strrchr(dfname, '/')) != NULL)
		fname++;
	else
		fname = dfname;
	len = strlen(xdg_autostart_home) + strlen(fname) + 2;
	if ((path = malloc(len)) == NULL)
		ERROR(NULL, "malloc()");
	(void)snprintf(path, len, "%s/%s", xdg_autostart_home, fname);

	return (path);
}

static int
set_xdg_config_dirs()
{
	char	      *dir;
	size_t	      len;
	struct passwd *pwd;
	
	if ((dir = getenv("XDG_CONFIG_HOME")) != NULL) {
		xdg_config_home = strdup(dir);
		if (xdg_config_home == NULL)
			ERROR(-1, "strdup()");
	} else {
		pwd = getpwuid(getuid());
		endpwent();
		if (pwd == NULL)
			ERROR(-1, "getpwuid(%u)", getuid());
		endpwent();
		len = strlen(pwd->pw_dir) + sizeof("/.config");
		if ((xdg_config_home = malloc(len)) == NULL)
			ERROR(-1, "malloc()");
		(void)snprintf(xdg_config_home, len, "%s/.config",
		    pwd->pw_dir);
	}
	len = strlen(xdg_config_home) + sizeof("/autostart");
	if ((xdg_autostart_home = malloc(len)) == NULL)
		ERROR(-1, "malloc()");
	(void)snprintf(xdg_autostart_home, len, "%s/autostart",
	    xdg_config_home);
	return (0);
}

static int
create_xdg_dir_list()
{
	char   *path, *dir, *dirs, *last;
	size_t len, n;
	
	n = 0;
	xdg_dirs[n].path   = xdg_autostart_home;
	xdg_dirs[n++].prio = N_XDG_DIRS;

	if ((dir = getenv("XDG_CONFIG_DIRS")) == NULL) {
		xdg_dirs[n].path   = PATH_XDG_AUTOSTART_DIR;
		xdg_dirs[n++].prio = 0;
	} else {
		if ((dirs = strdup(dir)) == NULL)
			ERROR(-1, "strdup()");
		for (dir = dirs; (dir = strtok_r(dir, ":", &last)) != NULL;
		    dir = NULL) {
			if (n >= N_XDG_DIRS) {
				warnx("# of XDG dirs exceeded");
				break;
			}
			len = strlen(dir) + sizeof("/autostart");
			if ((path = malloc(len)) == NULL)
				ERROR(-1, "malloc()");
			(void)snprintf(path, len, "%s/autostart", dir);
			xdg_dirs[n].path = path;
			xdg_dirs[n].prio = xdg_dirs[n - 1].prio + 1;
			n++;
		}
		free(dirs);
	}
	xdg_dirs[n].path = NULL;
	xdg_dirs[n].prio = -1;

	return (0);
}

static int
create_autostart_dir()
{
	char *path, *dir, *buf;

	if ((path = strdup(xdg_autostart_home)) == NULL)
		ERROR(-1, "strdup()");
	if ((buf = malloc(strlen(path) + 1)) == NULL)
		ERROR(-1, "malloc()");
	buf[0] = '\0';
	for (dir = path; (dir = strtok(dir, "/")) != NULL; dir = NULL) {
		(void)strcat(buf, "/");
		(void)strcat(buf, dir);
		if (mkdir(buf, S_IRWXU) == -1 && errno != EEXIST) {
			free(path); free(buf);
			ERROR(-1, "mkdir()");
		}
	}
	free(path); free(buf);
	return (0);
}

static desktop_file_t *
df_new()
{
	desktop_file_t *df;
	
	if ((df = malloc(sizeof(desktop_file_t))) == NULL)
		ERROR(NULL, "malloc()");
	df->name = df->exec = df->path = df->comment = NULL;
	df->not_show_in = df->only_show_in = NULL;
	df->terminal = df->hidden = false;
	df->prio = -1;

	return (df);
}

static void
df_free(desktop_file_t *df)
{
	free(df->name);
	free(df->exec);
	free(df->comment);
	free(df->path);
	free(df->not_show_in);
	free(df->only_show_in);
	free(df);
}

static desktop_file_t *
df_dup(const desktop_file_t *df)
{
	desktop_file_t *cp;

	_clearerr();
	if ((cp = df_new()) == NULL)
		return (NULL);
	if (df->exec != NULL) {
		if (dsbautostart_df_set_key(cp, DF_KEY_EXEC, df->exec) == -1)
			goto error;
	}
	if (df->name != NULL) {
		if (dsbautostart_df_set_key(cp, DF_KEY_NAME, df->name) == -1)
			goto error;
	}
	if (df->comment != NULL) {
		if (dsbautostart_df_set_key(cp, DF_KEY_COMMENT,
		    df->comment) == -1)
			goto error;
	}
	if (df->only_show_in != NULL) {
		if (dsbautostart_df_set_key(cp, DF_KEY_ONLY_SHOW_IN, df->only_show_in) == -1)
			goto error;
	}
	if (df->not_show_in != NULL) {
		if (dsbautostart_df_set_key(cp, DF_KEY_NOT_SHOW_IN, df->not_show_in) == -1)
			goto error;
	}
	dsbautostart_df_set_key(cp, DF_KEY_TERMINAL, &df->terminal);
	if (df->path != NULL) {
		if ((cp->path = strdup(df->path)) == NULL)
			goto error;
	}
	cp->prio = df->prio;

	return (cp);
error:
	df_free(cp);
	return (NULL);
}

static int
df_prio(const char *path)
{
	int    i;
	char   *dir;
	size_t len;

	if (xdg_dirs[0].path == NULL) {
		if (create_xdg_dir_list() == -1)
			ERROR(-1, "create_xdg_dir_list()");
	}
	dir = strrchr(path, '/');
	if (dir == NULL)
		return (-1);
	len = dir - path;
	for (i = 0; xdg_dirs[i].path != NULL; i++) {
		if (strncmp(path, xdg_dirs[i].path, len) == 0)
			return (xdg_dirs[i].prio);
	}
	return (-1);
}

static int
df_del(const char *path)
{
	desktop_file_t *df;

	/*
	 * Check if there is not more than one instance of the given
	 * desktop file. If this is the case, and we are allowed to
	 * delete it, we are done.
	 */
	if (df_count_paths(path) <= 1) {
		if (unlink(path) == 0)
			return (0);
		else if (errno != EPERM && errno != EACCES)
			ERROR(-1, "unlink()");
	}
	/* 
	 * System wide desktop file we can't delete. Create a desktop
	 * file of the same name in the user's autostart dir with the
	 * "Hidden" key set to true.
	 */
	if ((df = df_read(path)) == NULL)
		return (-1);
	free(df->path);
	df->path   = user_autostart_path(path);
	df->hidden = true;

	if (df->path == NULL)
		goto error;
	if (df_save(df) == -1)
		goto error;
	df_free(df);

	return (0);
error:
	df_free(df);
	return (-1);
}

static desktop_file_t *
df_replace(dsbautostart_t *as, entry_t *entry, desktop_file_t *df)
{
	hist_entry_t *hentry;
	
	if ((hentry = hist_add(as->hist)) == NULL)
		return (NULL);
	hentry->df0    = entry->df;
	hentry->df1    = df;
	hentry->entry  = entry;
	hentry->action = CHANGE;
	entry->df = df;

	return (df);
}

static int
df_count_paths(const char *path)
{
	int	   i, fd, count;
	const char *file;

	errno = 0;
	if ((file = strrchr(path, '/')) != NULL)
		file++;
	else
		file = path;
	for (i = count = 0; xdg_dirs[i].path != NULL; i++) {
		fd = open(xdg_dirs[i].path, O_RDONLY, 0);
		if (fd == -1 && errno != ENOENT)
			ERROR(-1, "open(%s)", xdg_dirs[i].path);
		if (faccessat(fd, file, F_OK, 0) == -1) {
			if (errno != ENOENT)
				ERROR(-1, "faccess(%s)", xdg_dirs[i].path);
		} else
			count++;
		(void)close(fd);
	}
	return (count);
}

static const char *
next_list_item(const char *str, size_t *len)
{
	const char	  *p;
	static const char *prev = NULL;

	if (str != NULL)
		prev = str;
	else
		str = prev;
	if (str == NULL || *str == '\0')
		return (NULL);
	while (*str == ';')
		str++;
	p = strchr(str, ';');
	if (p == NULL)
		p = strchr(str, '\0');
	*len = p - str;
	if (*len == 0)
		return (NULL);
	prev = ++p;

	return (str);
}

static bool
df_exclude(const desktop_file_t *df)
{
	size_t	   len;
	const char *p;

	if (df->not_show_in != NULL) {
		p = df->not_show_in;
		while ((p = next_list_item(p, &len)) != NULL) {
			if (strncmp(p, current_desktop, len) == 0)
				return (true);
			p = NULL;
		}
	}
	if (df->only_show_in != NULL) {
		p = df->only_show_in;
		while ((p = next_list_item(p, &len)) != NULL) {
			if (strncmp(p, current_desktop, len) == 0)
				return (false);
			p = NULL;
		}
		return (true);
	}
	return (false);
}

static int
df_save(desktop_file_t *df)
{
	int	   fd;
	char	   *tmpath, *userpath, *ln;
	FILE	   *in, *out;
	size_t	   i, len, namelen;
	const char template[] = "XXXXXX";

	init_var_tbl(df);
	in = out = NULL;
	
	if (df->path == NULL) {
		if (df_create(df) == NULL)
			return (-1);
		return (0);
	}
	if ((userpath = user_autostart_path(df->path)) == NULL)
		return (-1);
	free(df->path);
	df->path = userpath;
	if ((in = fopen(df->path, "r")) == NULL && errno != ENOENT)
		ERROR(-1, "fopen(%s)", df->path);
	len = strlen(df->path) + sizeof(".") + sizeof(template);
	if ((tmpath = malloc(len)) == NULL) {
		seterr("malloc()");
		goto error;
	}
	(void)snprintf(tmpath, len, "%s.%s", df->path, template);

	if ((fd = mkstemp(tmpath)) == -1) {
		seterr("mkstemp()");
		goto error;
	}
	if ((out = fdopen(fd, "w")) == NULL) {
		seterr("fdopen()");
		goto error;
	}
	while (in != NULL && (ln = readln(in)) != NULL) {
		for (i = 0; i < N_DF_VARS; i++) {
			namelen = strlen(df_vars[i].name);
			if (strncmp(ln, df_vars[i].name, namelen) == 0 &&
			    (isspace(ln[namelen]) || ln[namelen] == '=')) {
			    	if (df_vars[i].type == TYPE_STR) {
					if (*df_vars[i].val.strval == NULL)
						break;
					(void)fprintf(out, "%s=%s\n", 
					    df_vars[i].name,
					    *df_vars[i].val.strval);
				} else {
					(void)fprintf(out, "%s=%s\n", 
					    df_vars[i].name,
					    *df_vars[i].val.boolval ? \
					    "true" : "false");
				}
				df_vars[i].set = true;
				break;
			}
		}
		if (i == N_DF_VARS)
			(void)fprintf(out, "%s\n", ln);
	}
	if (in == NULL)
		(void)fprintf(out, "[Desktop Entry]\nType=Application\n");
	for (i = 0; i < N_DF_VARS; i++) {
		if (df_vars[i].set)
			continue;
		if (df_vars[i].type == TYPE_STR) {
			if (*df_vars[i].val.strval == NULL)
				continue;
			(void)fprintf(out, "%s=%s\n", 
			    df_vars[i].name, *df_vars[i].val.strval);
		} else {
			(void)fprintf(out, "%s=%s\n", df_vars[i].name,
			     *df_vars[i].val.boolval ? "true" : "false");
		}
	}
	(void)fclose(out);
	if (in != NULL)
		(void)fclose(in);
	in = out = NULL;
	if (rename(tmpath, df->path) == -1) {
		seterr("rename(%s, %s)", tmpath, df->path);
		goto error;
	}
	free(tmpath);

	return (0);
error:
	if (in != NULL)
		(void)fclose(in);
	if (out != NULL)
		(void)fclose(out);
	free(tmpath);

	return (-1);
}

static desktop_file_t **
df_readdir(const char *dir, desktop_file_t ***list)
{
	DIR	       *dirp;
	char	       *path, *suffix;
	size_t	       len;
	struct stat    sb;
	struct dirent  *dp;
	desktop_file_t *df;

	_clearerr();
	if ((dirp = opendir(dir)) == NULL) {
		if (errno != ENOENT)
			ERROR(NULL, "opendir(%s)", dir);
		return (NULL);
	}
	len = strlen(dir) + _POSIX_PATH_MAX + 2;
	if ((path = malloc(len)) == NULL) {
		seterr("malloc()");
		goto error;
	}
	while (dirp != NULL && (dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".")  == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		if ((suffix = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(++suffix, "desktop") != 0)
			continue;
		(void)snprintf(path, len, "%s/%s", dir, dp->d_name);
		if (stat(path, &sb) == -1) {
			warn("stat(%s)", path);
			continue;
		}
		if (!S_ISREG(sb.st_mode))
			continue;
		if ((df = df_read(path)) == NULL && _error)
			goto error;
		if (extend_desktop_file_list(list, df) == NULL && _error)
			goto error;
	}
	free(path);
	(void)closedir(dirp);

	return (*list);
error:
	free(path);
	(void)closedir(dirp);

	return (NULL);
}

static desktop_file_t *
extend_desktop_file_list(desktop_file_t ***list, desktop_file_t *df)
{
	size_t	       n;
	desktop_file_t **ptr;

	_clearerr();
	if (df == NULL)
		return (NULL);
	if (*list == NULL) {
		ptr = *list = malloc(2 * sizeof(desktop_file_t *));
		if (ptr == NULL)
			ERROR(NULL, "malloc()");
	} else {
		/* Replace desktop files with the same basename */
		for (ptr = *list; *ptr != NULL; ptr++) {
			if (cmp_basenames((*ptr)->path, df->path) == 0) {
				if ((*ptr)->prio < df->prio) {
					df_free(*ptr);
					*ptr = df;
				}
				return (*ptr);
			}
		}
		/* Append new desktop file */
		n = ptr - *list + 2;
		ptr = realloc(*list, n * sizeof(desktop_file_t *));
		if (ptr == NULL)
			ERROR(NULL, "realloc()");
		*list = ptr;
		ptr += n - 2;
	}
	*ptr++ = df; *ptr = NULL;

	return (df);
}

static bool
entry_changed(const dsbautostart_t *as, const entry_t *entry)
{
	entry_t *ep;
	
	for (ep = as->prev_entries; ep != NULL; ep = ep->next) {
		if (ep->id != entry->id)
			continue;
		return (!cmp_entries(ep, entry));
	}
	return (true);
}

