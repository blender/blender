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
	I wrote this as a hack so vgroups won't be quite so slow.  I really
	should replace it with something else, but I need to spend some time
	thinking as to what the proper solution would be (other then totally
	rewriting vgroups, of course).

	this is just a simple allocator that spawns mempools for unique size
	requests.  hardly ideal, I know.  *something* like this may be
	unavoidable, but it should certainly be possible to make it
	non-global and internal to the vgroup code.

	-joeedh sep. 17 2009
*/

//BMESH_TODO: kill this library before merging with trunk.  it's evil! -joeedh
void *BLI_cellalloc_malloc(long size, char *tag);
void *BLI_cellalloc_calloc(long size, char *tag);
void BLI_cellalloc_free(void *mem);
void BLI_cellalloc_printleaks(void);
int BLI_cellalloc_get_totblock(void);
void BLI_cellalloc_destroy(void);
void *BLI_cellalloc_dupalloc(void *mem);