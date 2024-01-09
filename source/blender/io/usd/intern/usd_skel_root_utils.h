/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.h"

#include <pxr/usd/usd/stage.h>

namespace blender::io::usd {

/**
 * We must structure the scene graph to encapsulate skinned prim under a UsdSkelRoot
 * prim. Per the USD documentation, a SkelRoot is a:
 *
 * "Boundable prim type used to identify a scope beneath which skeletally-posed primitives are
 * defined. A SkelRoot must be defined at or above a skinned primitive for any skinning behaviors
 * in UsdSkel."
 *
 * See: https://openusd.org/23.05/api/class_usd_skel_root.html#details
 *
 * This function attempts to ensure that skinned prims and skeletons are encapsulated
 * under SkelRoots, converting existing Xform primitives to SkelRoots to achieve this,
 * if possible.  In the case where no common ancestor which can be converted to a SkelRoot
 * is found, this function issues a warning.  One way to address such a case is by setting a
 * root prim in the export options, so that this root prim can be converted to a SkelRoot
 * for the entire scene.
 *
 * \param stage: The stage
 * \param params: The export parameters
 */
void create_skel_roots(pxr::UsdStageRefPtr stage, const USDExportParams &params);

}  // namespace blender::io::usd
