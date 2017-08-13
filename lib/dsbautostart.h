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
#include "dsbcfg/dsbcfg.h"

#define PATH_ASFILE "autostart.sh"

typedef struct entry_s {
	char   *cmd;
	bool   active;
	struct entry_s *next;
	struct entry_s *prev;
} entry_t;

/*
 * Struct to track changes for a undo/redo history queue.
 */
typedef struct change_history_s {
	char	type;		/* Type of change the entry was modified */
#define CHANGE_TYPE_ADD	      1
#define CHANGE_TYPE_DELETE    2
#define CHANGE_TYPE_CONTENT   3
#define CHANGE_TYPE_MOVE_UP   4
#define CHANGE_TYPE_MOVE_DOWN 5
	bool	active;		/* Previous/current activation status */
	char	*cmd;		/* Previous/current current command string */
	entry_t	*entry;		/* Pointer to entry from dsbautostart_t */
	entry_t *eprev, *enext;	/* Previous and next entry of this entry */
	struct change_history_s *prev;
	struct change_history_s *next;
} change_history_t;

typedef struct dsbautostart_s {
	entry_t		 *entry;
	change_history_t *undo_head;
	change_history_t *redo_head;
	change_history_t *undo_index; /* Current index of undo queue */
	change_history_t *redo_index; /* Current index of redo queue */
} dsbautostart_t;

int		dsbautostart_set(dsbautostart_t *, entry_t *, const char *,
		    bool);
int		dsbautostart_write(dsbautostart_t *);
void		dsbautostart_undo(dsbautostart_t *);
void		dsbautostart_redo(dsbautostart_t *);
void		dsbautostart_free(dsbautostart_t *);
bool		dsbautostart_error(void);
bool		dsbautostart_cmp(dsbautostart_t *, dsbautostart_t *);
bool		dsbautostart_can_undo(dsbautostart_t *);
bool		dsbautostart_can_redo(dsbautostart_t *);
entry_t		*dsbautostart_entry_move_up(dsbautostart_t *, entry_t *);
entry_t		*dsbautostart_entry_move_down(dsbautostart_t *, entry_t *);
entry_t		*dsbautostart_del_entry(dsbautostart_t *, entry_t *);
entry_t		*dsbautostart_add_entry(dsbautostart_t *, const char *, bool);
const char	*dsbautostart_strerror(void);
dsbautostart_t *dsbautostart_read(void);
dsbautostart_t *dsbautostart_copy(dsbautostart_t *);
#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif /* !_DSBAUTOSTART_H_ */

