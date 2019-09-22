/*
 * mbsync - mailbox synchronizer
 * Copyright (C) 2000-2002 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2002-2006,2010-2012 Oswald Buddenhagen <ossi@users.sf.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, mbsync may be linked with the OpenSSL library,
 * despite that library's more restrictive license.
 */

#include "driver.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

driver_t *drivers[N_DRIVERS] = { &maildir_driver, &imap_driver };

int
count_generic_messages( message_t *msgs )
{
	int count = 0;
	for (; msgs; msgs = msgs->next)
		count++;
	return count;
}

void
free_generic_messages( message_t *msgs )
{
	message_t *tmsg;

	for (; msgs; msgs = tmsg) {
		tmsg = msgs->next;
		free( msgs->msgid );
		flags_wipe( &msgs->gm_labels );
		flags_wipe( &msgs->raw_flags );
		free( msgs );
	}
}

void
parse_generic_store( store_conf_t *store, conffile_t *cfg )
{
	if (!strcasecmp( "Trash", cfg->cmd )) {
		store->trash = nfstrdup( cfg->val );
	} else if (!strcasecmp( "TrashRemoteNew", cfg->cmd )) {
		store->trash_remote_new = parse_bool( cfg );
	} else if (!strcasecmp( "TrashNewOnly", cfg->cmd )) {
		store->trash_only_new = parse_bool( cfg );
	} else if (!strcasecmp( "MaxSize", cfg->cmd )) {
		store->max_size = parse_size( cfg );
	} else if (!strcasecmp( "MapInbox", cfg->cmd )) {
		store->map_inbox = nfstrdup( cfg->val );
	} else if (!strcasecmp( "Flatten", cfg->cmd )) {
		const char *p;
		for (p = cfg->val; *p; p++) {
			if (*p == '/') {
				error( "%s:%d: flattened hierarchy delimiter cannot contain the canonical delimiter '/'\n", cfg->file, cfg->line );
				cfg->err = 1;
				return;
			}
		}
		store->flat_delim = nfstrdup( cfg->val );
	} else {
		error( "%s:%d: unknown keyword '%s'\n", cfg->file, cfg->line, cfg->cmd );
		cfg->err = 1;
	}
}

typedef struct flags_data {
	size_t max_size;
	size_t size;
	char data[];
} flags_data_t;

void
add_flag(flags_t *flags, char *flag)
{
	size_t flag_size = strlen(flag) + 1;
	while (1) {
		if (flags->size <= sizeof(flags->data)) {
			size_t newsize = flags->size + flag_size;
			if (newsize <= sizeof(flags->data)) {
				memcpy(flags->data + flags->size, flag, flag_size);
				flags->size = newsize;
				return;
			} else {
				size_t alloc_size = 4 * sizeof(flags_t);
				flags_data_t *datap = nfmalloc(alloc_size);
				memcpy(datap->data, flags->data, flags->size);
				datap->max_size = alloc_size - sizeof(flags_data_t);
				datap->size = flags->size;
				flags->size = UINT8_MAX;
				flags->data_p = datap;
			}
		} else {
			size_t newsize = flags->data_p->size + flag_size;
			if (newsize <= flags->data_p->max_size) {
				memcpy(flags->data_p->data + flags->data_p->size, flag, flag_size);
				flags->data_p->size = newsize;
				return;
			} else {
				size_t alloc_size = 4 * (flags->data_p->max_size + sizeof(flags_data_t));
				flags->data_p = nfrealloc(flags->data_p, alloc_size);
				flags->data_p->max_size = alloc_size - sizeof(flags_data_t);
			}
		}
	}
}

size_t flags_size(flags_t *flags)
{
	if (flags->size <= sizeof(flags->data)) {
		return flags->size;
	} else {
		return flags->data_p->size;
	}
}

char *flags_data(flags_t *flags)
{
	if (flags->size <= sizeof(flags->data)) {
		return flags->data;
	} else {
		return flags->data_p->data;
	}
}

void flags_wipe(flags_t *flags)
{
	if (flags->size > sizeof(flags->data)) {
		free(flags->data_p);
	}
	flags->data_p = NULL;
	flags->size = 0;
}
