/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_asset_types.h"

#include "BKE_paint_types.hh"

namespace blender::bke {
PaintRuntime::PaintRuntime() = default;
PaintRuntime::~PaintRuntime()
{
  MEM_delete(this->previous_active_brush_reference);
}
}  // namespace blender::bke
