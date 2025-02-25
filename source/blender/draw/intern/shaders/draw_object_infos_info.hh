/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_shader_shared.hh"
#  include "gpencil_shader_shared.h"

#  include "draw_view_info.hh"

#  define HAIR_SHADER
#  define DRW_GPENCIL_INFO

#  define OrcoTexCoFactors (drw_infos[resource_id].orco_mul_bias)
#  define ObjectInfo (drw_infos[resource_id].infos)
#  define ObjectColor (drw_infos[resource_id].ob_color)

#  define ObjectAttributeStart (drw_infos[resource_id].orco_mul_bias[0].w)
#  define ObjectAttributeLen (drw_infos[resource_id].orco_mul_bias[1].w)
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
DEFINE("OBINFO_NEW")
DEFINE_VALUE("OrcoTexCoFactors", "(drw_infos[resource_id].orco_mul_bias)")
DEFINE_VALUE("ObjectInfo", "(drw_infos[resource_id].infos)")
DEFINE_VALUE("ObjectColor", "(drw_infos[resource_id].ob_color)")
STORAGE_BUF(DRW_OBJ_INFOS_SLOT, READ, ObjectInfos, drw_infos[])
GPU_SHADER_CREATE_END()

/** \note Requires draw_object_infos. */
GPU_SHADER_CREATE_INFO(draw_object_attributes)
DEFINE("OBATTR_LIB")
DEFINE_VALUE("ObjectAttributeStart", "(drw_infos[resource_id].orco_mul_bias[0].w)")
DEFINE_VALUE("ObjectAttributeLen", "(drw_infos[resource_id].orco_mul_bias[1].w)")
STORAGE_BUF(DRW_OBJ_ATTR_SLOT, READ, ObjectAttribute, drw_attrs[])
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
SAMPLER(0, FLOAT_BUFFER, hairPointBuffer)
/* TODO(@fclem): Pack these into one UBO. */
/* hairStrandsRes: Number of points per hair strand.
 * 2 - no subdivision
 * 3+ - 1 or more interpolated points per hair. */
PUSH_CONSTANT(INT, hairStrandsRes)
/* hairThicknessRes : Subdivide around the hair.
 * 1 - Wire Hair: Only one pixel thick, independent of view distance.
 * 2 - Poly-strip Hair: Correct width, flat if camera is parallel.
 * 3+ - Cylinder Hair: Massive calculation but potentially perfect. Still need proper support. */
PUSH_CONSTANT(INT, hairThicknessRes)
/* Hair thickness shape. */
PUSH_CONSTANT(FLOAT, hairRadRoot)
PUSH_CONSTANT(FLOAT, hairRadTip)
PUSH_CONSTANT(FLOAT, hairRadShape)
PUSH_CONSTANT(BOOL, hairCloseTip)
/* Strand batch offset when used in compute shaders. */
PUSH_CONSTANT(INT, hairStrandOffset)
/* Hair particles are stored in world space coordinate.
 * This matrix convert to the instance "world space". */
PUSH_CONSTANT(MAT4, hairDupliMatrix)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_pointcloud)
SAMPLER_FREQ(0, FLOAT_BUFFER, ptcloud_pos_rad_tx, BATCH)
DEFINE("POINTCLOUD_SHADER")
DEFINE("DRW_POINTCLOUD_INFO")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_volume)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_resource_handle_new)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_gpencil)
TYPEDEF_SOURCE("gpencil_shader_shared.h")
DEFINE("DRW_GPENCIL_INFO")
SAMPLER(0, FLOAT_BUFFER, gp_pos_tx)
SAMPLER(1, FLOAT_BUFFER, gp_col_tx)
/* Per Object */
PUSH_CONSTANT(FLOAT, gpThicknessScale)      /* TODO(fclem): Replace with object info. */
PUSH_CONSTANT(FLOAT, gpThicknessWorldScale) /* TODO(fclem): Same as above. */
DEFINE_VALUE("gpThicknessIsScreenSpace", "(gpThicknessWorldScale < 0.0)")
/* Per Layer */
PUSH_CONSTANT(FLOAT, gpThicknessOffset)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
GPU_SHADER_CREATE_END()

/** \} */
