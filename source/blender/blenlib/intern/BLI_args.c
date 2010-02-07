/**
 * A general argument parsing module
 *
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <ctype.h> /* for tolower */

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_args.h"
#include "BLI_ghash.h"

typedef struct bAKey {
	char *arg;
	uintptr_t pass; /* cast easier */
	int case_str; /* case specific or not */
} bAKey;

typedef struct bArgument {
	BA_ArgCallback func;
	void *data;
} bArgument;

struct bArgs {
	GHash  *items;
	int 	argc;
	char  **argv;
	int	  *passes;
};

unsigned int case_strhash(void *ptr) {
	char *s= ptr;
	unsigned int i= 0;
	unsigned char c;

	while ( (c= tolower(*s++)) )
		i= i*37 + c;

	return i;
}

static unsigned int	keyhash(void *ptr)
{
	bAKey *k = ptr;
	return case_strhash(k->arg) ^ BLI_ghashutil_inthash((void*)k->pass);
}

static int keycmp(void *a, void *b)
{
	bAKey *ka = a;
	bAKey *kb = b;
	if (ka->pass == kb->pass || ka->pass == -1 || kb->pass == -1) { /* -1 is wildcard for pass */
		if (ka->case_str == 1 || kb->case_str == 1)
			return BLI_strcasecmp(ka->arg, kb->arg);
		else
			return strcmp(ka->arg, kb->arg);
	} else {
		return BLI_ghashutil_intcmp((void*)ka->pass, (void*)kb->pass);
	}
}

bArgs *BLI_argsInit(int argc, char **argv)
{
	bArgs *ba = MEM_callocN(sizeof(bArgs), "bArgs");
	ba->passes = MEM_callocN(sizeof(int) * argc, "bArgs passes");
	ba->items = BLI_ghash_new(keyhash, keycmp);
	ba->argc = argc;
	ba->argv = argv;

	return ba;
}

static void freeItem(void *val)
{
	MEM_freeN(val);
}

void BLI_argsFree(struct bArgs *ba)
{
	BLI_ghash_free(ba->items, freeItem, freeItem);
	MEM_freeN(ba->passes);
	MEM_freeN(ba);
}

void BLI_argsPrint(struct bArgs *ba)
{
	int i;
	for (i = 0; i < ba->argc; i++) {
		printf("argv[%d] = %s\n", i, ba->argv[i]);
	}
}

char **BLI_argsArgv(struct bArgs *ba)
{
	return ba->argv;
}

void BLI_argsAdd(struct bArgs *ba, char *arg, int pass, BA_ArgCallback cb, void *data)
{
	bArgument *a = MEM_callocN(sizeof(bArgument), "bArgument");
	bAKey *key = MEM_callocN(sizeof(bAKey), "bAKey");

	key->arg = arg;
	key->pass = pass;
	key->case_str = 0;

	a->func = cb;
	a->data = data;

	BLI_ghash_insert(ba->items, key, a);
}

void BLI_argsAddCase(struct bArgs *ba, char *arg, int pass, BA_ArgCallback cb, void *data)
{
	bArgument *a = MEM_callocN(sizeof(bArgument), "bArgument");
	bAKey *key = MEM_callocN(sizeof(bAKey), "bAKey");

	key->arg = arg;
	key->pass = pass;
	key->case_str = 1;

	a->func = cb;
	a->data = data;

	BLI_ghash_insert(ba->items, key, a);
}


void BLI_argsParse(struct bArgs *ba, int pass, BA_ArgCallback default_cb, void *default_data)
{
	bAKey key;
	int i = 0;

	key.case_str = -1; /* signal what side of the comparison it is */
	key.pass = pass;

	for( i = 1; i < ba->argc; i++) { /* skip argv[0] */
		key.arg = ba->argv[i];
		if (ba->passes[i] == 0) {
			bArgument *a = BLI_ghash_lookup(ba->items, &key);
			BA_ArgCallback func = NULL;
			void *data = NULL;

			if (a) {
				func = a->func;
				data = a->data;
			} else {
				func = default_cb;
				data = default_data;

				if (func) {
					printf("calling default on %s\n", ba->argv[i]);
				}
			}

			if (func) {
				int retval = func(ba->argc - i, ba->argv + i, data);

				if (retval >= 0) {
					int j;

					/* use extra arguments */
					for (j = 0; j <= retval; j++) {
						ba->passes[i + j] = pass;
					}
					i += retval;
				} else if (retval == -1){
					ba->passes[i] = pass;
					break;
				}
			}
		}
	}
}
