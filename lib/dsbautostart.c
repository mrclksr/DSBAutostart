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

static char *get_aspath(const char *);
static char *readln(FILE *);
static FILE *open_asfile(void);

static bool error = false;
static char errbuf[1024];

#define ERROR(ret, fmt, ...) \
	do { \
		seterr(fmt, ##__VA_ARGS__); \
		return (ret); \
	} while (0)

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
	dsbautostart_t *as = NULL;

	_clearerr();

	if ((fp = open_asfile()) == NULL)
		return (NULL);
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
		if (dsbautostart_add_entry(&as, p, active) == NULL)
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

	_clearerr();
	
	if ((fp = open_tempfile(&tmpath)) == NULL)
		return (-1);
	if ((topath = get_aspath(NULL)) == NULL)
		return (-1);
	if (fputs("#!/bin/sh\n", fp) == EOF)
		ERROR(-1, "fputs()");
	for (; as != NULL; as = as->next) {
		if (as->cmd == NULL || *as->cmd == '\0')
			continue;
		if (!as->active)
			(void)fprintf(fp, "#[INACTIVE]#%s&\n", as->cmd);
		else
			(void)fprintf(fp, "%s&\n", as->cmd);
	}
	(void)fclose(fp);

	if ((ret = rename(tmpath, topath)) == -1)
		seterr("rename(%s, %s)", tmpath, topath);
	free(tmpath); free(topath);

	return (ret);
}

int
dsbautostart_set(dsbautostart_t *entry, const char *cmd, bool active)
{
	if (entry == NULL)
		return (0);
	free(entry->cmd);
	if ((entry->cmd = strdup(cmd)) == NULL)
		ERROR(-1, "strdup()");
	entry->active = active;

	return (0);
}

void
dsbautostart_del_entry(dsbautostart_t **list, dsbautostart_t *entry)
{
	if (entry == NULL || list == NULL || *list == NULL)
		return;
	if (entry == *list) {
		if (entry->next != NULL)
			entry->next->prev = NULL;
		*list = entry->next;
	} else if (entry->next == NULL) {
		entry->prev->next = NULL;
	} else {
		entry->prev->next = entry->next;
		entry->next->prev = entry->prev;
	}
	free(entry->cmd); free(entry);
}

dsbautostart_t *
dsbautostart_add_entry(dsbautostart_t **as, const char *cmd, bool active)
{
	dsbautostart_t *p, *tail;

	_clearerr();

	for (tail = *as; tail != NULL && tail->next != NULL; tail = tail->next)
		;
	if ((p = malloc(sizeof(dsbautostart_t))) == NULL)
		ERROR(NULL, "malloc()");
	if (tail == NULL) {
		*as = p;
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

void
dsbautostart_item_move_up(dsbautostart_t **head, dsbautostart_t *as)
{
	dsbautostart_t *a, *b;

	if (as == NULL || as->prev == NULL)
		return;
	a = as->prev; b = as;
	if (a->prev == NULL)
		*head = b;
	else
		a->prev->next = b;
	if (b->next != NULL)
		b->next->prev = a;
	a->next = b->next;
	b->next = a;
	b->prev = a->prev;
	a->prev = b;
}

void
dsbautostart_item_move_down(dsbautostart_t **head, dsbautostart_t *as)
{
	if (as == NULL || as->next == NULL)
		return;
	dsbautostart_item_move_up(head, as->next);
}

dsbautostart_t *
dsbautostart_copy(dsbautostart_t *list)
{
	dsbautostart_t *cp = NULL;

	for (; list != NULL; list = list->next) {
		if (dsbautostart_add_entry(&cp, list->cmd,
		    list->active) == NULL)
			return (NULL);
	}
	return (cp);
}

bool
dsbautostart_cmp(dsbautostart_t *l0, dsbautostart_t *l1)
{

	for (; l0 != NULL && l1 != NULL; l0 = l0->next, l1 = l1->next) {
		if (l0->active != l1->active)
			return (false);
		if (strcmp(l0->cmd, l1->cmd) != 0)
			return (false);
	}
	if (l0 != NULL || l1 != NULL)
		return (false);
	return (true);
}

void
dsbautostart_free(dsbautostart_t *list)
{
	dsbautostart_t *entry, *next;

	for (entry = list; entry != NULL; entry = next) {
		free(entry->cmd); next = entry->next;
		free(entry);
	}
}

