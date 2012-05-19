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
 * The Original Code is: none of this file.
 *
 * Contributor(s): Daniel Dunbar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef __BLI_EDGEHASH_H__
#define __BLI_EDGEHASH_H__

/** \file BLI_edgehash.h
 *  \ingroup bli
 *  \author Daniel Dunbar
 *  \brief A general unordered 2-int pair hash table ADT.
 */

struct EdgeHash;
struct EdgeHashIterator;
typedef struct EdgeHash EdgeHash;
typedef struct EdgeHashIterator EdgeHashIterator;

typedef void (*EdgeHashFreeFP)(void *key);

EdgeHash       *BLI_edgehash_new(void);
void            BLI_edgehash_free(EdgeHash *eh, EdgeHashFreeFP valfreefp);

/* Insert edge (v0,v1) into hash with given value, does
 * not check for duplicates.
 */
void            BLI_edgehash_insert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val);

/* Return value for given edge (v0,v1), or NULL if
 * if key does not exist in hash. (If need exists
 * to differentiate between key-value being NULL and
 * lack of key then see BLI_edgehash_lookup_p().
 */
void           *BLI_edgehash_lookup(EdgeHash *eh, unsigned int v0, unsigned int v1);

/* Return pointer to value for given edge (v0,v1),
 * or NULL if key does not exist in hash.
 */
void          **BLI_edgehash_lookup_p(EdgeHash *eh, unsigned int v0, unsigned int v1);

/* Return boolean true/false if edge (v0,v1) in hash. */
int             BLI_edgehash_haskey(EdgeHash *eh, unsigned int v0, unsigned int v1);

/* Return number of keys in hash. */
int             BLI_edgehash_size(EdgeHash *eh);

/* Remove all edges from hash. */
void            BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP valfreefp);

/***/

/**
 * Create a new EdgeHashIterator. The hash table must not be mutated
 * while the iterator is in use, and the iterator will step exactly
 * BLI_edgehash_size(gh) times before becoming done.
 */
EdgeHashIterator   *BLI_edgehashIterator_new(EdgeHash *eh);

/* Free an EdgeHashIterator. */
void                BLI_edgehashIterator_free(EdgeHashIterator *ehi);

/* Retrieve the key from an iterator. */
void                BLI_edgehashIterator_getKey(EdgeHashIterator *ehi, unsigned int *v0_r, unsigned int *v1_r);

/* Retrieve the value from an iterator. */
void               *BLI_edgehashIterator_getValue(EdgeHashIterator *ehi);

/* Set the value for an iterator. */
void                BLI_edgehashIterator_setValue(EdgeHashIterator *ehi, void *val);

/* Steps the iterator to the next index. */
void                BLI_edgehashIterator_step(EdgeHashIterator *ehi);

/* Determine if an iterator is done. */
int                 BLI_edgehashIterator_isDone(EdgeHashIterator *ehi);

#endif
