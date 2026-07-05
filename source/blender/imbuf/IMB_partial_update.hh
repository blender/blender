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
 */

#pragma once

#include "BLI_vector.hh"

#include "DNA_vec_types.h"

struct ImBuf;

namespace blender {

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
  };

  Kind kind = Kind::None;

  /** New changeset ID after these changes, to use for the next collect. */
  int64_t last_changeset_id = -1;

  /** Modified regions for partial image buffer change. */
  Vector<rcti> updated_regions;
};

}  // namespace imbuf::partial_update

/** Mark a subset of the image CPU buffer as modified. */
void IMB_partial_update_mark_region(ImBuf *ibuf, const rcti &region);

/** Mark the full image CPU buffer as modified. */
void IMB_partial_update_mark_full(ImBuf *ibuf);

/** Collect the changes to an image buffer since #last_changeset_id. */
imbuf::partial_update::Changes IMB_partial_update_collect(ImBuf *ibuf, int64_t last_changeset_id);

/** Free the partial updaate storage. */
void IMB_partial_update_free(ImBuf *ibuf);

/** Next global changeset ID. */
int64_t IMB_partial_update_changeset_id_next();

/* Current global changeset ID. */
int64_t IMB_partial_update_changeset_id_current();

}  // namespace blender
