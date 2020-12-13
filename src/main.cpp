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

#include <QLocale>
#include <QTranslator>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <sys/syslimits.h>

#include "mainwin.h"

void
autostart()
{
	char	       cmd[PATH_MAX];
	entry_t	       *ep;
	dsbautostart_t *as;

	if ((as = dsbautostart_init()) == NULL)
		errx(EXIT_FAILURE, "%s", dsbautostart_strerror());
	for (ep = as->cur_entries; ep != NULL; ep = ep->next) {
		if (ep->exclude || ep->deleted)
			continue;
		(void)snprintf(cmd, sizeof(cmd), "%s&", ep->df->exec);
		switch (system(cmd)) {
		case  -1:
		case 127:
			err(EXIT_FAILURE, "system(%s)", cmd);
			break;
		}
	}
	exit(EXIT_SUCCESS);
}

void
create_from_list()
{
	char	       *p, *q, *line = NULL;
	bool	       is_duplicate;
	entry_t	       *ep;
        size_t	       n, linecap = 0;
	dsbautostart_t *as;

	as = dsbautostart_init();
	if (as == NULL)
		errx(EXIT_FAILURE, "%s", dsbautostart_strerror());
	while (getline(&line, &linecap, stdin) > 0) {
		n = strspn(line, "\n\t ");
		p = &line[n];
		(void)strtok(p, "#\n\r");
		if (*p == '#' || *p == '\0')
			continue;
		for (q = strchr(p, '\0'); q != p; q--) {
			if (*q == '&') {
				*q = '\0';
				break;
			}
		}
		is_duplicate = false;
		for (ep = as->cur_entries; ep != NULL; ep = ep->next) {
			if (ep->df == NULL)
				continue;
			if (strcmp(ep->df->exec, p) == 0) {
				is_duplicate = true;
				break;
			}
		}
		if (is_duplicate)
			continue;
		if (dsbautostart_entry_add(as, p, NULL, NULL, NULL,
		    NULL, false) == NULL)
			err(EXIT_FAILURE, "%s", dsbautostart_strerror());
	}
	if (dsbautostart_save(as) == -1)
		errx(EXIT_FAILURE, "%s", dsbautostart_strerror());
	exit(EXIT_SUCCESS);
}

void
usage()
{
	(void)printf("Usage: %s [-h]\n"					    \
		     "       %s <-a|-c>\n"				    \
		     "Options\n"					    \
		     "-a     Autostart commands, and exit\n"		    \
		     "-c     Create desktop files in the user's autostart " \
		     "directory from the\n"				    \
		     "       command list read from stdin.\n"		    \
		     "-h     Show this help text.\n", PROGRAM, PROGRAM);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "ach")) != -1) {
		switch (ch) {
		case 'a':
			autostart();
			break;
		case 'c':
			create_from_list();
			break;
		case '?':
		case 'h':
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	QApplication app(argc, argv);
	QTranslator translator;

	if (translator.load(QLocale(), QLatin1String(PROGRAM),
	    QLatin1String("_"), QLatin1String(LOCALE_PATH)))
		app.installTranslator(&translator);
	Mainwin w;
	w.show();
	return (app.exec());
}
