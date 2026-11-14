/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstddef>

#include "BLI_implicit_sharing_ptr.hh"

namespace blender::imbuf {

struct PixelPoolAllocation {
  void *data = nullptr;
  ImplicitSharingPtr<> sharing_info;
};

/* Allocate exact-size pixel storage, reusing pooled memory when available.
 * A new ImplicitSharingInfo is created for each allocation, even if pixel memory is reused.
 * This prevents existing weak references from confusing reused memory with the previous image. */
PixelPoolAllocation pixel_pool_allocate(size_t size,
                                        size_t alignment,
                                        bool initialize_pixels,
                                        bool use_pool);

}  // namespace blender::imbuf
