/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup blenloader
 * External write-file function prototypes.
 */

#include "BLI_filereader.h"
#include "BLI_implicit_sharing.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"

struct Main;
struct Scene;

struct MemFileSharedStorage {
  /**
   * Maps the address id to the shared data and corresponding sharing info..
   */
  blender::Map<uint64_t, blender::ImplicitSharingInfoAndData> sharing_info_by_address_id;

  ~MemFileSharedStorage();
};

struct MemFileChunk {
  void *next, *prev;
  const char *buf;
  /** Size in bytes. */
  size_t size;
  /** When true, this chunk doesn't own the memory, it's shared with a previous #MemFileChunk */
  bool is_identical;
  /** When true, this chunk is also identical to the one in the next step (used by undo code to
   * detect unchanged IDs).
   * Defined when writing the next step (i.e. last undo step has those always false). */
  bool is_identical_future;
  /** Session UID of the ID being currently written (MAIN_ID_SESSION_UID_UNSET when not writing
   * ID-related data). Used to find matching chunks in previous memundo step. */
  uint id_session_uid;
};

struct MemFile {
  ListBase chunks;
  size_t size;
  /**
   * Some data is not serialized into a new buffer because the undo-step can take ownership of it
   * without making a copy. This is faster and requires less memory.
   */
  MemFileSharedStorage *shared_storage;
};

struct MemFileWriteData {
  MemFile *written_memfile;
  MemFile *reference_memfile;

  uint current_id_session_uid;
  MemFileChunk *reference_current_chunk;

  /** Maps an ID session uid to its first reference MemFileChunk, if existing. */
  blender::Map<uint, MemFileChunk *> id_session_uid_mapping;
};

struct MemFileUndoData {
  char filepath[/*FILE_MAX*/ 1024];
  MemFile memfile;
  size_t undo_size;
};

/* FileReader-compatible wrapper for reading MemFiles */
struct UndoReader {
  FileReader reader;

  MemFile *memfile;
  int undo_direction;

  bool memchunk_identical;
};

/* Actually only used `writefile.cc`. */

void BLO_memfile_write_init(MemFileWriteData *mem_data,
                            MemFile *written_memfile,
                            MemFile *reference_memfile);
void BLO_memfile_write_finalize(MemFileWriteData *mem_data);

void BLO_memfile_chunk_add(MemFileWriteData *mem_data, const char *buf, size_t size);

/* exports */

/**
 * Not memfile itself.
 */
/* **************** support for memory-write, for undo buffers *************** */

void BLO_memfile_free(MemFile *memfile);
/**
 * Result is that 'first' is being freed.
 * To keep the #MemFile linked list of consistent, `first` is always first in list.
 */
void BLO_memfile_merge(MemFile *first, MemFile *second);
/**
 * Clear is_identical_future before adding next memfile.
 */
void BLO_memfile_clear_future(MemFile *memfile);

/* Utilities. */

Main *BLO_memfile_main_get(MemFile *memfile, Main *bmain, Scene **r_scene);

FileReader *BLO_memfile_new_filereader(MemFile *memfile, int undo_direction);
