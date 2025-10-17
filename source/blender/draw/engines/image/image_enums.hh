/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_enum_flags.hh"

namespace blender::image_engine {

/* Shader parameters. */
enum class ImageDrawFlags {
  DEFAULT = 0,
  SHOW_ALPHA = (1 << 0),
  APPLY_ALPHA = (1 << 1),
  SHUFFLING = (1 << 2),
  DEPTH = (1 << 3)
};
ENUM_OPERATORS(ImageDrawFlags);

}  // namespace blender::image_engine
