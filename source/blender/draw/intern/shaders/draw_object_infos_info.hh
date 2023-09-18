/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_defines.h"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_object_infos)
    .typedef_source("draw_shader_shared.h")
    .define("OBINFO_LIB")
    .define("OrcoTexCoFactors", "(drw_infos[resource_id].orco_mul_bias)")
    .define("ObjectInfo", "(drw_infos[resource_id].infos)")
    .define("ObjectColor", "(drw_infos[resource_id].ob_color)")
    .uniform_buf(DRW_OBJ_INFOS_UBO_SLOT,
                 "ObjectInfos",
                 "drw_infos[DRW_RESOURCE_CHUNK_LEN]",
                 Frequency::BATCH);

GPU_SHADER_CREATE_INFO(draw_volume_infos)
    .typedef_source("draw_shader_shared.h")
    .uniform_buf(DRW_OBJ_DATA_INFO_UBO_SLOT, "VolumeInfos", "drw_volume", Frequency::BATCH);

GPU_SHADER_CREATE_INFO(draw_curves_infos)
    .typedef_source("draw_shader_shared.h")
    .uniform_buf(DRW_OBJ_DATA_INFO_UBO_SLOT, "CurvesInfos", "drw_curves", Frequency::BATCH);

GPU_SHADER_CREATE_INFO(draw_layer_attributes)
    .typedef_source("draw_shader_shared.h")
    .define("VLATTR_LIB")
    .uniform_buf(DRW_LAYER_ATTR_UBO_SLOT,
                 "LayerAttribute",
                 "drw_layer_attrs[DRW_RESOURCE_CHUNK_LEN]",
                 Frequency::BATCH);

GPU_SHADER_CREATE_INFO(draw_object_infos_new)
    .typedef_source("draw_shader_shared.h")
    .define("OBINFO_LIB")
    .define("OrcoTexCoFactors", "(drw_infos[resource_id].orco_mul_bias)")
    .define("ObjectInfo", "(drw_infos[resource_id].infos)")
    .define("ObjectColor", "(drw_infos[resource_id].ob_color)")
    .storage_buf(DRW_OBJ_INFOS_SLOT, Qualifier::READ, "ObjectInfos", "drw_infos[]");

/** \note Requires draw_object_infos_new. */
GPU_SHADER_CREATE_INFO(draw_object_attribute_new)
    .define("OBATTR_LIB")
    .define("ObjectAttributeStart", "(drw_infos[resource_id].orco_mul_bias[0].w)")
    .define("ObjectAttributeLen", "(drw_infos[resource_id].orco_mul_bias[1].w)")
    .storage_buf(DRW_OBJ_ATTR_SLOT, Qualifier::READ, "ObjectAttribute", "drw_attrs[]")
    .additional_info("draw_object_infos_new");
