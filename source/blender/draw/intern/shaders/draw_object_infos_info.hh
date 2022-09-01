/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_defines.h"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_object_infos)
    .typedef_source("draw_shader_shared.h")
    .define("OBINFO_LIB")
    .define("OrcoTexCoFactors", "(drw_infos[resource_id].orco_mul_bias)")
    .define("ObjectInfo", "(drw_infos[resource_id].infos)")
    .define("ObjectColor", "(drw_infos[resource_id].color)")
    .uniform_buf(1, "ObjectInfos", "drw_infos[DRW_RESOURCE_CHUNK_LEN]", Frequency::BATCH);

GPU_SHADER_CREATE_INFO(draw_volume_infos)
    .typedef_source("draw_shader_shared.h")
    .uniform_buf(2, "VolumeInfos", "drw_volume", Frequency::BATCH);

GPU_SHADER_CREATE_INFO(draw_curves_infos)
    .typedef_source("draw_shader_shared.h")
    .uniform_buf(2, "CurvesInfos", "drw_curves", Frequency::BATCH);

GPU_SHADER_CREATE_INFO(draw_object_infos_new)
    .typedef_source("draw_shader_shared.h")
    .define("OBINFO_LIB")
    .define("OrcoTexCoFactors", "(drw_infos[resource_id].orco_mul_bias)")
    .define("ObjectInfo", "(drw_infos[resource_id].infos)")
    .define("ObjectColor", "(drw_infos[resource_id].color)")
    .storage_buf(DRW_OBJ_INFOS_SLOT, Qualifier::READ, "ObjectInfos", "drw_infos[]");

/** \note Requires draw_object_infos_new. */
GPU_SHADER_CREATE_INFO(draw_object_attribute_new)
    .define("OBATTR_LIB")
    .define("ObjectAttributeStart", "(drw_infos[resource_id].orco_mul_bias[0].w)")
    .define("ObjectAttributeLen", "(drw_infos[resource_id].orco_mul_bias[1].w)")
    .storage_buf(DRW_OBJ_ATTR_SLOT, Qualifier::READ, "ObjectAttribute", "drw_attrs[]")
    .additional_info("draw_object_infos_new");
