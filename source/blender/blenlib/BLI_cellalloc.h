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
 this evil bit of code is necassary for vgroups and multires to run fast
 enough.  it is surprisingly tricky allocate MDeformWeights and MDisps in
 a way that doesn't cause severe performance problems.  once a better solution
 is found we can get rid of this code, but until then this is necassary
 (though, disabling it if jedmalloc is in use might be feasible).
 
 ideas for replacement:
	ok, mdisps could store a mempool in CustomDataLayer.  there might be
	one there already?  vgroups, uh. . .not sure what to do with vgroups,
	they do cause a significant performance problem.
	
	it's tempting to split vgroups into lots of little customdata layers,
	but that would waste a LOT of memory.  ugh.  can we plug in jemalloc
	to guardedalloc, on all platforms?  that would work.
	
	I really hate this little library; it really should be replaced before trunk
	reintegration.
	
 - joeedh
*/

void *BLI_cellalloc_malloc(int size, const char *tag);
void *BLI_cellalloc_calloc(int size, const char *tag);
void BLI_cellalloc_free(void *mem);
void BLI_cellalloc_printleaks(void);
int BLI_cellalloc_get_totblock(void);
void BLI_cellalloc_destroy(void);
void *BLI_cellalloc_dupalloc(void *mem);
