/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_utildefines.h"

namespace blender::draw::image_engine {

/* Shader parameters. */
enum class ImageDrawFlags {
  Default = 0,
  ShowAlpha = (1 << 0),
  ApplyAlpha = (1 << 1),
  Shuffling = (1 << 2),
  Depth = (1 << 3)
};
ENUM_OPERATORS(ImageDrawFlags, ImageDrawFlags::Depth);

}  // namespace blender::draw::image_engine
