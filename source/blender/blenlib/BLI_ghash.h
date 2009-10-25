/**
 * A general (pointer -> pointer) hash table ADT
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
 
#ifndef BLI_GHASH_H
#define BLI_GHASH_H

#ifdef __cplusplus
extern "C" {
#endif

struct GHash;
typedef struct GHash GHash;

typedef struct GHashIterator {
	GHash *gh;
	int curBucket;
	struct Entry *curEntry;
} GHashIterator;

typedef unsigned int	(*GHashHashFP)		(void *key);
typedef int				(*GHashCmpFP)		(void *a, void *b);
typedef	void			(*GHashKeyFreeFP)	(void *key);
typedef void			(*GHashValFreeFP)	(void *val);

GHash*	BLI_ghash_new		(GHashHashFP hashfp, GHashCmpFP cmpfp);
void	BLI_ghash_free		(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp);

void	BLI_ghash_insert	(GHash *gh, void *key, void *val);
int		BLI_ghash_remove	(GHash *gh, void *key, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp);
void*	BLI_ghash_lookup	(GHash *gh, void *key);
int		BLI_ghash_haskey	(GHash *gh, void *key);

int		BLI_ghash_size		(GHash *gh);

/* *** */

	/**
	 * Create a new GHashIterator. The hash table must not be mutated
	 * while the iterator is in use, and the iterator will step exactly
	 * BLI_ghash_size(gh) times before becoming done.
	 * 
	 * @param gh The GHash to iterate over.
	 * @return Pointer to a new DynStr.
	 */
GHashIterator*	BLI_ghashIterator_new		(GHash *gh);
	/**
	 * Init an already allocated GHashIterator. The hash table must not
	 * be mutated while the iterator is in use, and the iterator will
	 * step exactly BLI_ghash_size(gh) times before becoming done.
	 * 
	 * @param ghi The GHashIterator to initialize.
	 * @param gh The GHash to iterate over.
	 */
void BLI_ghashIterator_init(GHashIterator *ghi, GHash *gh);
	/**
	 * Free a GHashIterator.
	 *
	 * @param ghi The iterator to free.
	 */
void			BLI_ghashIterator_free		(GHashIterator *ghi);

	/**
	 * Retrieve the key from an iterator.
	 *
	 * @param ghi The iterator.
	 * @return The key at the current index, or NULL if the 
	 * iterator is done.
	 */
void*			BLI_ghashIterator_getKey	(GHashIterator *ghi);
	/**
	 * Retrieve the value from an iterator.
	 *
	 * @param ghi The iterator.
	 * @return The value at the current index, or NULL if the 
	 * iterator is done.
	 */
void*			BLI_ghashIterator_getValue	(GHashIterator *ghi);
	/**
	 * Steps the iterator to the next index.
	 *
	 * @param ghi The iterator.
	 */
void			BLI_ghashIterator_step		(GHashIterator *ghi);
	/**
	 * Determine if an iterator is done (has reached the end of
	 * the hash table).
	 *
	 * @param ghi The iterator.
	 * @return True if done, False otherwise.
	 */
int				BLI_ghashIterator_isDone	(GHashIterator *ghi);

/* *** */

unsigned int	BLI_ghashutil_ptrhash	(void *key);
int				BLI_ghashutil_ptrcmp	(void *a, void *b);

unsigned int	BLI_ghashutil_strhash	(void *key);
int				BLI_ghashutil_strcmp	(void *a, void *b);

unsigned int	BLI_ghashutil_inthash	(void *ptr);
int				BLI_ghashutil_intcmp(void *a, void *b);

#ifdef __cplusplus
}
#endif

#endif

