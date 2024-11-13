/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_shader_shared.hh"

#  include "draw_view_info.hh"
#endif

#include "draw_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_object_infos)
TYPEDEF_SOURCE("draw_shader_shared.hh")
DEFINE("OBINFO_LIB")
DEFINE_VALUE("OrcoTexCoFactors", "(drw_infos[resource_id].orco_mul_bias)")
DEFINE_VALUE("ObjectInfo", "(drw_infos[resource_id].infos)")
DEFINE_VALUE("ObjectColor", "(drw_infos[resource_id].ob_color)")
UNIFORM_BUF_FREQ(DRW_OBJ_INFOS_UBO_SLOT, ObjectInfos, drw_infos[DRW_RESOURCE_CHUNK_LEN], BATCH)
GPU_SHADER_CREATE_END()

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

GPU_SHADER_CREATE_INFO(draw_object_infos_new)
TYPEDEF_SOURCE("draw_shader_shared.hh")
DEFINE("OBINFO_LIB")
DEFINE("OBINFO_NEW")
DEFINE_VALUE("OrcoTexCoFactors", "(drw_infos[resource_id].orco_mul_bias)")
DEFINE_VALUE("ObjectInfo", "(drw_infos[resource_id].infos)")
DEFINE_VALUE("ObjectColor", "(drw_infos[resource_id].ob_color)")
STORAGE_BUF(DRW_OBJ_INFOS_SLOT, READ, ObjectInfos, drw_infos[])
GPU_SHADER_CREATE_END()

/** \note Requires draw_object_infos_new. */
GPU_SHADER_CREATE_INFO(draw_object_attribute_new)
DEFINE("OBATTR_LIB")
DEFINE_VALUE("ObjectAttributeStart", "(drw_infos[resource_id].orco_mul_bias[0].w)")
DEFINE_VALUE("ObjectAttributeLen", "(drw_infos[resource_id].orco_mul_bias[1].w)")
STORAGE_BUF(DRW_OBJ_ATTR_SLOT, READ, ObjectAttribute, drw_attrs[])
ADDITIONAL_INFO(draw_object_infos_new)
GPU_SHADER_CREATE_END()

/* -------------------------------------------------------------------- */
/** \name Geometry Type
 * \{ */

GPU_SHADER_CREATE_INFO(draw_mesh)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_resource_id)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_mesh_new)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_id)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_hair)
DEFINE("HAIR_SHADER")
DEFINE("DRW_HAIR_INFO")
SAMPLER(15, FLOAT_BUFFER, hairPointBuffer)
/* TODO(@fclem): Pack these into one UBO. */
PUSH_CONSTANT(INT, hairStrandsRes)
PUSH_CONSTANT(INT, hairThicknessRes)
PUSH_CONSTANT(FLOAT, hairRadRoot)
PUSH_CONSTANT(FLOAT, hairRadTip)
PUSH_CONSTANT(FLOAT, hairRadShape)
PUSH_CONSTANT(BOOL, hairCloseTip)
PUSH_CONSTANT(INT, hairStrandOffset)
PUSH_CONSTANT(MAT4, hairDupliMatrix)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_resource_id)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_hair_new)
DEFINE("HAIR_SHADER")
DEFINE("DRW_HAIR_INFO")
SAMPLER(0, FLOAT_BUFFER, hairPointBuffer)
/* TODO(@fclem): Pack these into one UBO. */
PUSH_CONSTANT(INT, hairStrandsRes)
PUSH_CONSTANT(INT, hairThicknessRes)
PUSH_CONSTANT(FLOAT, hairRadRoot)
PUSH_CONSTANT(FLOAT, hairRadTip)
PUSH_CONSTANT(FLOAT, hairRadShape)
PUSH_CONSTANT(BOOL, hairCloseTip)
PUSH_CONSTANT(INT, hairStrandOffset)
PUSH_CONSTANT(MAT4, hairDupliMatrix)
GPU_SHADER_CREATE_END()

#ifndef GPU_SHADER /* Conflicts with define for C++ shader test. */
GPU_SHADER_CREATE_INFO(draw_pointcloud)
SAMPLER_FREQ(0, FLOAT_BUFFER, ptcloud_pos_rad_tx, BATCH)
DEFINE("POINTCLOUD_SHADER")
DEFINE("DRW_POINTCLOUD_INFO")
ADDITIONAL_INFO(draw_modelmat_instanced_attr)
ADDITIONAL_INFO(draw_resource_id_uniform)
GPU_SHADER_CREATE_END()
#endif

GPU_SHADER_CREATE_INFO(draw_pointcloud_new)
SAMPLER_FREQ(0, FLOAT_BUFFER, ptcloud_pos_rad_tx, BATCH)
DEFINE("POINTCLOUD_SHADER")
DEFINE("DRW_POINTCLOUD_INFO")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_volume)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_resource_id_uniform)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_volume_new)
ADDITIONAL_INFO(draw_modelmat_new)
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
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_gpencil_new)
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
ADDITIONAL_INFO(draw_object_infos_new)
GPU_SHADER_CREATE_END()

/** \} */
