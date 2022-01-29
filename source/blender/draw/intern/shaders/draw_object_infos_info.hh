
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_object_infos)
    .typedef_source("draw_shader_shared.h")
    .uniform_buf(1, "ObjectInfos", "drw_infos[DRW_RESOURCE_CHUNK_LEN]", Frequency::BATCH);
