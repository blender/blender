/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 NVIDIA Corporation.
 * All rights reserved.
 */
#pragma once

#include "usd_writer_transform.h"

#include <pxr/usd/usdGeom/xformable.h>

namespace blender::io::usd {

void validate_skel_roots(pxr::UsdStageRefPtr stage, const USDExportParams &params);

class USDSkelRootWriter : public USDTransformWriter {

 public:
  USDSkelRootWriter(const USDExporterContext &ctx) : USDTransformWriter(ctx) {}

 protected:
  /* Override to create UsdSkelRoot prim. */
  pxr::UsdGeomXformable create_xformable() const override;

  /* Rturns true if the prim to be created is
   * already unde a USD SkeRoot. */
  bool is_under_skel_root() const;
};

}  // namespace blender::io::usd
