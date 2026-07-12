/* SPDX-FileCopyrightText: 2024-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 *
 * To reduce the overhead of GPU updates for painting this file contains a mechanism to detect
 * subsets of the image that are changed. The image is split into chunks of size #CHUNK_SIZE
 * with modification tracking for each.
 *
 * Changes that happen over time are organized in changesets. Consumers of these changes store
 * a changeset ID, and can then query changes since the last changeset ID.
 *
 * Changeset IDs are global across image buffers, which makes it possible to track a single ID
 * without a tight coupling to consumers as follows.
 *
 *   for each ibuf:
 *     IMB_partial_update_flush(ibuf)
 *   new_changest_id = IMB_partial_update_changeset_id_current()
 *   for each ibuf:
 *     changes = IMB_partial_update_collect(ibuf, last_changeset_id)
 *     apply changes
 *   last_changeset_id = new_changest_id
 *
 */

#pragma once

#include "BLI_bit_vector.hh"
#include "BLI_vector.hh"

#include "DNA_vec_types.h"

struct ImBuf;

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Update marking API
 * \{ */

/** Mark a subset of the image CPU buffer as modified. */
void IMB_partial_update_mark_region(ImBuf *ibuf, const rcti &region);

/** Mark the full image CPU buffer as modified. */
void IMB_partial_update_mark_full(ImBuf *ibuf);

/** Mark image buffer as having unsaved changes. */
void IMB_mark_dirty(ImBuf *ibuf);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update collect API
 * \{ */

namespace imbuf::partial_update {

/** Size of the chunks that partial changes are tracked in. */
constexpr int CHUNK_SIZE = 256;

/** The changes to an #ImBuf since a given changeset ID. */
struct Changes {
  enum class Kind {
    /** Nothing changed. */
    None,
    /** Some regions of the image buffer changed. */
    Partial,
    /** Full image buffer needs to be updated. */
    Full,
    /** Image buffer resolution changed. */
    Resized,
  };

  Kind kind = Kind::None;

  /* Buffer resolution. */
  int buffer_width = 0;
  int buffer_height = 0;

  /* Chunk grid resolution of #modified_chunks. */
  int chunk_x_len = 0;
  int chunk_y_len = 0;

  /* Modified bitmask for chunks of size #CHUNK_SIZE x #CHUNK_SIZE. */
  BitVector<> modified_chunks;

  /** Mark all chunks as modified. */
  void set_all_chunks_modified();

  /** Get merged rectangular changed regions. */
  Vector<rcti> modified_regions() const;
};

}  // namespace imbuf::partial_update

/** Flush updates in an image buffer, must be called before #IMB_partial_update_collect. */
void IMB_partial_update_flush(ImBuf *ibuf);

/* Current global changeset ID. */
int64_t IMB_partial_update_changeset_id_current();

/** Next global changeset ID. */
int64_t IMB_partial_update_changeset_id_next();

/** Collect the changes to an image buffer since #last_changeset_id. */
imbuf::partial_update::Changes IMB_partial_update_collect(ImBuf *ibuf, int64_t last_changeset_id);

/** \} */

/** Free the partial updaate storage. */
void IMB_partial_update_free(ImBuf *ibuf);

}  // namespace blender
