/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_pool_private.hh"

namespace blender::gpu {
/* TODO(not_mark): implement backend specific `MTLTexturePool`. */
using MTLTexturePool = TexturePoolImpl;
}  // namespace blender::gpu
