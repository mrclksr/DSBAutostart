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

typedef struct dsbautostart_s {
	char *cmd;
	bool active;
	struct dsbautostart_s *next;
	struct dsbautostart_s *prev;
} dsbautostart_t;

int		dsbautostart_set(dsbautostart_t *, const char *, bool);
int		dsbautostart_write(dsbautostart_t *);
void		dsbautostart_del_entry(dsbautostart_t **, dsbautostart_t *);
void		dsbautostart_item_move_up(dsbautostart_t **, dsbautostart_t *);
void		dsbautostart_item_move_down(dsbautostart_t **, dsbautostart_t *);
bool		dsbautostart_error(void);
const char	*dsbautostart_strerror(void);
dsbautostart_t *dsbautostart_read(void);
dsbautostart_t *dsbautostart_add_entry(dsbautostart_t **, const char *, bool);
#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif /* !_DSBAUTOSTART_H_ */

