/**
 * A general argument parsing module
 *
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
	bAKey *key;
	BA_ArgCallback func;
	void *data;
} bArgument;

struct bArgs {
	GHash  *items;
	int 	argc;
	char  **argv;
	int	  *passes;
};

static unsigned int case_strhash(void *ptr) {
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

static bArgument *lookUp(struct bArgs *ba, char *arg, int pass, int case_str)
{
	bAKey key;

	key.case_str = case_str;
	key.pass = pass;
	key.arg = arg;

	return BLI_ghash_lookup(ba->items, &key);
}

bArgs *BLI_argsInit(int argc, char **argv)
{
	bArgs *ba = MEM_callocN(sizeof(bArgs), "bArgs");
	ba->passes = MEM_callocN(sizeof(int) * argc, "bArgs passes");
	ba->items = BLI_ghash_new(keyhash, keycmp, "bArgs passes gh");
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

static void internalAdd(struct bArgs *ba, char *arg, int pass, int case_str, BA_ArgCallback cb, void *data)
{
	bArgument *a;
	bAKey *key;

	a = lookUp(ba, arg, pass, case_str);

	if (a) {
		printf("WARNING: conflicting argument\n");
		printf("\ttrying to add '%s' on pass %i, %scase sensitive\n", arg, pass, case_str == 1? "not ": "");
		printf("\tconflict with '%s' on pass %i, %scase sensitive\n\n", a->key->arg, (int)a->key->pass, a->key->case_str == 1? "not ": "");
	}

	a = MEM_callocN(sizeof(bArgument), "bArgument");
	key = MEM_callocN(sizeof(bAKey), "bAKey");

	key->arg = arg;
	key->pass = pass;
	key->case_str = case_str;

	a->key = key;
	a->func = cb;
	a->data = data;

	BLI_ghash_insert(ba->items, key, a);
}

void BLI_argsAdd(struct bArgs *ba, char *arg, int pass, BA_ArgCallback cb, void *data)
{
	internalAdd(ba, arg, pass, 0, cb, data);
}

void BLI_argsAddPair(struct bArgs *ba, char *arg_short, char *arg_long, int pass, BA_ArgCallback cb, void *data)
{
	internalAdd(ba, arg_short, pass, 0, cb, data);
	internalAdd(ba, arg_long, pass, 0, cb, data);
}

void BLI_argsAddCase(struct bArgs *ba, char *arg, int pass, BA_ArgCallback cb, void *data)
{
	internalAdd(ba, arg, pass, 1, cb, data);
}


void BLI_argsParse(struct bArgs *ba, int pass, BA_ArgCallback default_cb, void *default_data)
{
	int i = 0;

	for( i = 1; i < ba->argc; i++) { /* skip argv[0] */
		/* stop on -- */
		if (BLI_streq(ba->argv[i], "--"))
			break;

		if (ba->passes[i] == 0) {
			 /* -1 signal what side of the comparison it is */
			bArgument *a = lookUp(ba, ba->argv[i], pass, -1);
			BA_ArgCallback func = NULL;
			void *data = NULL;

			if (a) {
				func = a->func;
				data = a->data;
			} else {
				func = default_cb;
				data = default_data;
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
