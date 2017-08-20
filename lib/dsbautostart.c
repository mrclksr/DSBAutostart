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

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include "dsbautostart.h"

#define ERROR(ret, fmt, ...) do { \
	seterr(fmt, ##__VA_ARGS__); \
	return (ret); \
} while (0)

static int	entry_set(entry_t *, const char *, bool);
static char	*readln(FILE *fp);
static char	*get_aspath(const char *);
static FILE	*open_asfile(void);
static FILE	*open_tempfile(char **);
static void	_clearerr(void);
static void	seterr(const char *msg, ...);
static void	entry_del(dsbautostart_t *as, entry_t *entry);
static void	entry_move_up(dsbautostart_t *as, entry_t *entry);
static void	entry_move_down(dsbautostart_t *as, entry_t *entry);
static void 	insert_entry(dsbautostart_t *as, entry_t *entry,
		entry_t *prev, entry_t *next);
static entry_t *entry_add(dsbautostart_t *, const char *, bool);
static change_history_t *hist_add(change_history_t **, change_history_t **);

static bool error = false;
static char errbuf[1024];

static void
_clearerr()
{
	error = false;
}

static void
seterr(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);

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

static char *
get_aspath(const char *suffix)
{
	char   *path, *dir;
	size_t len;

	if ((dir = dsbcfg_mkdir(NULL)) == NULL) {
		errno = 0;
		ERROR(NULL, dsbcfg_strerror());
	}
	len = strlen(dir) + sizeof(PATH_ASFILE) + 1;
	if (suffix != NULL)
		len += strlen(suffix) + 1;
	if ((path = malloc(len)) == NULL)
		ERROR(NULL, "malloc()");
	(void)snprintf(path, len, "%s/%s%s", dir, PATH_ASFILE,
	    suffix != NULL ? suffix : "");
	return (path);
}

static FILE *
open_asfile()
{
	char *path;
	FILE *fp;

	if ((path = get_aspath(NULL)) == NULL)
		return (NULL);
	if ((fp = fopen(path, "r")) == NULL && errno != ENOENT)
		ERROR(NULL, "fopen(%s)", path);
	free(path);
	return (fp);
}

static FILE *
open_tempfile(char **path)
{
	int  fd;
	char *p;
	FILE *fp;

	if ((p = get_aspath(".XXXX")) == NULL)
		return (NULL);
	if ((fd = mkstemp(p)) == -1) {
		free(p);
		ERROR(NULL, "mkstemp()");
	}
	if ((fp = fdopen(fd, "w")) == NULL) {
		seterr("fdopen()"); free(p); (void)close(fd);
		return (NULL);
	}
	*path = p;

	return (fp);
}

bool
dsbautostart_error()
{
	return (error);
}

const char *
dsbautostart_strerror()
{
	return (errbuf);
}

dsbautostart_t *
dsbautostart_read()
{
	char *p;
	bool active;
	FILE *fp;
	dsbautostart_t *as;

	_clearerr();
	if ((as = malloc(sizeof(dsbautostart_t))) == NULL)
		return (NULL);
	as->redo_head = as->undo_head = as->redo_index = as->undo_index = NULL;
	as->entry = NULL;
	if ((fp = open_asfile()) == NULL)
		return (error ? NULL : as);
	while ((p = readln(fp)) != NULL) {
		if (strncmp(p, "#[INACTIVE]#", 12) == 0) {
			active = false;
			(void)memmove(p, p + 12, strlen(p) - 12 + 1);
		} else if (p[0] == '#')
			continue;
		else
			active = true;
		if (p[strlen(p) - 1] == '&')
			p[strlen(p) - 1] = '\0';
		if (entry_add(as, p, active) == NULL)
			return (NULL);
	}
	(void)fclose(fp);

	return (as);
}

int
dsbautostart_write(dsbautostart_t *as)
{
	int  ret;
	FILE *fp;
	char *tmpath, *topath;
	entry_t *entry;

	_clearerr();
	
	if ((fp = open_tempfile(&tmpath)) == NULL)
		return (-1);
	if ((topath = get_aspath(NULL)) == NULL)
		return (-1);
	if (fputs("#!/bin/sh\n", fp) == EOF)
		ERROR(-1, "fputs()");
	for (entry = as->entry; entry != NULL; entry = entry->next) {
		if (entry->cmd == NULL || *entry->cmd == '\0')
			continue;
		if (!entry->active)
			(void)fprintf(fp, "#[INACTIVE]#%s&\n", entry->cmd);
		else
			(void)fprintf(fp, "%s&\n", entry->cmd);
	}
	(void)fclose(fp);

	if ((ret = rename(tmpath, topath)) == -1)
		seterr("rename(%s, %s)", tmpath, topath);
	free(tmpath); free(topath);

	return (ret);
}

int
dsbautostart_set(dsbautostart_t *as, entry_t *entry, const char *cmd,
    bool active)
{
	change_history_t *re, *un;

	if ((re = hist_add(&as->redo_head, &as->redo_index)) == NULL ||
	    (un = hist_add(&as->undo_head, &as->undo_index)) == NULL)
		return (-1);
	if ((un->cmd = strdup(entry->cmd)) == NULL)
		ERROR(-1, "strdup()");
	un->type   = CHANGE_TYPE_CONTENT;
	un->entry  = entry;
	un->active = entry->active;

	if ((re->cmd = strdup(cmd)) == NULL)
		ERROR(-1, "strdup()");
	re->type   = CHANGE_TYPE_CONTENT;
	re->entry  = entry;
	re->active = active;
	
	as->undo_index = un;
	as->redo_index = re;
	
	entry_set(entry, cmd, active);

	return (0);
}

entry_t *
dsbautostart_del_entry(dsbautostart_t *as, entry_t *entry)
{
	change_history_t *re, *un;

	if ((re = hist_add(&as->redo_head, &as->redo_index)) == NULL ||
	    (un = hist_add(&as->undo_head, &as->undo_index)) == NULL)
		return (NULL);
	un->type   = CHANGE_TYPE_DELETE;
	un->entry  = entry;
	un->eprev  = entry->prev;
	un->enext  = entry->next;

	re->type   = CHANGE_TYPE_DELETE;
	re->entry  = entry;

	as->redo_index = re;
	as->undo_index = un;

	entry_del(as, entry);

	return (entry);
}

entry_t *
dsbautostart_add_entry(dsbautostart_t *as, const char *cmd, bool active)
{
	entry_t *entry;
	change_history_t *re, *un;

	if ((entry = entry_add(as, cmd, active)) == NULL)
		return (NULL);
	if ((re = hist_add(&as->redo_head, &as->redo_index)) == NULL ||
	    (un = hist_add(&as->undo_head, &as->undo_index)) == NULL)
		return (NULL);
	un->type   = CHANGE_TYPE_ADD;
	un->entry  = entry;

	re->type   = CHANGE_TYPE_ADD;
	re->entry  = entry;
	re->eprev  = entry->prev;
	re->enext  = entry->next;

	as->redo_index = re;
	as->undo_index = un;

	return (entry);
}

entry_t *
dsbautostart_entry_move_up(dsbautostart_t *as, entry_t *entry)
{
	change_history_t *re, *un;

	if (entry == NULL || entry->prev == NULL)
		return (entry);
	if ((re = hist_add(&as->redo_head, &as->redo_index)) == NULL ||
	    (un = hist_add(&as->undo_head, &as->undo_index)) == NULL)
		return (NULL);
	un->type  = CHANGE_TYPE_MOVE_UP;
	un->entry = entry;

	re->type  = CHANGE_TYPE_MOVE_UP;
	re->entry = entry;

	as->redo_index = re;
	as->undo_index = un;

	entry_move_up(as, entry);

	return (entry);
}

entry_t *
dsbautostart_entry_move_down(dsbautostart_t *as, entry_t *entry)
{
	change_history_t *re, *un;

	if (entry == NULL || entry->next == NULL)
		return (entry);
	if ((re = hist_add(&as->redo_head, &as->redo_index)) == NULL ||
	    (un = hist_add(&as->undo_head, &as->undo_index)) == NULL)
		return (NULL);
	un->type  = CHANGE_TYPE_MOVE_DOWN;
	un->entry = entry;

	re->type  = CHANGE_TYPE_MOVE_DOWN;
	re->entry = entry;

	as->redo_index = re;
	as->undo_index = un;

	entry_move_down(as, entry);

	return (entry);
}

dsbautostart_t *
dsbautostart_copy(dsbautostart_t *as)
{
	dsbautostart_t *cp;
	entry_t *entry;

	if ((cp = malloc(sizeof(dsbautostart_t))) == NULL)
		ERROR(NULL, "malloc()");
	cp->undo_head = cp->redo_head = NULL;
	cp->undo_index = cp->redo_index = NULL;
	cp->entry = NULL;
	for (entry = as->entry; entry != NULL; entry = entry->next) {
		if (entry_add(cp, entry->cmd, entry->active) == NULL)
			return (NULL);
	}
	return (cp);
}

bool
dsbautostart_cmp(dsbautostart_t *l0, dsbautostart_t *l1)
{
	entry_t *e0, *e1;

	e0 = l0->entry; e1 = l1->entry;
	for (; e0 != NULL && e1 != NULL; e0 = e0->next, e1 = e1->next) {
		if (e0->active != e1->active)
			return (false);
		if (strcmp(e0->cmd, e1->cmd) != 0)
			return (false);
	}
	if (e0 != NULL || e1 != NULL)
		return (false);
	return (true);
}

bool
dsbautostart_can_undo(dsbautostart_t *as)
{
	return (as->undo_index == NULL ? false : true);
}

bool
dsbautostart_can_redo(dsbautostart_t *as)
{
	if (as->redo_head == NULL)
		return (false);
	if (as->redo_index != NULL && as->redo_index->next == NULL) {
		return (false);
	}
	return (true);
}

void
dsbautostart_free(dsbautostart_t *as)
{
	entry_t *entry, *next;

	for (entry = as->entry; entry != NULL; entry = next) {
		free(entry->cmd); next = entry->next;
		free(entry);
	}
	free(as);
}

void
dsbautostart_undo(dsbautostart_t *as)
{
	change_history_t *h;

	if (as->undo_index == NULL)
		return;
	h = as->undo_index;
	switch (h->type) {
	case CHANGE_TYPE_CONTENT:
		entry_set(h->entry, h->cmd, h->active);
		break;
	case CHANGE_TYPE_DELETE:
		insert_entry(as, h->entry, h->eprev, h->enext);
		break;
	case CHANGE_TYPE_ADD:
		entry_del(as, h->entry);
		break;
	case CHANGE_TYPE_MOVE_UP:
		entry_move_down(as, h->entry);
		break;
	case CHANGE_TYPE_MOVE_DOWN:
		entry_move_up(as, h->entry);
		break;
	}
	/* Go up one entry in history. */
	as->undo_index = as->undo_index->prev;
	as->redo_index = as->redo_index->prev;
}

void
dsbautostart_redo(dsbautostart_t *as)
{
	change_history_t *h;

	/* hist is the pointer to the current change */
	if (as->redo_head == NULL)
		/* Nothing to redo */
		return;
	if (as->redo_index == NULL)
		as->redo_index = h = as->redo_head;
	else if (as->redo_index->next == NULL)
		return;
	else
		h = as->redo_index = as->redo_index->next;
	switch (h->type) {
	case CHANGE_TYPE_CONTENT:
		entry_set(h->entry, h->cmd, h->active);
		break;
	case CHANGE_TYPE_DELETE:
		entry_del(as, h->entry);
		break;
	case CHANGE_TYPE_ADD:
		insert_entry(as, h->entry, h->eprev, h->enext);
		break;
	case CHANGE_TYPE_MOVE_UP:
		entry_move_up(as, h->entry);
		break;
	case CHANGE_TYPE_MOVE_DOWN:
		entry_move_down(as, h->entry);
		break;
	}
	if (as->undo_index == NULL)
		as->undo_index = as->undo_head;
	else
		as->undo_index = as->undo_index->next;
}

/*
 * Add a new change_history_t object to the given history, and
 * return a pointer to it. 
 */
static change_history_t *
hist_add(change_history_t **head, change_history_t **idx)
{
	change_history_t *hist;

	if ((hist = malloc(sizeof(change_history_t))) == NULL)
		ERROR(NULL, "malloc()");
	hist->next = hist->prev = NULL;
	if (*idx == NULL) {
		if (*head != NULL) {
			(*head)->prev = hist;
			(*head)->next = *head;
			*head = hist;
		} else
			*head = hist;
	} else {
		if ((*idx)->next != NULL)
			(*idx)->next->prev = hist;
		hist->prev = *idx;
		hist->next = (*idx)->next;
		(*idx)->next = hist;
	}
	*idx = hist;

	return (hist);
}

/*
 * Re-insert deleted entry
 */
static void
insert_entry(dsbautostart_t *as, entry_t *entry, entry_t *prev, entry_t *next)
{
	if (as == NULL || entry == NULL)
		return;
	if (prev == NULL)
		as->entry = entry;
	else
		prev->next = entry;
	if (next != NULL)
		next->prev = entry;
	entry->prev = prev;
	entry->next = next;
}

static void
entry_del(dsbautostart_t *as, entry_t *entry)
{
	if (entry == NULL || as == NULL)
		return;
	if (entry == as->entry) {
		if (entry->next != NULL)
			entry->next->prev = NULL;
		as->entry = entry->next;
		entry->prev = NULL;
	} else if (entry->next == NULL) {
		entry->prev->next = NULL;
	} else {
		entry->prev->next = entry->next;
		entry->next->prev = entry->prev;
	}
}

static int
entry_set(entry_t *entry, const char *cmd, bool active)
{
	if (entry == NULL)
		return (0);
	free(entry->cmd);
	if ((entry->cmd = strdup(cmd)) == NULL)
		ERROR(-1, "strdup()");
	entry->active = active;

	return (0);
}

static entry_t *
entry_add(dsbautostart_t *as, const char *cmd, bool active)
{
	entry_t *p, *tail;

	_clearerr();

	for (tail = as->entry; tail != NULL && tail->next != NULL;
	    tail = tail->next)
		;
	if ((p = malloc(sizeof(entry_t))) == NULL)
		ERROR(NULL, "malloc()");
	if (tail == NULL) {
		as->entry = p;
		p->next = p->prev = NULL;
	} else {
		p->next = NULL;
		p->prev = tail;
		tail->next = p;
	}
	p->cmd = strdup(cmd);
	if (p->cmd == NULL)
		ERROR(NULL, "strdup()");
	p->active = active;

	return (p);
}

static void
entry_move_up(dsbautostart_t *as, entry_t *entry)
{
	entry_t *a, *b;

	if (entry == NULL || entry->prev == NULL)
		return;
	a = entry->prev; b = entry;
	if (a->prev == NULL)
		as->entry = b;
	else
		a->prev->next = b;
	if (b->next != NULL)
		b->next->prev = a;
	a->next = b->next;
	b->next = a;
	b->prev = a->prev;
	a->prev = b;
}

static void
entry_move_down(dsbautostart_t *as, entry_t *entry)
{
	if (entry == NULL || entry->next == NULL)
		return;
	entry_move_up(as, entry->next);
}

