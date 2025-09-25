/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_shader_shared.hh"
#  include "gpencil_shader_shared.hh"

#  include "draw_view_infos.hh"

#  define CURVES_SHADER
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

GPU_SHADER_CREATE_INFO(draw_curves)
DEFINE("CURVES_SHADER")
DEFINE("DRW_HAIR_INFO")
SAMPLER_FREQ(0, samplerBuffer, curves_pos_rad_buf, BATCH)
SAMPLER_FREQ(1, isamplerBuffer, curves_indirection_buf, BATCH)
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
