/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <map>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>

struct Main;
struct Object;
struct Scene;
struct USDExportParams;
struct USDImportParams;

namespace blender::io::usd {

struct ImportSettings;

/**
 * This file contains utilities for converting between UsdSkel data and
 * Blender armatures and shape keys. The following is a reference on the
 * UsdSkel API:
 *
 * https://openusd.org/23.05/api/usd_skel_page_front.html
 */

/**
 * Import USD blend shapes from a USD primitive as shape keys on a mesh
 * object. Optionally, if the blend shapes have animating weights, the
 * time-sampled weights will be imported as shape key animation curves.
 * If the USD primitive does not have blend shape targets defined, this
 * function is a no-op.
 *
 * \param bmain: Main pointer
 * \param mesh_obj: Mesh object to which imported shape keys will be added
 * \param prim: The USD primitive from which blendshapes will be imported
 * \param import_anim: Whether to import time-sampled weights as shape key
 *                     animation curves
 */
void import_blendshapes(Main *bmain,
                        Object *mesh_obj,
                        const pxr::UsdPrim &prim,
                        bool import_anim = true);

/**
 * Import the given USD skeleton as an armature object. Optionally, if the
 * skeleton has an animation defined, the time sampled joint transforms will be
 * imported as bone animation curves.
 *
 * \param bmain: Main pointer
 * \param arm_obj: Armature object to which the bone hierachy will be added
 * \param skel: The USD skeleton from which bones and animation will be imported
 * \param import_anim: Whether to import time-sampled joint transforms as bone
 *                     animation curves
 */
void import_skeleton(Main *bmain,
                     Object *arm_obj,
                     const pxr::UsdSkelSkeleton &skel,
                     bool import_anim = true);
/**
 * Import skinning data from a source USD prim as deform groups and an armature
 * modifier on the given mesh object. If the USD prim does not have a skeleton
 * binding defined, this function is a no-op.
 *
 * \param bmain: Main pointer
 * \param obj: Mesh object to which an armature modifier will be added
 * \param prim: The USD primitive from which skinning data will be imported
 */
void import_mesh_skel_bindings(Main *bmain, Object *mesh_obj, const pxr::UsdPrim &prim);

}  // namespace blender::io::usd
