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
 
#ifndef __BLI_GHASH_H__
#define __BLI_GHASH_H__

/** \file BLI_ghash.h
 *  \ingroup bli
 *  \brief A general (pointer -> pointer) hash table ADT
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int	(*GHashHashFP)		(const void *key);
typedef int				(*GHashCmpFP)		(const void *a, const void *b);
typedef	void			(*GHashKeyFreeFP)	(void *key);
typedef void			(*GHashValFreeFP)	(void *val);

typedef struct Entry {
	struct Entry *next;

	void *key, *val;
} Entry;

typedef struct GHash {
	GHashHashFP	hashfp;
	GHashCmpFP	cmpfp;

	Entry **buckets;
	struct BLI_mempool *entrypool;
	int nbuckets, nentries, cursize;
} GHash;

typedef struct GHashIterator {
	GHash *gh;
	int curBucket;
	struct Entry *curEntry;
} GHashIterator;

/* *** */

GHash* BLI_ghash_new   (GHashHashFP hashfp, GHashCmpFP cmpfp, const char *info);
void   BLI_ghash_free  (GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp);
void   BLI_ghash_insert(GHash *gh, void *key, void *val);
void * BLI_ghash_lookup(GHash *gh, const void *key);
int    BLI_ghash_remove(GHash *gh, void *key, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp);
int    BLI_ghash_haskey(GHash *gh, void *key);
int	   BLI_ghash_size  (GHash *gh);

/* *** */

	/**
	 * Create a new GHashIterator. The hash table must not be mutated
	 * while the iterator is in use, and the iterator will step exactly
	 * BLI_ghash_size(gh) times before becoming done.
	 * 
	 * \param gh The GHash to iterate over.
	 * \return Pointer to a new DynStr.
	 */
GHashIterator*	BLI_ghashIterator_new		(GHash *gh);
	/**
	 * Init an already allocated GHashIterator. The hash table must not
	 * be mutated while the iterator is in use, and the iterator will
	 * step exactly BLI_ghash_size(gh) times before becoming done.
	 * 
	 * \param ghi The GHashIterator to initialize.
	 * \param gh The GHash to iterate over.
	 */
void BLI_ghashIterator_init(GHashIterator *ghi, GHash *gh);
	/**
	 * Free a GHashIterator.
	 *
	 * \param ghi The iterator to free.
	 */
void			BLI_ghashIterator_free		(GHashIterator *ghi);

	/**
	 * Retrieve the key from an iterator.
	 *
	 * \param ghi The iterator.
	 * \return The key at the current index, or NULL if the 
	 * iterator is done.
	 */
void*			BLI_ghashIterator_getKey	(GHashIterator *ghi);
	/**
	 * Retrieve the value from an iterator.
	 *
	 * \param ghi The iterator.
	 * \return The value at the current index, or NULL if the 
	 * iterator is done.
	 */
void*			BLI_ghashIterator_getValue	(GHashIterator *ghi);
	/**
	 * Steps the iterator to the next index.
	 *
	 * \param ghi The iterator.
	 */
void			BLI_ghashIterator_step		(GHashIterator *ghi);
	/**
	 * Determine if an iterator is done (has reached the end of
	 * the hash table).
	 *
	 * \param ghi The iterator.
	 * \return True if done, False otherwise.
	 */
int				BLI_ghashIterator_isDone	(GHashIterator *ghi);

#define GHASH_ITER(gh_iter_, ghash_)                                          \
	for (BLI_ghashIterator_init(&gh_iter_, ghash_);                           \
	     !BLI_ghashIterator_isDone(&gh_iter_);                                \
	     BLI_ghashIterator_step(&gh_iter_))

/* *** */

unsigned int	BLI_ghashutil_ptrhash	(const void *key);
int				BLI_ghashutil_ptrcmp	(const void *a, const void *b);

unsigned int	BLI_ghashutil_strhash	(const void *key);
int				BLI_ghashutil_strcmp	(const void *a, const void *b);

unsigned int	BLI_ghashutil_inthash	(const void *ptr);
int				BLI_ghashutil_intcmp	(const void *a, const void *b);

typedef struct GHashPair {
	const void *first;
	const void *second;
} GHashPair;

GHashPair*		BLI_ghashutil_pairalloc (const void *first, const void *second);
unsigned int	BLI_ghashutil_pairhash	(const void *ptr);
int				BLI_ghashutil_paircmp	(const void *a, const void *b);
void			BLI_ghashutil_pairfree	(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* __BLI_GHASH_H__ */
