/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 * blenloader readfile private function prototypes.
 */

#pragma once

#include <ostream>

#include "DNA_sdna_pointers.hh"
#include "DNA_sdna_types.h"

#include "BLI_map.hh"
#include "BLI_set.hh"

#include "BLO_undofile.hh"

class WriteWrap;

struct WriteDataStableAddressIDs {
  /**
   * Knows which DNA members are pointers. Those members are overridden when serializing the
   * .blend file to get more stable pointer identifiers.
   */
  std::shared_ptr<blender::dna::pointers::PointersInDNA> sdna_pointers;
  /**
   * Maps each runtime-pointer to a unique identifier that's written in the .blend file.
   *
   * Currently, no pointers are ever removed from this map during writing of a single file.
   * Correctness wise, this is fine. However, when some data-blocks write temporary addresses,
   * those may be reused across IDs while actually pointing to different data. This can break
   * address id stability in some situations. In the future this could be improved by clearing
   * such temporary pointers before writing the next data-block.
   */
  blender::Map<const void *, uint64_t> pointer_map;
  /**
   * Contains all the #pointer_map.values(). This is used to make sure that the same id is never
   * reused for a different pointer. While this is technically allowed in .blend files (when the
   * pointers are local data of different objects), we currently don't always know what type a
   * pointer points to when writing it. So we can't determine if a pointer is local or not.
   */
  blender::Set<uint64_t> used_ids;
  /**
   * The next stable address id is derived from this. This is modified in
   * two cases:
   * - A new stable address is needed, in which case this is just incremented.
   * - A new "section" of the .blend file starts. In this case, this should be reinitialized with
   *   some hash of an identifier of the next section. This makes sure that if the number of
   *   pointers in the previous section is modified, the pointers in the new section are not
   *   affected. A "section" can be anything, but currently a section simply starts when a new
   *   data-block starts. In the future, an API could be added that allows sections to start
   *   within a data-block which could isolate stable pointer ids even more.
   *
   * When creating the new address id, keep in mind that this may be 0 and it may collide with
   * previous hints.
   */
  uint64_t next_id_hint = 0;
};

struct WriteData {
  const SDNA *sdna;
  std::ostream *debug_dst = nullptr;

  struct {
    /** Use for file and memory writing (size stored in max_size). */
    uchar *buf;
    /** Number of bytes used in #WriteData.buf (flushed when exceeded). */
    size_t used_len;

    /** Maximum size of the buffer. */
    size_t max_size;
    /** Threshold above which writes get their own chunk. */
    size_t chunk_size;
  } buffer;

#ifdef USE_WRITE_DATA_LEN
  /** Total number of bytes written. */
  size_t write_len;
#endif

  /** Whether writefile code is currently writing an ID. */
  bool is_writing_id;

  /** Some validation and error handling data. */
  struct {
    /**
     * Set on unlikely case of an error (ignores further file writing). Only used for very
     * low-level errors (like if the actual write on file fails).
     */
    bool critical_error;
    /**
     * A set of all 'old' addresses used as UID of written blocks for the current ID. Allows
     * detecting invalid re-uses of the same address multiple times.
     */
    blender::Set<const void *> per_id_addresses_set;
  } validation_data;

  /**
   * Data to generate stable fake pointer values in written blendfile.
   *
   * \note For undo steps, a partial copy of this data is stored in the written MemFile at the end
   * of the writing, and used to initialize this data on the next undo step writing (see
   * #BLO_memfile_write_init and #BLO_memfile_write_finalize).
   */
  WriteDataStableAddressIDs stable_address_ids;

  /**
   * Keeps track of which shared data has been written for the current ID. This is necessary to
   * avoid writing the same data more than once.
   */
  blender::Set<const void *> per_id_written_shared_addresses;

  /** #MemFile writing (used for undo). */
  MemFileWriteData mem;
  /** When true, write to #WriteData.current, could also call 'is_undo'. */
  bool use_memfile;

  /**
   * Wrap writing, so we can use ZSTD or
   * other compression types later, see: #G_FILE_COMPRESS.
   * Will be nullptr for UNDO.
   */
  WriteWrap *ww;

  /**
   * Timestamp info defined when creating the new WriteData. Used for performance logging.
   */
  double timestamp_init;
};
