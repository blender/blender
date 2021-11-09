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
#include "usd_writer_skel_root.h"

#include <pxr/usd/usdSkel/root.h>

namespace blender::io::usd {

bool USDSkelRootWriter::is_under_skel_root() const
{
  pxr::SdfPath parent_path(usd_export_context_.usd_path);

  parent_path = parent_path.GetParentPath();

  if (parent_path.IsEmpty()) {
    return false;
  }

  pxr::UsdPrim prim = usd_export_context_.stage->GetPrimAtPath(parent_path);

  while (prim.IsValid()) {
    if (prim.IsA<pxr::UsdSkelRoot>()) {
      return true;
    }
    prim = prim.GetParent();
  }

  return false;
}

pxr::UsdGeomXformable USDSkelRootWriter::create_xformable() const
{
  /* Create a UsdSkelRoot primitive, unless this prim is already
    beneath a UsdSkelRoot, in which case create an Xform. */

  pxr::UsdGeomXformable root;

  if (is_under_skel_root()) {
    root = (usd_export_context_.export_params.export_as_overs) ?
               pxr::UsdGeomXform(
                   usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
               pxr::UsdGeomXform::Define(usd_export_context_.stage, usd_export_context_.usd_path);
  }
  else {
    root = (usd_export_context_.export_params.export_as_overs) ?
               pxr::UsdSkelRoot(
                   usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
               pxr::UsdSkelRoot::Define(usd_export_context_.stage, usd_export_context_.usd_path);
  }

  return root;
}

}  // namespace blender::io::usd
