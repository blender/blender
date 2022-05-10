/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_object_infos)
    .typedef_source("draw_shader_shared.h")
    .define("OBINFO_LIB")
    .uniform_buf(1, "ObjectInfos", "drw_infos[DRW_RESOURCE_CHUNK_LEN]", Frequency::BATCH);

GPU_SHADER_CREATE_INFO(draw_volume_infos)
    .typedef_source("draw_shader_shared.h")
    .uniform_buf(2, "VolumeInfos", "drw_volume", Frequency::BATCH);
