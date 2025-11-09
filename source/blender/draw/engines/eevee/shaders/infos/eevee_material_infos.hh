/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "eevee_geom_infos.hh"
#  include "eevee_surf_capture_infos.hh"
#  include "eevee_surf_deferred_infos.hh"
#  include "eevee_surf_depth_infos.hh"
#  include "eevee_surf_forward_infos.hh"
#  include "eevee_surf_hybrid_infos.hh"
#  include "eevee_surf_shadow_infos.hh"
#  include "eevee_surf_volume_infos.hh"
#  include "eevee_surf_world_infos.hh"

#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO

#  define SHADOW_UPDATE_ATOMIC_RASTER
#  define MAT_TRANSPARENT

#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Test shaders
 *
 * Variations that are only there to test shaders at compile time.
 * \{ */

#ifndef NDEBUG

GPU_SHADER_CREATE_INFO(eevee_material_stub)
/* Dummy uniform buffer to detect overlap with material node-tree. */
UNIFORM_BUF(0, int, node_tree)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(eevee_surface_world_world, eevee_geom_world, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_world_curves, eevee_geom_curves, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_world_mesh, eevee_geom_mesh, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_world_pointcloud, eevee_geom_pointcloud, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_world_volume, eevee_geom_volume, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_world, eevee_geom_world, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_curves, eevee_geom_curves, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_mesh, eevee_geom_mesh, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_pointcloud, eevee_geom_pointcloud, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_volume, eevee_geom_volume, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_world, eevee_geom_world, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_curves, eevee_geom_curves, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_mesh, eevee_geom_mesh, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_pointcloud, eevee_geom_pointcloud, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_volume, eevee_geom_volume, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_world, eevee_geom_world, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_curves, eevee_geom_curves, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_mesh, eevee_geom_mesh, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_pointcloud, eevee_geom_pointcloud, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_volume, eevee_geom_volume, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_world, eevee_geom_world, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_curves, eevee_geom_curves, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_mesh, eevee_geom_mesh, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_pointcloud, eevee_geom_pointcloud, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_volume, eevee_geom_volume, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_world, eevee_geom_world, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_curves, eevee_geom_curves, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_mesh, eevee_geom_mesh, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_pointcloud, eevee_geom_pointcloud, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_volume, eevee_geom_volume, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_world, eevee_geom_world, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_curves, eevee_geom_curves, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_mesh, eevee_geom_mesh, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_pointcloud, eevee_geom_pointcloud, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_volume, eevee_geom_volume, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_world, eevee_geom_world, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_curves, eevee_geom_curves, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_mesh, eevee_geom_mesh, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_pointcloud, eevee_geom_pointcloud, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_volume, eevee_geom_volume, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_world, eevee_geom_world, eevee_surf_shadow_tbdr, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_curves, eevee_geom_curves, eevee_surf_shadow_tbdr, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_mesh, eevee_geom_mesh, eevee_surf_shadow_tbdr, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_pointcloud, eevee_geom_pointcloud, eevee_surf_shadow_tbdr, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_volume, eevee_geom_volume, eevee_surf_shadow_tbdr, eevee_material_stub)
/* clang-format on */
#endif

/** \} */
