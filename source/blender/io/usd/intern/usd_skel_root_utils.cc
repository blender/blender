/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_skel_root_utils.h"

#include "WM_api.hh"

#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/root.h>

#include <iostream>

/* Utility: return the common Xform ancestor of the given prims. Is no such ancestor can
 * be found, return an in valid Xform. */
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

  if (!ancestor) {
    return pxr::UsdGeomXform();
  }

  while (ancestor && !ancestor.IsA<pxr::UsdGeomXform>()) {
    ancestor = ancestor.GetParent();
  }

  if (ancestor && ancestor.IsA<pxr::UsdGeomXform>()) {
    return pxr::UsdGeomXform(ancestor);
  }

  return pxr::UsdGeomXform();
}

namespace blender::io::usd {

void create_skel_roots(pxr::UsdStageRefPtr stage, const USDExportParams &params)
{
  if (!stage || !(params.export_armatures || params.export_shapekeys)) {
    return;
  }

  /* Whether we converted any prims to UsdSkel. */
  bool converted_to_usdskel = false;

  pxr::UsdPrimRange it = stage->Traverse();
  for (pxr::UsdPrim prim : it) {

    if (!prim) {
      continue;
    }

    if (prim.IsA<pxr::UsdSkelSkeleton>() || !prim.HasAPI<pxr::UsdSkelBindingAPI>()) {
      continue;
    }

    pxr::UsdSkelBindingAPI skel_bind_api(prim);

    if (!skel_bind_api) {
      WM_reportf(RPT_WARNING,
                 "%s: couldn't apply UsdSkelBindingAPI to prim %s\n",
                 __func__,
                 prim.GetPath().GetAsString().c_str());
      continue;
    }

    /* If we got here, then this prim has the skel binding API. */

    /* Get this prim's bound skeleton. */
    pxr::UsdSkelSkeleton skel;
    if (!skel_bind_api.GetSkeleton(&skel)) {
      continue;
    }

    if (!skel.GetPrim().IsValid()) {
      WM_reportf(RPT_WARNING,
                 "%s: invalid skeleton for prim %s\n",
                 __func__,
                 prim.GetPath().GetAsString().c_str());
      continue;
    }

    /* Try to find a commmon ancestor of the skinned prim and its bound skeleton. */
    pxr::UsdSkelRoot prim_skel_root = pxr::UsdSkelRoot::Find(prim);
    pxr::UsdSkelRoot skel_skel_root = pxr::UsdSkelRoot::Find(skel.GetPrim());

    if (prim_skel_root && skel_skel_root && prim_skel_root.GetPath() == skel_skel_root.GetPath()) {
      continue;
    }

    if (pxr::UsdGeomXform xf = get_xform_ancestor(prim, skel.GetPrim())) {
      /* We found a common Xform ancestor, so we set its type to UsdSkelRoot. */
      WM_reportf(RPT_INFO,
                 "%s: Converting Xform prim %s to a SkelRoot\n",
                 __func__,
                 prim.GetPath().GetAsString().c_str());

      pxr::UsdSkelRoot::Define(stage, xf.GetPath());
      converted_to_usdskel = true;
    }
    else {
      WM_reportf(RPT_WARNING,
                 "%s: Couldn't find a common Xform ancestor for skinned prim %s "
                 "and skeleton %s to convert to a USD SkelRoot. "
                 "This can be addressed by setting a root primitive in the export options.\n",
                 __func__,
                 prim.GetPath().GetAsString().c_str());
    }
  }

  if (!converted_to_usdskel) {
    return;
  }

  /* Check for nested SkelRoots, i.e., SkelRoots beneath other SkelRoots, which we want to avoid.
   */
  it = stage->Traverse();
  for (pxr::UsdPrim prim : it) {
    if (prim.IsA<pxr::UsdSkelRoot>()) {
      if (pxr::UsdSkelRoot root = pxr::UsdSkelRoot::Find(prim.GetParent())) {
        /* This is a nested SkelRoot, so convert it to an Xform. */
        pxr::UsdGeomXform::Define(stage, prim.GetPath());
      }
    }
  }
}

}  // namespace blender::io::usd
