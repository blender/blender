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

#include "WM_api.hh"

#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/root.h>

#include <iostream>

namespace blender::io::usd {

bool USDSkelRootWriter::is_under_skel_root() const
{
  pxr::SdfPath parent_path(usd_export_context_.usd_path);

  parent_path = parent_path.GetParentPath();

  if (parent_path.IsEmpty()) {
    return false;
  }

  pxr::UsdPrim prim = usd_export_context_.stage->GetPrimAtPath(parent_path);

  if (!prim.IsValid()) {
    return false;
  }

  pxr::UsdSkelRoot root = pxr::UsdSkelRoot::Find(prim);

  return static_cast<bool>(root);
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

static pxr::UsdGeomXform get_xform_ancestor(const pxr::UsdPrim &prim1, const pxr::UsdPrim &prim2)
{
  if (!prim1 || !prim2) {
    return pxr::UsdGeomXform();
  }

  pxr::SdfPath prefix = prim1.GetPath().GetCommonPrefix(prim2.GetPath());

  if (prefix.IsEmpty()) {
    return pxr::UsdGeomXform();
  }

  pxr::UsdPrim ancestor = prim1.GetStage()->GetPrimAtPath(prefix);

  if (!ancestor.IsA<pxr::UsdGeomXform>()) {
    ancestor = ancestor.GetParent();
  }

  if (ancestor.IsA<pxr::UsdGeomXform>()) {
    return pxr::UsdGeomXform(ancestor);
  }

  return pxr::UsdGeomXform();
}

void validate_skel_roots(pxr::UsdStageRefPtr stage, const USDExportParams &params)
{
  if (!params.export_armatures || !stage) {
    return;
  }

  bool created_skel_root = false;

  pxr::UsdPrimRange it = stage->Traverse();
  for (pxr::UsdPrim prim : it) {
    if (prim.HasAPI<pxr::UsdSkelBindingAPI>() && !prim.IsA<pxr::UsdSkelSkeleton>()) {

      pxr::UsdSkelBindingAPI skel_bind_api(prim);
      if (skel_bind_api) {
        pxr::UsdSkelSkeleton skel;
        if (skel_bind_api.GetSkeleton(&skel)) {

          if (!skel.GetPrim().IsValid()) {
            std::cout << "WARNING in validate_skel_roots(): invalid skeleton for prim "
                      << prim.GetPath() << std::endl;
            continue;
          }

          pxr::UsdSkelRoot prim_root = pxr::UsdSkelRoot::Find(prim);
          pxr::UsdSkelRoot arm_root = pxr::UsdSkelRoot::Find(skel.GetPrim());

          bool common_root = false;

          if (prim_root && arm_root && prim_root.GetPath() == arm_root.GetPath()) {
            common_root = true;
          }

          if (!common_root) {
            WM_reportf(
                RPT_WARNING,
                "USD Export: skinned prim %s and skeleton %s do not share a common SkelRoot and "
                "may not bind correctly.  See the documentation for possible solutions.\n",
                prim.GetPath().GetAsString().c_str(),
                skel.GetPrim().GetPath().GetAsString().c_str());
            std::cout << "WARNING: skinned prim " << prim.GetPath() << " and skeleton "
                      << skel.GetPrim().GetPath()
                      << " do not share a common SkelRoot and may not bind correctly.  See the "
                         "documentation for possible solutions."
                      << std::endl;

            if (params.fix_skel_root) {
              std::cout << "Attempting to fix the Skel Root hierarchy." << std::endl;
              WM_reportf(
                  RPT_WARNING,
                  "Attempting to fix the Skel Root hierarchy.  See the console for information");

              if (pxr::UsdGeomXform xf = get_xform_ancestor(prim, skel.GetPrim())) {
                /* Enable skeletal processing by setting the type to UsdSkelRoot. */
                std::cout << "Converting Xform prim " << xf.GetPath() << " to a SkelRoot"
                          << std::endl;

                pxr::UsdSkelRoot::Define(stage, xf.GetPath());
                created_skel_root = true;
              }
              else {
                std::cout << "Couldn't find a commone Xform ancestor for skinned prim "
                          << prim.GetPath() << " and skeleton " << skel.GetPrim().GetPath()
                          << " to convert to a USDSkelRoot\n";
                std::cout << "You might wish to group these objects under an Empty in the Blender "
                             "scene.\n";
              }
            }
          }
        }
      }
    }
  }

  if (!created_skel_root) {
    return;
  }

  it = stage->Traverse();
  for (pxr::UsdPrim prim : it) {
    if (prim.IsA<pxr::UsdSkelRoot>()) {
      if (pxr::UsdSkelRoot root = pxr::UsdSkelRoot::Find(prim.GetParent())) {
        /* This is a nested SkelRoot, so convert it to an Xform. */
        std::cout << "Converting nested SkelRoot " << prim.GetPath() << " to an Xform."
                  << std::endl;
        pxr::UsdGeomXform::Define(stage, prim.GetPath());
      }
    }
  }
}

}  // namespace blender::io::usd
