/**
 * A general unordered 2-int pair hash table ADT
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is: none of this file.
 *
 * Contributor(s): Daniel Dunbar
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
 
#ifndef BLI_EDGEHASH_H
#define BLI_EDGEHASH_H

struct EdgeHash;
typedef struct EdgeHash EdgeHash;

typedef	void	(*EdgeHashFreeFP)(void *key);

EdgeHash*		BLI_edgehash_new		(void);
void			BLI_edgehash_free		(EdgeHash *eh, EdgeHashFreeFP valfreefp);

	/* Insert edge (v0,v1) into hash with given value, does
	 * not check for duplicates.
	 */
void			BLI_edgehash_insert		(EdgeHash *eh, int v0, int v1, void *val);

	/* Return value for given edge (v0,v1), or NULL if
	 * if key does not exist in hash. (If need exists 
	 * to differentiate between key-value being NULL and 
	 * lack of key then see BLI_edgehash_lookup_p().
	 */
void*			BLI_edgehash_lookup		(EdgeHash *eh, int v0, int v1);

	/* Return pointer to value for given edge (v0,v1),
	 * or NULL if key does not exist in hash.
	 */
void**			BLI_edgehash_lookup_p	(EdgeHash *eh, int v0, int v1);

	/* Return boolean true/false if edge (v0,v1) in hash. */
int				BLI_edgehash_haskey		(EdgeHash *eh, int v0, int v1);

	/* Return number of keys in hash. */
int				BLI_edgehash_size		(EdgeHash *eh);

	/* Remove all edges from hash. */
void			BLI_edgehash_clear		(EdgeHash *eh, EdgeHashFreeFP valfreefp);

#endif

