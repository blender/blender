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
 * The Original Code is Copyright (C) 2004 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * .blend file reading entry point
 */

/** \file blender/blenloader/intern/undofile.c
 *  \ingroup blenloader
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_blenlib.h"

#include "BLO_undofile.h"

/* **************** support for memory-write, for undo buffers *************** */

/* not memfile itself */
void BLO_memfile_free(MemFile *memfile)
{
	MemFileChunk *chunk;
	
	while ((chunk = BLI_pophead(&memfile->chunks))) {
		if (chunk->ident == 0)
			MEM_freeN(chunk->buf);
		MEM_freeN(chunk);
	}
	memfile->size = 0;
}

/* to keep list of memfiles consistent, 'first' is always first in list */
/* result is that 'first' is being freed */
void BLO_memfile_merge(MemFile *first, MemFile *second)
{
	MemFileChunk *fc, *sc;
	
	fc = first->chunks.first;
	sc = second->chunks.first;
	while (fc || sc) {
		if (fc && sc) {
			if (sc->ident) {
				sc->ident = 0;
				fc->ident = 1;
			}
		}
		if (fc) fc = fc->next;
		if (sc) sc = sc->next;
	}
	
	BLO_memfile_free(first);
}

void memfile_chunk_add(MemFile *compare, MemFile *current, const char *buf, unsigned int size)
{
	static MemFileChunk *compchunk = NULL;
	MemFileChunk *curchunk;
	
	/* this function inits when compare != NULL or when current == NULL  */
	if (compare) {
		compchunk = compare->chunks.first;
		return;
	}
	if (current == NULL) {
		compchunk = NULL;
		return;
	}
	
	curchunk = MEM_mallocN(sizeof(MemFileChunk), "MemFileChunk");
	curchunk->size = size;
	curchunk->buf = NULL;
	curchunk->ident = 0;
	BLI_addtail(&current->chunks, curchunk);
	
	/* we compare compchunk with buf */
	if (compchunk) {
		if (compchunk->size == curchunk->size) {
			if (memcmp(compchunk->buf, buf, size) == 0) {
				curchunk->buf = compchunk->buf;
				curchunk->ident = 1;
			}
		}
		compchunk = compchunk->next;
	}
	
	/* not equal... */
	if (curchunk->buf == NULL) {
		curchunk->buf = MEM_mallocN(size, "Chunk buffer");
		memcpy(curchunk->buf, buf, size);
		current->size += size;
	}
}

