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

#ifndef _DSBAUTOSTART_H_
#define _DSBAUTOSTART_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>

#define PATH_ASFILE "autostart.sh"

typedef enum {
	DF_KEY_NAME, DF_KEY_COMMENT, DF_KEY_EXEC, DF_KEY_HIDDEN,
	DF_KEY_TERMINAL, DF_KEY_NOT_SHOW_IN, DF_KEY_ONLY_SHOW_IN
} df_key_t;

typedef struct desktop_file_s {
	int  prio;
	char *type;
	char *name;
	char *comment;
	char *exec;
	char *path;
	char *only_show_in;
	char *not_show_in;
	bool hidden;
	bool terminal;
} desktop_file_t;

typedef struct entry_s {
	int	       id;
	bool	       deleted;
	bool	       exclude;
	struct entry_s *next;
	struct entry_s *prev;
	desktop_file_t *df;
} entry_t;

typedef enum {
	ADD = 1, DELETE, CHANGE
} action_t;

typedef struct hist_entry_s {
	entry_t	       *entry;
	action_t       action;
	desktop_file_t *df0;
	desktop_file_t *df1;
	struct hist_entry_s *next;
	struct hist_entry_s *prev;
} hist_entry_t;

/*
 * Struct to track changes for a undo/redo history queue.
 */
typedef struct change_history_s {
	hist_entry_t *head;
	hist_entry_t *tail;
	hist_entry_t *idx;
} change_history_t;

typedef struct dsbautostart_s {
	entry_t		 *prev_entries;
	entry_t		 *cur_entries;
	change_history_t *hist;
} dsbautostart_t;

int		dsbautostart_read_desktop_files(dsbautostart_t *);
int		dsbautostart_df_del(const char *);
int		dsbautostart_df_set_key(desktop_file_t *, df_key_t,
			const void *);
int		dsbautostart_entry_set(dsbautostart_t *, entry_t *,
			const char *, const char *, const char *,
			const char *, const char *, bool);
int		dsbautostart_save(dsbautostart_t *);
void		dsbautostart_undo(dsbautostart_t *);
void		dsbautostart_redo(dsbautostart_t *);
bool		dsbautostart_error(void);
bool		dsbautostart_can_undo(const dsbautostart_t *);
bool		dsbautostart_can_redo(const dsbautostart_t *);
bool		dsbautostart_changed(const dsbautostart_t *);
entry_t		*dsbautostart_entry_del(dsbautostart_t *, entry_t *);
entry_t		*dsbautostart_df_add(dsbautostart_t *, const char *);
entry_t		*dsbautostart_entry_add(dsbautostart_t *, const char *cmd,
			const char *name, const char *comment, const char *,
			const char *, bool terminal);
const char	*dsbautostart_strerror(void);
dsbautostart_t	*dsbautostart_init(void);
#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif /* !_DSBAUTOSTART_H_ */

