/*
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
 * .blend file reading entry point
 */

/** \file \ingroup blenloader
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>

/* open/close */
#ifndef _WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_blenlib.h"

#include "BLO_undofile.h"
#include "BLO_readfile.h"

#include "BKE_main.h"

/* keep last */
#include "BLI_strict_flags.h"

/* **************** support for memory-write, for undo buffers *************** */

/* not memfile itself */
void BLO_memfile_free(MemFile *memfile)
{
	MemFileChunk *chunk;

	while ((chunk = BLI_pophead(&memfile->chunks))) {
		if (chunk->is_identical == false) {
			MEM_freeN((void *)chunk->buf);
		}
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
			if (sc->is_identical) {
				sc->is_identical = false;
				fc->is_identical = true;
			}
		}
		if (fc) fc = fc->next;
		if (sc) sc = sc->next;
	}

	BLO_memfile_free(first);
}

void memfile_chunk_add(
        MemFile *memfile, const char *buf, uint size,
        MemFileChunk **compchunk_step)
{
	MemFileChunk *curchunk = MEM_mallocN(sizeof(MemFileChunk), "MemFileChunk");
	curchunk->size = size;
	curchunk->buf = NULL;
	curchunk->is_identical = false;
	BLI_addtail(&memfile->chunks, curchunk);

	/* we compare compchunk with buf */
	if (*compchunk_step != NULL) {
		MemFileChunk *compchunk = *compchunk_step;
		if (compchunk->size == curchunk->size) {
			if (memcmp(compchunk->buf, buf, size) == 0) {
				curchunk->buf = compchunk->buf;
				curchunk->is_identical = true;
			}
		}
		*compchunk_step = compchunk->next;
	}

	/* not equal... */
	if (curchunk->buf == NULL) {
		char *buf_new = MEM_mallocN(size, "Chunk buffer");
		memcpy(buf_new, buf, size);
		curchunk->buf = buf_new;
		memfile->size += size;
	}
}

struct Main *BLO_memfile_main_get(struct MemFile *memfile, struct Main *oldmain, struct Scene **r_scene)
{
	struct Main *bmain_undo = NULL;
	BlendFileData *bfd = BLO_read_from_memfile(oldmain, BKE_main_blendfile_path(oldmain), memfile, BLO_READ_SKIP_NONE, NULL);

	if (bfd) {
		bmain_undo = bfd->main;
		if (r_scene) {
			*r_scene = bfd->curscene;
		}

		MEM_freeN(bfd);
	}

	return bmain_undo;
}


/**
 * Saves .blend using undo buffer.
 *
 * \return success.
 */
bool BLO_memfile_write_file(struct MemFile *memfile, const char *filename)
{
	MemFileChunk *chunk;
	int file, oflags;

	/* note: This is currently used for autosave and 'quit.blend', where _not_ following symlinks is OK,
	 * however if this is ever executed explicitly by the user, we may want to allow writing to symlinks.
	 */

	oflags = O_BINARY | O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_NOFOLLOW
	/* use O_NOFOLLOW to avoid writing to a symlink - use 'O_EXCL' (CVE-2008-1103) */
	oflags |= O_NOFOLLOW;
#else
	/* TODO(sergey): How to deal with symlinks on windows? */
#  ifndef _MSC_VER
#    warning "Symbolic links will be followed on undo save, possibly causing CVE-2008-1103"
#  endif
#endif
	file = BLI_open(filename,  oflags, 0666);

	if (file == -1) {
		fprintf(stderr, "Unable to save '%s': %s\n",
		        filename, errno ? strerror(errno) : "Unknown error opening file");
		return false;
	}

	for (chunk = memfile->chunks.first; chunk; chunk = chunk->next) {
		if ((size_t)write(file, chunk->buf, chunk->size) != chunk->size) {
			break;
		}
	}

	close(file);

	if (chunk) {
		fprintf(stderr, "Unable to save '%s': %s\n",
		        filename, errno ? strerror(errno) : "Unknown error writing file");
		return false;
	}
	return true;
}
