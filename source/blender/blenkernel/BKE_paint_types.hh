/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"

/** \file
 * \ingroup bke
 */

struct AssetWeakReference;

namespace blender::bke {
struct PaintRuntime : NonCopyable, NonMovable {
  bool initialized = false;
  uint16_t ob_mode = 0;
  AssetWeakReference *previous_active_brush_reference = nullptr;

  PaintRuntime();
  ~PaintRuntime();
};
};  // namespace blender::bke
