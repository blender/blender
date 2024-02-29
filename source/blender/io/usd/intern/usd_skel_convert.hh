/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BLI_map.hh"
#include "BLI_vector.hh"

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdSkel/bindingAPI.h>

struct Depsgraph;
struct Key;
struct Main;
struct Mesh;
struct Object;
struct ReportList;

namespace blender::io::usd {

struct ImportSettings;

/**
 * This file contains utilities for converting between `UsdSkel` data and
 * Blender armatures and shape keys. The following is a reference on the
 * `UsdSkel` API:
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
 * \param prim: The USD primitive from which blend-shapes will be imported
 * \param reports: the storage for potential warning or error reports (generated using BKE_report
 *                 API).
 * \param import_anim: Whether to import time-sampled weights as shape key
 *                     animation curves
 */
void import_blendshapes(Main *bmain,
                        Object *mesh_obj,
                        const pxr::UsdPrim &prim,
                        ReportList *reports,
                        bool import_anim = true);

/**
 * Import the given USD skeleton as an armature object. Optionally, if the
 * skeleton has an animation defined, the time sampled joint transforms will be
 * imported as bone animation curves.
 *
 * \param bmain: Main pointer
 * \param arm_obj: Armature object to which the bone hierarchy will be added
 * \param skel: The USD skeleton from which bones and animation will be imported
 * \param reports: the storage for potential warning or error reports (generated using BKE_report
 *                 API).
 * \param import_anim: Whether to import time-sampled joint transforms as bone
 *                     animation curves
 */
void import_skeleton(Main *bmain,
                     Object *arm_obj,
                     const pxr::UsdSkelSkeleton &skel,
                     ReportList *reports,
                     bool import_anim = true);
/**
 * Import skinning data from a source USD prim as deform groups and an armature
 * modifier on the given mesh object. If the USD prim does not have a skeleton
 * binding defined, this function is a no-op.
 *
 * \param bmain: Main pointer
 * \param obj: Mesh object to which an armature modifier will be added
 * \param prim: The USD primitive from which skinning data will be imported
 * \param reports: the storage for potential warning or error reports (generated using BKE_report
 *                 API).
 */
void import_mesh_skel_bindings(Main *bmain,
                               Object *mesh_obj,
                               const pxr::UsdPrim &prim,
                               ReportList *reports);

/**
 * Map an object to its USD prim export path.
 */
using ObjExportMap = Map<const Object *, pxr::SdfPath>;

/**
 * This function is called after the USD writers are invoked, to
 * complete the UsdSkel export process, for example, to bind skinned
 * meshes to skeletons or to set blend shape animation data.
 *
 * \param stage: The stage
 * \param armature_export_map: Map armature objects to USD skeletons
 * \param skinned_mesh_export_map: Map mesh objects to USD skinned meshes
 * \param shape_key_export_map: Map mesh objects with shape-key to USD meshes
 *                              with blend shape targets
 * \param depsgraph: The dependency graph in which objects were evaluated
 */
void skel_export_chaser(pxr::UsdStageRefPtr stage,
                        const ObjExportMap &armature_export_map,
                        const ObjExportMap &skinned_mesh_export_map,
                        const ObjExportMap &shape_key_mesh_export_map,
                        const Depsgraph *depsgraph);

/**
 * Complete the export process for skinned meshes.
 *
 * \param stage: The stage
 * \param armature_export_map: Map armature objects to USD skeleton paths
 * \param skinned_mesh_export_map: Map mesh objects to USD skinned meshes
 * \param xf_cache: Cache to speed up USD prim transform computations
 * \param depsgraph: The dependency graph in which objects were evaluated
 */
void skinned_mesh_export_chaser(pxr::UsdStageRefPtr stage,
                                const ObjExportMap &armature_export_map,
                                const ObjExportMap &skinned_mesh_export_map,
                                pxr::UsdGeomXformCache &xf_cache,
                                const Depsgraph *depsgraph);

/**
 * Complete the export process for shape keys.
 *
 * \param stage: The stage
 * \param shape_key_export_map: Map mesh objects with shape-key to USD meshes
 *                              with blend shape targets
 */
void shape_key_export_chaser(pxr::UsdStageRefPtr stage,
                             const ObjExportMap &shape_key_mesh_export_map);

/**
 * Convert deform groups on the given mesh to USD joint index and weight attributes.
 *
 * \param stage: The source mesh with deform groups to export
 * \param skel_api:  API for setting the attributes on the USD prim
 * \param bone_names:  List of armature bone names corresponding to the deform groups
 */
void export_deform_verts(const Mesh *mesh,
                         const pxr::UsdSkelBindingAPI &skel_api,
                         Span<std::string> bone_names);

}  // namespace blender::io::usd
