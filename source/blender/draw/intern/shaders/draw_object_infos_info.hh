/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
