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
 * The Original Code is Copyright (C) 2022 NVIDIA Corporation.
 * All rights reserved.
 */
#pragma once

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>
#include <map>

struct Main;
struct Object;
struct Scene;
struct USDExportParams;
struct USDImportParams;

namespace blender::io::usd {

struct ImportSettings;

void test_create_shapekeys(Main *bmain, Object *shape_obj);

void import_blendshapes(Main *bmain, Object *shape_obj, pxr::UsdPrim prim);

void create_skeleton_curves(Main *bmain,
                            Object *obj,
                            const pxr::UsdSkelSkeletonQuery &skel_query,
                            const std::map<pxr::TfToken, std::string> &joint_to_bone_map);

void import_skel_bindings(Main *bmain, Object *shape_obj, pxr::UsdPrim prim);

bool compute_skel_space_bind_transforms(const pxr::UsdSkelSkeletonQuery &skel_query,
                                        pxr::VtMatrix4dArray &out_xforms,
                                        pxr::UsdTimeCode time);

pxr::GfMatrix4d get_world_matrix(const pxr::UsdPrim &prim, pxr::UsdTimeCode time);

}  // namespace blender::io::usd
