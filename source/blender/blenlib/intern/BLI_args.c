/*
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

/** \file blender/blenlib/intern/BLI_args.c
 *  \ingroup bli
 *  \brief A general argument parsing module
 */

#include <ctype.h> /* for tolower */
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_args.h"
#include "BLI_ghash.h"

static char NO_DOCS[] = "NO DOCUMENTATION SPECIFIED";

struct bArgDoc;
typedef struct bArgDoc {
	struct bArgDoc *next, *prev;
	const char *short_arg;
	const char *long_arg;
	const char *documentation;
	int done;
} bArgDoc;

typedef struct bAKey {
	const char *arg;
	uintptr_t pass; /* cast easier */
	int case_str; /* case specific or not */
} bAKey;

typedef struct bArgument {
	bAKey *key;
	BA_ArgCallback func;
	void *data;
	bArgDoc *doc;
} bArgument;

struct bArgs {
	ListBase docs;
	GHash  *items;
	int argc;
	const char  **argv;
	int *passes;
};

static unsigned int case_strhash(const void *ptr)
{
	const char *s = ptr;
	unsigned int i = 0;
	unsigned char c;

	while ( (c = tolower(*s++)) )
		i = i * 37 + c;

	return i;
}

static unsigned int keyhash(const void *ptr)
{
	const bAKey *k = ptr;
	return case_strhash(k->arg); // ^ BLI_ghashutil_inthash((void*)k->pass);
}

static int keycmp(const void *a, const void *b)
{
	const bAKey *ka = a;
	const bAKey *kb = b;
	if (ka->pass == kb->pass || ka->pass == -1 || kb->pass == -1) { /* -1 is wildcard for pass */
		if (ka->case_str == 1 || kb->case_str == 1)
			return BLI_strcasecmp(ka->arg, kb->arg);
		else
			return strcmp(ka->arg, kb->arg);
	}
	else {
		return BLI_ghashutil_intcmp((const void *)ka->pass, (const void *)kb->pass);
	}
}

static bArgument *lookUp(struct bArgs *ba, const char *arg, int pass, int case_str)
{
	bAKey key;

	key.case_str = case_str;
	key.pass = pass;
	key.arg = arg;

	return BLI_ghash_lookup(ba->items, &key);
}

bArgs *BLI_argsInit(int argc, const char **argv)
{
	bArgs *ba = MEM_callocN(sizeof(bArgs), "bArgs");
	ba->passes = MEM_callocN(sizeof(int) * argc, "bArgs passes");
	ba->items = BLI_ghash_new(keyhash, keycmp, "bArgs passes gh");
	ba->docs.first = ba->docs.last = NULL;
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
	BLI_freelistN(&ba->docs);
	MEM_freeN(ba);
}

void BLI_argsPrint(struct bArgs *ba)
{
	int i;
	for (i = 0; i < ba->argc; i++) {
		printf("argv[%d] = %s\n", i, ba->argv[i]);
	}
}

const char **BLI_argsArgv(struct bArgs *ba)
{
	return ba->argv;
}

static bArgDoc *internalDocs(struct bArgs *ba, const char *short_arg, const char *long_arg, const char *doc)
{
	bArgDoc *d;

	d = MEM_callocN(sizeof(bArgDoc), "bArgDoc");

	if (doc == NULL)
		doc = NO_DOCS;

	d->short_arg = short_arg;
	d->long_arg = long_arg;
	d->documentation = doc;

	BLI_addtail(&ba->docs, d);

	return d;
}

static void internalAdd(struct bArgs *ba, const char *arg, int pass, int case_str, BA_ArgCallback cb, void *data, bArgDoc *d)
{
	bArgument *a;
	bAKey *key;

	a = lookUp(ba, arg, pass, case_str);

	if (a) {
		printf("WARNING: conflicting argument\n");
		printf("\ttrying to add '%s' on pass %i, %scase sensitive\n", arg, pass, case_str == 1 ? "not " : "");
		printf("\tconflict with '%s' on pass %i, %scase sensitive\n\n", a->key->arg, (int)a->key->pass, a->key->case_str == 1 ? "not " : "");
	}

	a = MEM_callocN(sizeof(bArgument), "bArgument");
	key = MEM_callocN(sizeof(bAKey), "bAKey");

	key->arg = arg;
	key->pass = pass;
	key->case_str = case_str;

	a->key = key;
	a->func = cb;
	a->data = data;
	a->doc = d;

	BLI_ghash_insert(ba->items, key, a);
}

void BLI_argsAddCase(struct bArgs *ba, int pass, const char *short_arg, int short_case, const char *long_arg, int long_case, const char *doc, BA_ArgCallback cb, void *data)
{
	bArgDoc *d = internalDocs(ba, short_arg, long_arg, doc);

	if (short_arg)
		internalAdd(ba, short_arg, pass, short_case, cb, data, d);

	if (long_arg)
		internalAdd(ba, long_arg, pass, long_case, cb, data, d);


}

void BLI_argsAdd(struct bArgs *ba, int pass, const char *short_arg, const char *long_arg, const char *doc, BA_ArgCallback cb, void *data)
{
	BLI_argsAddCase(ba, pass, short_arg, 0, long_arg, 0, doc, cb, data);
}

static void internalDocPrint(bArgDoc *d)
{
	if (d->short_arg && d->long_arg)
		printf("%s or %s", d->short_arg, d->long_arg);
	else if (d->short_arg)
		printf("%s", d->short_arg);
	else if (d->long_arg)
		printf("%s", d->long_arg);

	printf(" %s\n\n", d->documentation);
}

void BLI_argsPrintArgDoc(struct bArgs *ba, const char *arg)
{
	bArgument *a = lookUp(ba, arg, -1, -1);

	if (a) {
		bArgDoc *d = a->doc;

		internalDocPrint(d);

		d->done = 1;
	}
}

void BLI_argsPrintOtherDoc(struct bArgs *ba)
{
	bArgDoc *d;

	for (d = ba->docs.first; d; d = d->next) {
		if (d->done == 0) {
			internalDocPrint(d);
		}
	}
}

void BLI_argsParse(struct bArgs *ba, int pass, BA_ArgCallback default_cb, void *default_data)
{
	int i = 0;

	for (i = 1; i < ba->argc; i++) {  /* skip argv[0] */
		if (ba->passes[i] == 0) {
			/* -1 signal what side of the comparison it is */
			bArgument *a = lookUp(ba, ba->argv[i], pass, -1);
			BA_ArgCallback func = NULL;
			void *data = NULL;

			if (a) {
				func = a->func;
				data = a->data;
			}
			else {
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
				}
				else if (retval == -1) {
					if (a) {
						if (a->key->pass != -1)
							ba->passes[i] = pass;
					}
					break;
				}
			}
		}
	}
}
