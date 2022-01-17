
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_object_infos)
    .uniform_buf(1, "ObjectInfos", "drw_infos[DRW_RESOURCE_CHUNK_LEN]", Frequency::BATCH);
