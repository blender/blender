/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_map.hh"

struct Image;

namespace blender {

/**
 * \brief Per-source partial update tracking.
 *
 * A single changeset ID tracks all image buffers.
 */
struct ScreenSpacePartialUpdate {
  const Image *image = nullptr;
  int64_t last_changeset_id = -1;

  /** \brief Reset the cursors when switching to a different image. */
  void ensure_image(const Image *new_image)
  {
    if (image != new_image) {
      last_changeset_id = -1;
      image = new_image;
    }
  }
};

}  // namespace blender
