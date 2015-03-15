/*-
 * Copyright (c) 2015 Marcel Kaiser. All rights reserved.
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libintl.h>
#include "dsbcfg/dsbcfg.h"
#include "gtk-helper/gtk-helper.h"

#define TITLE	     "DSB Autostart"
#define PATH_ASFILE  "autostart.sh"
#define YES_STR	     _("_Yes")	/* For yesnobox() in gtk-helper.c */
#define NO_STR	     _("_No")	/* Dito */

typedef enum { false, true } bool;

typedef struct cmdentry_s {
	gboolean  active;	/* Check button/entry state. */
	GtkWidget *cb;		/* Check button. */
	GtkWidget *entry;	/* Input field. */
	GtkWidget *bt;		/* Delete button. */
	GtkWidget *hbox;	/* Holds 'entry' and 'bt'. */
} cmdentry_t;

static FILE	*open_asfile(const char *);
static void	set_changed(void);
static void	add_entry(const char *, gpointer);
static void	delete_entry(GtkWidget *, gpointer);
static void	toggled(GtkToggleButton *, gpointer);
static void	save_cb(void);
static gboolean quit(void);

static int	  nentries = 0, maxentries = 0;
static bool	  changed  = false;
static GtkWidget  *win, *vbox;
static cmdentry_t **entry = NULL;

int
main(int argc, char *argv[])
{
	char	     ln[_POSIX2_LINE_MAX];
	FILE	     *fp;
	gboolean     active;
	GdkPixbuf    *icon;
	GtkWidget    *save_bt, *quit_bt, *add_bt, *sw, *label;
	GtkIconTheme *icon_theme;

#ifdef WITH_GETTEXT
	(void)setlocale(LC_ALL, "");
	(void)bindtextdomain(PROGRAM, PATH_LOCALE);
	(void)textdomain(PROGRAM);
#endif
	gtk_init(&argc, &argv);
	win = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(win), _(TITLE));
	gtk_window_set_resizable(GTK_WINDOW(win), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(win), 300, 300);
	gtk_container_set_border_width(GTK_CONTAINER(win), 10);

	icon_theme = gtk_icon_theme_get_default();
	icon = gtk_icon_theme_load_icon(icon_theme,
	    GTK_STOCK_EXECUTE, 32, 0, NULL);
	if (icon != NULL)
                gtk_window_set_icon(GTK_WINDOW(win), icon);

	label = new_label(ALIGN_LEFT, ALIGN_CENTER,
	    _("Add commands to be executed at session start\n"));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(win)->vbox),
	    label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(sw), 1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
	    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), vbox);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(win)->vbox), sw, TRUE, TRUE, 0);

	/* Read the autostart file. */
	if ((fp = open_asfile("r")) != NULL) {
		while (fgets(ln, sizeof(ln), fp) != NULL) {
			if (strncmp(ln, "#[INACTIVE]#", 12) == 0) {
				active = FALSE;
				(void)memmove(ln, ln + 12, strlen(ln) - 12 + 1);
			} else if (ln[0] == '#')
				continue;
			else
				active = TRUE;
			/* Remove '\r' and '\n' */
			(void)strtok(ln, "\n\r");
			if (ln[strlen(ln) - 1] == '&')
				ln[strlen(ln) - 1] = '\0';
			add_entry(ln, &active);
		}
		(void)fclose(fp);
	}
	/* Set new entries active. */
	active  = TRUE;
	add_bt  = new_button(_("_Add command"),  GTK_STOCK_ADD);
	save_bt = new_button(_("_Save"), GTK_STOCK_SAVE);
	quit_bt = new_button(_("_Quit"), GTK_STOCK_QUIT);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(win)->action_area), add_bt,
	    FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(win)->action_area),
	    save_bt, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(win)->action_area),
	    quit_bt, TRUE, TRUE, 0);
	g_signal_connect(add_bt, "clicked", G_CALLBACK(add_entry), &active);
	g_signal_connect(save_bt, "clicked", G_CALLBACK(save_cb), NULL);
	g_signal_connect(quit_bt, "clicked", G_CALLBACK(quit), NULL);
	g_signal_connect (win, "delete-event", G_CALLBACK(quit), NULL);
	gtk_widget_show_all(GTK_WIDGET(win));
	gtk_main();
	
	return (0);
}

static void
add_entry(const char *str, gpointer active)
{
	int n;

	if (maxentries == nentries) {
		maxentries += 10;
		entry = realloc(entry, maxentries * sizeof(cmdentry_t *));
		if (entry == NULL)
			xerr(GTK_WINDOW(win), EXIT_FAILURE, "realloc()");
	}
	n = nentries;
	entry[n] = malloc(sizeof(cmdentry_t));
	if (entry[n] == NULL)
		xerr(GTK_WINDOW(win), EXIT_FAILURE, "malloc()");
	entry[n]->active = *(gboolean *)active;
	entry[n]->hbox   = gtk_hbox_new(FALSE, 0);
	entry[n]->cb	 = gtk_check_button_new();
	entry[n]->bt     = new_button(_("Delete"), GTK_STOCK_REMOVE);
	entry[n]->entry  = gtk_entry_new();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entry[n]->cb),
	    *(gboolean *)active);
	gtk_entry_set_max_length(GTK_ENTRY(entry[n]->entry), 0);
	gtk_entry_set_visibility(GTK_ENTRY(entry[n]->entry), TRUE);
	gtk_box_pack_start(GTK_BOX(entry[n]->hbox), entry[n]->cb,
	    FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(entry[n]->hbox), entry[n]->entry,
	    TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(entry[n]->hbox), entry[n]->bt,
	    FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), entry[n]->hbox,
	    FALSE, FALSE, 0);
	if (str != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry[n]->entry), str);
	gtk_widget_set_sensitive(entry[n]->entry, *(gboolean *)active);
	gtk_widget_set_sensitive(entry[n]->bt, *(gboolean *)active);

	g_signal_connect(entry[n]->bt, "clicked", G_CALLBACK(delete_entry),
	    entry[n]);
	g_signal_connect(entry[n]->entry, "changed", G_CALLBACK(set_changed),
	    NULL);
	g_signal_connect(GTK_TOGGLE_BUTTON(entry[n]->cb), "toggled",
	    G_CALLBACK(toggled), entry[n]);
	nentries++;
	gtk_widget_show_all(vbox);
}

static void
delete_entry(GtkWidget *bt, gpointer ent)
{
	int	   i;
	cmdentry_t *ce;
	const char *cmdstr;

	ce = (cmdentry_t *)ent;
	if ((cmdstr = gtk_entry_get_text(GTK_ENTRY(ce->entry))) != NULL) {
		if (*cmdstr != '\0')
			changed = true;
	}
	for (i = 0; i < nentries && entry[i] != ce; i++)
		;
	gtk_widget_destroy(entry[i]->hbox);
	free(entry[i]);

	for (; i < nentries - 1; i++)
		entry[i] = entry[i + 1];
	if (maxentries - nentries >= 20) {
		maxentries = nentries + 10;
		entry = realloc(entry, maxentries * sizeof(cmdentry_t *));
		if (entry == NULL)
			xerr(GTK_WINDOW(win), EXIT_FAILURE, "realloc()");
	}
	nentries--;
}

static FILE *
open_asfile(const char *mode)
{
	char   *path, *dir;
	FILE   *fp;
	size_t len;
	
	if ((dir = dsbcfg_mkdir(PROGRAM)) == NULL)
		xerrx(GTK_WINDOW(win), EXIT_FAILURE, "%s", dsbcfg_strerror());
	len = strlen(dir) + sizeof(PATH_ASFILE) + 1;

	if ((path = malloc(len)) == NULL)
		xerr(GTK_WINDOW(win), EXIT_FAILURE, "malloc()");
	(void)snprintf(path, len, "%s/%s", dir, PATH_ASFILE);

	fp = fopen(path, mode);
	if (fp == NULL && errno != ENOENT)
		xerr(GTK_WINDOW(win), EXIT_FAILURE, "fopen(%s)", path);
	free(path);
	return (fp);
}

static void
save_cb()
{
	int	   i, len;
	FILE	   *fp;
	const char *cmdstr;

	for (i = 0; i < nentries; i++) {
		cmdstr = gtk_entry_get_text(GTK_ENTRY(entry[i]->entry));
		if (cmdstr == NULL || *cmdstr == '\0')
			continue;
		len = gtk_entry_get_text_length(GTK_ENTRY(entry[i]->entry));
		if (len > _POSIX2_LINE_MAX) {
			xwarnx(GTK_WINDOW(win),
			    _("Command lenght must not exceed %d characters"),
			    _POSIX2_LINE_MAX);
			return;
		}
		while (--len >= 0) {
			if (cmdstr[len] == '&') {
				xwarnx(GTK_WINDOW(win),
				    _("Commands must not end with a '&'"));
				return;
			}
		}
	}
	fp = open_asfile("w");
	if (fputs("#!/bin/sh\n", fp) == EOF)
		xerr(GTK_WINDOW(win), EXIT_FAILURE, "fputs()");
	for (i = 0; i < nentries; i++) {
		cmdstr = gtk_entry_get_text(GTK_ENTRY(entry[i]->entry));
		if (cmdstr == NULL || *cmdstr == '\0')
			continue;
		entry[i]->active = gtk_toggle_button_get_active(
		    GTK_TOGGLE_BUTTON(entry[i]->cb));
		if (!entry[i]->active)
			(void)fprintf(fp, "#[INACTIVE]#%s&\n", cmdstr);
		else
			(void)fprintf(fp, "%s&\n", cmdstr);
	}
	(void)fflush(fp);
	(void)ftruncate(fileno(fp), ftell(fp));
	(void)fclose(fp);

	changed = false;
}

static void
toggled(GtkToggleButton *tb, gpointer ent)
{
	gboolean   active;
	cmdentry_t *ce;

	ce = (cmdentry_t *)ent;

	active = gtk_toggle_button_get_active(tb);
	gtk_widget_set_sensitive(ce->entry, active);
	gtk_widget_set_sensitive(ce->bt, active);
}

static void 
set_changed()
{
	changed = true;
}

static gboolean
quit()
{
	int i;

	for (i = 0; i < nentries; i++) {
		if (gtk_toggle_button_get_active(
		    GTK_TOGGLE_BUTTON(entry[i]->cb)) != entry[i]->active)
			changed = true;
	}
	if (changed) {
		if (yesnobox(GTK_WINDOW(win),
		    _("Do really want to quit without saving?")))
			exit(EXIT_SUCCESS);
		return (FALSE);
	}
	exit(EXIT_SUCCESS);
}

