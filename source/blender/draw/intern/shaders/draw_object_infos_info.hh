/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_shader_shared.hh"
#  include "gpencil_shader_shared.hh"

#  include "draw_view_info.hh"

#  define HAIR_SHADER
#  define DRW_GPENCIL_INFO
#endif

#include "draw_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_volume_infos)
TYPEDEF_SOURCE("draw_shader_shared.hh")
DEFINE("VOLUME_INFO_LIB")
UNIFORM_BUF_FREQ(DRW_OBJ_DATA_INFO_UBO_SLOT, VolumeInfos, drw_volume, BATCH)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_infos)
TYPEDEF_SOURCE("draw_shader_shared.hh")
UNIFORM_BUF_FREQ(DRW_OBJ_DATA_INFO_UBO_SLOT, CurvesInfos, drw_curves, BATCH)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_layer_attributes)
TYPEDEF_SOURCE("draw_shader_shared.hh")
DEFINE("VLATTR_LIB")
UNIFORM_BUF_FREQ(DRW_LAYER_ATTR_UBO_SLOT,
                 LayerAttribute,
                 drw_layer_attrs[DRW_RESOURCE_CHUNK_LEN],
                 BATCH)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_object_infos)
TYPEDEF_SOURCE("draw_shader_shared.hh")
DEFINE("OBINFO_LIB")
STORAGE_BUF(DRW_OBJ_INFOS_SLOT, read, ObjectInfos, drw_infos[])
GPU_SHADER_CREATE_END()

/** \note Requires draw_object_infos. */
GPU_SHADER_CREATE_INFO(draw_object_attributes)
DEFINE("OBATTR_LIB")
STORAGE_BUF(DRW_OBJ_ATTR_SLOT, read, ObjectAttribute, drw_attrs[])
ADDITIONAL_INFO(draw_object_infos)
GPU_SHADER_CREATE_END()

/* -------------------------------------------------------------------- */
/** \name Geometry Type
 * \{ */

GPU_SHADER_CREATE_INFO(draw_mesh)
ADDITIONAL_INFO(draw_modelmat)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_hair)
DEFINE("HAIR_SHADER")
DEFINE("DRW_HAIR_INFO")
/* Per control points data inside subdivision shader
 * or
 * per tessellated point inside final shader. */
SAMPLER(0, samplerBuffer, hairPointBuffer)
/* TODO(@fclem): Pack these into one UBO. */
/* hairStrandsRes: Number of points per hair strand.
 * 2 - no subdivision
 * 3+ - 1 or more interpolated points per hair. */
PUSH_CONSTANT(int, hairStrandsRes)
/* hairThicknessRes : Subdivide around the hair.
 * 1 - Wire Hair: Only one pixel thick, independent of view distance.
 * 2 - Poly-strip Hair: Correct width, flat if camera is parallel.
 * 3+ - Cylinder Hair: Massive calculation but potentially perfect. Still need proper support. */
PUSH_CONSTANT(int, hairThicknessRes)
/* Hair thickness shape. */
PUSH_CONSTANT(float, hairRadRoot)
PUSH_CONSTANT(float, hairRadTip)
PUSH_CONSTANT(float, hairRadShape)
PUSH_CONSTANT(bool, hairCloseTip)
/* Strand batch offset when used in compute shaders. */
PUSH_CONSTANT(int, hairStrandOffset)
/* Hair particles are stored in world space coordinate.
 * This matrix convert to the instance "world space". */
PUSH_CONSTANT(float4x4, hairDupliMatrix)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_pointcloud)
SAMPLER_FREQ(0, samplerBuffer, ptcloud_pos_rad_tx, BATCH)
DEFINE("POINTCLOUD_SHADER")
DEFINE("DRW_POINTCLOUD_INFO")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_volume)
ADDITIONAL_INFO(draw_modelmat)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_gpencil)
TYPEDEF_SOURCE("gpencil_shader_shared.hh")
DEFINE("DRW_GPENCIL_INFO")
SAMPLER(0, samplerBuffer, gp_pos_tx)
SAMPLER(1, samplerBuffer, gp_col_tx)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
GPU_SHADER_CREATE_END()

/** \} */
