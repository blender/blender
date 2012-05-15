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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef __BLI_SMALLHASH_H__
#define __BLI_SMALLHASH_H__

/** \file BLI_smallhash.h
 *  \ingroup bli
 */

/* a light stack-friendly hash library,
 * (it uses stack space for smallish hash tables) */

/* based on a doubling non-chaining approach */

typedef struct {
	uintptr_t key;
	void *val;
} SmallHashEntry;

/*how much stack space to use before dynamically allocating memory*/
#define SMSTACKSIZE 521
typedef struct SmallHash {
	SmallHashEntry *table;
	SmallHashEntry _stacktable[SMSTACKSIZE];
	SmallHashEntry _copytable[SMSTACKSIZE];
	SmallHashEntry *stacktable, *copytable;
	int used;
	int curhash;
	int size;
} SmallHash;

typedef struct {
	SmallHash *hash;
	int i;
} SmallHashIter;

void    BLI_smallhash_init(SmallHash *hash);
void    BLI_smallhash_release(SmallHash *hash);
void    BLI_smallhash_insert(SmallHash *hash, uintptr_t key, void *item);
void    BLI_smallhash_remove(SmallHash *hash, uintptr_t key);
void   *BLI_smallhash_lookup(SmallHash *hash, uintptr_t key);
int     BLI_smallhash_haskey(SmallHash *hash, uintptr_t key);
int     BLI_smallhash_count(SmallHash *hash);
void   *BLI_smallhash_iternext(SmallHashIter *iter, uintptr_t *key);
void   *BLI_smallhash_iternew(SmallHash *hash, SmallHashIter *iter, uintptr_t *key);
/* void BLI_smallhash_print(SmallHash *hash); */ /* UNUSED */

#endif /* __BLI_SMALLHASH_H__ */
