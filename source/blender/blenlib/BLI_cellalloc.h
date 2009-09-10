/**
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
 * The Original Code is Copyright (C) 2008 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
	Simple, fast memory allocator that uses many BLI_mempools for allocation.
	use for allocations < 1024 bytes that simply cannot be rewritten to use
	mempools directly or memarenas.

	Should really become part of guardedalloc, but I'm not sure how to
	get it linking with blenlib on all build systems. - joeedh
*/

void *BLI_cellalloc_malloc(long size, char *tag);
void *BLI_cellalloc_calloc(long size, char *tag);
void BLI_cellalloc_free(void *mem);
void BLI_cellalloc_printleaks(void);
int BLI_cellalloc_get_totblock(void);
void BLI_cellalloc_destroy(void);
