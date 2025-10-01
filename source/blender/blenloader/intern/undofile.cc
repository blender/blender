/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>

/* open/close */
#ifndef _WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_implicit_sharing.hh"

#include "BLO_readfile.hh"
#include "BLO_undofile.hh"

#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_undo_system.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* **************** support for memory-write, for undo buffers *************** */

void BLO_memfile_free(MemFile *memfile)
{
  while (MemFileChunk *chunk = static_cast<MemFileChunk *>(BLI_pophead(&memfile->chunks))) {
    if (chunk->is_identical == false) {
      MEM_freeN(chunk->buf);
    }
    MEM_freeN(chunk);
  }
  MEM_delete(memfile->shared_storage);
  memfile->shared_storage = nullptr;
  memfile->size = 0;
}

MemFileSharedStorage::~MemFileSharedStorage()
{
  for (const blender::ImplicitSharingInfoAndData &data : sharing_info_by_address_id.values()) {
    /* Removing the user makes sure shared data is freed when the undo step was its last owner. */
    data.sharing_info->remove_user_and_delete_if_last();
  }
}

void BLO_memfile_merge(MemFile *first, MemFile *second)
{
  /* We use this mapping to store the memory buffers from second memfile chunks which are not owned
   * by it (i.e. shared with some previous memory steps). */
  blender::Map<const char *, MemFileChunk *> buffer_to_second_memchunk;

  /* First, detect all memchunks in second memfile that are not owned by it. */
  LISTBASE_FOREACH (MemFileChunk *, sc, &second->chunks) {
    if (sc->is_identical) {
      buffer_to_second_memchunk.add(sc->buf, sc);
    }
  }

  /* Now, check all chunks from first memfile (the one we are removing), and if a memchunk owned by
   * it is also used by the second memfile, transfer the ownership. */
  LISTBASE_FOREACH (MemFileChunk *, fc, &first->chunks) {
    if (!fc->is_identical) {
      if (MemFileChunk *sc = buffer_to_second_memchunk.lookup_default(fc->buf, nullptr)) {
        BLI_assert(sc->is_identical);
        sc->is_identical = false;
        fc->is_identical = true;
      }
      /* Note that if the second memfile does not use that chunk, we assume that the first one
       * fully owns it without sharing it with any other memfile, and hence it should be freed with
       * it. */
    }
  }

  BLO_memfile_free(first);
}

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
  mem_data->reference_current_chunk = reference_memfile ? static_cast<MemFileChunk *>(
                                                              reference_memfile->chunks.first) :
                                                          nullptr;

  /* If we have a reference memfile, we generate a mapping between the session_uid's of the
   * IDs stored in that previous undo step, and its first matching memchunk. This will allow
   * us to easily find the existing undo memory storage of IDs even when some re-ordering in
   * current Main data-base broke the order matching with the memchunks from previous step.
   */
  if (reference_memfile != nullptr) {
    uint current_session_uid = MAIN_ID_SESSION_UID_UNSET;
    LISTBASE_FOREACH (MemFileChunk *, mem_chunk, &reference_memfile->chunks) {
      if (!ELEM(mem_chunk->id_session_uid, MAIN_ID_SESSION_UID_UNSET, current_session_uid)) {
        current_session_uid = mem_chunk->id_session_uid;
        mem_data->id_session_uid_mapping.add_new(current_session_uid, mem_chunk);
      }
    }
  }
}

void BLO_memfile_write_finalize(MemFileWriteData *mem_data)
{
  mem_data->id_session_uid_mapping.clear();
}

void BLO_memfile_chunk_add(MemFileWriteData *mem_data, const char *buf, size_t size)
{
  MemFile *memfile = mem_data->written_memfile;
  MemFileChunk **compchunk_step = &mem_data->reference_current_chunk;

  MemFileChunk *curchunk = MEM_mallocN<MemFileChunk>("MemFileChunk");
  curchunk->size = size;
  curchunk->buf = nullptr;
  curchunk->is_identical = false;
  /* This is unsafe in the sense that an app handler or other code that does not
   * perform an undo push may make changes after the last undo push that
   * will then not be undo. Though it's not entirely clear that is wrong behavior. */
  curchunk->is_identical_future = true;
  curchunk->id_session_uid = mem_data->current_id_session_uid;
  BLI_addtail(&memfile->chunks, curchunk);

  /* we compare compchunk with buf */
  if (*compchunk_step != nullptr) {
    MemFileChunk *compchunk = *compchunk_step;
    if (compchunk->size == curchunk->size) {
      if (memcmp(compchunk->buf, buf, size) == 0) {
        curchunk->buf = compchunk->buf;
        curchunk->is_identical = true;
        compchunk->is_identical_future = true;
      }
    }
    *compchunk_step = static_cast<MemFileChunk *>(compchunk->next);
  }

  /* not equal... */
  if (curchunk->buf == nullptr) {
    char *buf_new = MEM_malloc_arrayN<char>(size, "Chunk buffer");
    memcpy(buf_new, buf, size);
    curchunk->buf = buf_new;
    memfile->size += size;
  }
}

Main *BLO_memfile_main_get(MemFile *memfile, Main *bmain, Scene **r_scene)
{
  Main *bmain_undo = nullptr;
  BlendFileReadParams read_params{};
  BlendFileData *bfd = BLO_read_from_memfile(
      bmain, BKE_main_blendfile_path(bmain), memfile, &read_params, nullptr);

  if (bfd) {
    bmain_undo = bfd->main;
    if (r_scene) {
      *r_scene = bfd->curscene;
    }

    MEM_delete(bfd);
  }

  return bmain_undo;
}

static int64_t undo_read(FileReader *reader, void *buffer, size_t size)
{
  UndoReader *undo = (UndoReader *)reader;

  static size_t seek = SIZE_MAX; /* The current position. */
  static size_t offset = 0;      /* Size of previous chunks. */
  static MemFileChunk *chunk = nullptr;
  size_t chunkoffset, readsize, totread;

  undo->memchunk_identical = true;

  if (size == 0) {
    return 0;
  }

  if (seek != size_t(undo->reader.offset)) {
    chunk = static_cast<MemFileChunk *>(undo->memfile->chunks.first);
    seek = 0;

    while (chunk) {
      if (seek + chunk->size > size_t(undo->reader.offset)) {
        break;
      }
      seek += chunk->size;
      chunk = static_cast<MemFileChunk *>(chunk->next);
    }
    offset = seek;
    seek = size_t(undo->reader.offset);
  }

  if (chunk) {
    totread = 0;

    do {
      /* First check if it's on the end if current chunk. */
      if (seek - offset == chunk->size) {
        offset += chunk->size;
        chunk = static_cast<MemFileChunk *>(chunk->next);
      }

      /* Debug, should never happen. */
      if (chunk == nullptr) {
        printf("illegal read, chunk zero\n");
        return 0;
      }

      chunkoffset = seek - offset;
      readsize = size - totread;

      /* Data can be spread over multiple chunks, so clamp size
       * to within this chunk, and then it will read further in
       * the next chunk. */
      if (chunkoffset + readsize > chunk->size) {
        readsize = chunk->size - chunkoffset;
      }

      memcpy(POINTER_OFFSET(buffer, totread), chunk->buf + chunkoffset, readsize);
      totread += readsize;
      undo->reader.offset += (off64_t)readsize;
      seek += readsize;

      /* `is_identical` of current chunk represents whether it changed compared to previous undo
       * step. this is fine in redo case, but not in undo case, where we need an extra flag
       * defined when saving the next (future) step after the one we want to restore, as we are
       * supposed to 'come from' that future undo step, and not the one before current one. */
      undo->memchunk_identical &= undo->undo_direction == STEP_REDO ? chunk->is_identical :
                                                                      chunk->is_identical_future;
    } while (totread < size);

    return int64_t(totread);
  }

  return 0;
}

static void undo_close(FileReader *reader)
{
  MEM_freeN(reader);
}

FileReader *BLO_memfile_new_filereader(MemFile *memfile, int undo_direction)
{
  UndoReader *undo = MEM_callocN<UndoReader>(__func__);

  undo->memfile = memfile;
  undo->undo_direction = undo_direction;

  undo->reader.read = undo_read;
  undo->reader.seek = nullptr;
  undo->reader.close = undo_close;

  return (FileReader *)undo;
}
