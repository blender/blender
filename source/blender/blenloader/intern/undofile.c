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

/** \file
 * \ingroup blenloader
 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* open/close */
#ifndef _WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"

#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "BKE_lib_id.h"
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
  /* We use this mapping to store the memory buffers from second memfile chunks which are not owned
   * by it (i.e. shared with some previous memory steps). */
  GHash *buffer_to_second_memchunk = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);

  /* First, detect all memchunks in second memfile that are not owned by it. */
  for (MemFileChunk *sc = second->chunks.first; sc != NULL; sc = sc->next) {
    if (sc->is_identical) {
      BLI_ghash_insert(buffer_to_second_memchunk, (void *)sc->buf, sc);
    }
  }

  /* Now, check all chunks from first memfile (the one we are removing), and if a memchunk owned by
   * it is also used by the second memfile, transfer the ownership. */
  for (MemFileChunk *fc = first->chunks.first; fc != NULL; fc = fc->next) {
    if (!fc->is_identical) {
      MemFileChunk *sc = BLI_ghash_lookup(buffer_to_second_memchunk, fc->buf);
      if (sc != NULL) {
        BLI_assert(sc->is_identical);
        sc->is_identical = false;
        fc->is_identical = true;
      }
      /* Note that if the second memfile does not use that chunk, we assume that the first one
       * fully owns it without sharing it with any other memfile, and hence it should be freed with
       * it. */
    }
  }

  BLI_ghash_free(buffer_to_second_memchunk, NULL, NULL);

  BLO_memfile_free(first);
}

/* Clear is_identical_future before adding next memfile. */
void BLO_memfile_clear_future(MemFile *memfile)
{
  LISTBASE_FOREACH (MemFileChunk *, chunk, &memfile->chunks) {
    chunk->is_identical_future = false;
  }
}

void BLO_memfile_write_init(MemFileWriteData *mem_data,
                            MemFile *written_memfile,
                            MemFile *reference_memfile)
{
  mem_data->written_memfile = written_memfile;
  mem_data->reference_memfile = reference_memfile;
  mem_data->reference_current_chunk = reference_memfile ? reference_memfile->chunks.first : NULL;

  /* If we have a reference memfile, we generate a mapping between the session_uuid's of the
   * IDs stored in that previous undo step, and its first matching memchunk. This will allow
   * us to easily find the existing undo memory storage of IDs even when some re-ordering in
   * current Main data-base broke the order matching with the memchunks from previous step.
   */
  if (reference_memfile != NULL) {
    mem_data->id_session_uuid_mapping = BLI_ghash_new(
        BLI_ghashutil_inthash_p_simple, BLI_ghashutil_intcmp, __func__);
    uint current_session_uuid = MAIN_ID_SESSION_UUID_UNSET;
    LISTBASE_FOREACH (MemFileChunk *, mem_chunk, &reference_memfile->chunks) {
      if (!ELEM(mem_chunk->id_session_uuid, MAIN_ID_SESSION_UUID_UNSET, current_session_uuid)) {
        current_session_uuid = mem_chunk->id_session_uuid;
        void **entry;
        if (!BLI_ghash_ensure_p(mem_data->id_session_uuid_mapping,
                                POINTER_FROM_UINT(current_session_uuid),
                                &entry)) {
          *entry = mem_chunk;
        }
        else {
          BLI_assert(0);
        }
      }
    }
  }
}

void BLO_memfile_write_finalize(MemFileWriteData *mem_data)
{
  if (mem_data->id_session_uuid_mapping != NULL) {
    BLI_ghash_free(mem_data->id_session_uuid_mapping, NULL, NULL);
  }
}

void BLO_memfile_chunk_add(MemFileWriteData *mem_data, const char *buf, uint size)
{
  MemFile *memfile = mem_data->written_memfile;
  MemFileChunk **compchunk_step = &mem_data->reference_current_chunk;

  MemFileChunk *curchunk = MEM_mallocN(sizeof(MemFileChunk), "MemFileChunk");
  curchunk->size = size;
  curchunk->buf = NULL;
  curchunk->is_identical = false;
  /* This is unsafe in the sense that an app handler or other code that does not
   * perform an undo push may make changes after the last undo push that
   * will then not be undo. Though it's not entirely clear that is wrong behavior. */
  curchunk->is_identical_future = true;
  curchunk->id_session_uuid = mem_data->current_id_session_uuid;
  BLI_addtail(&memfile->chunks, curchunk);

  /* we compare compchunk with buf */
  if (*compchunk_step != NULL) {
    MemFileChunk *compchunk = *compchunk_step;
    if (compchunk->size == curchunk->size) {
      if (memcmp(compchunk->buf, buf, size) == 0) {
        curchunk->buf = compchunk->buf;
        curchunk->is_identical = true;
        compchunk->is_identical_future = true;
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

struct Main *BLO_memfile_main_get(struct MemFile *memfile,
                                  struct Main *oldmain,
                                  struct Scene **r_scene)
{
  struct Main *bmain_undo = NULL;
  BlendFileData *bfd = BLO_read_from_memfile(oldmain,
                                             BKE_main_blendfile_path(oldmain),
                                             memfile,
                                             &(const struct BlendFileReadParams){0},
                                             NULL);

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

  /* note: This is currently used for autosave and 'quit.blend',
   * where _not_ following symlinks is OK,
   * however if this is ever executed explicitly by the user,
   * we may want to allow writing to symlinks.
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
  file = BLI_open(filename, oflags, 0666);

  if (file == -1) {
    fprintf(stderr,
            "Unable to save '%s': %s\n",
            filename,
            errno ? strerror(errno) : "Unknown error opening file");
    return false;
  }

  for (chunk = memfile->chunks.first; chunk; chunk = chunk->next) {
    if ((size_t)write(file, chunk->buf, chunk->size) != chunk->size) {
      break;
    }
  }

  close(file);

  if (chunk) {
    fprintf(stderr,
            "Unable to save '%s': %s\n",
            filename,
            errno ? strerror(errno) : "Unknown error writing file");
    return false;
  }
  return true;
}
