/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_view3d_types.h"

#include "BKE_view3d_runtime.hh"

#include "NOD_eval_log.hh"

namespace blender::bke {

RegionView3DRuntime::RegionView3DRuntime() = default;
RegionView3DRuntime::~RegionView3DRuntime() = default;

eRegionView3D_ViewLock region_view3d_lock_flags_get(const RegionView3D *rv3d)
{
  return rv3d->viewlock | rv3d->runtime->viewlock;
}

}  // namespace blender::bke
