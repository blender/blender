
#ifndef GPU_SHADER
#  include "gpu_shader_shared_utils.h"
#endif

#define DRW_SHADER_SHARED_H

#define DRW_RESOURCE_CHUNK_LEN 512

struct ViewInfos {
  /* View matrices */
  float4x4 persmat;
  float4x4 persinv;
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;

  float4 clip_planes[6];
  float4 viewvecs[2];
  /* Should not be here. Not view dependent (only main view). */
  float4 viewcamtexcofac;
};
BLI_STATIC_ASSERT_ALIGN(ViewInfos, 16)

/* TODO(fclem) Mass rename. */
#define ViewProjectionMatrix drw_view.persmat
#define ViewProjectionMatrixInverse drw_view.persinv
#define ViewMatrix drw_view.viewmat
#define ViewMatrixInverse drw_view.viewinv
#define ProjectionMatrix drw_view.winmat
#define ProjectionMatrixInverse drw_view.wininv
#define clipPlanes drw_view.clip_planes
#define ViewVecs drw_view.viewvecs
#define CameraTexCoFactors drw_view.viewcamtexcofac

struct ObjectMatrices {
  float4x4 drw_modelMatrix;
  float4x4 drw_modelMatrixInverse;
};
BLI_STATIC_ASSERT_ALIGN(ViewInfos, 16)

struct ObjectInfos {
  float4 drw_OrcoTexCoFactors[2];
  float4 drw_ObjectColor;
  float4 drw_Infos;
};
BLI_STATIC_ASSERT_ALIGN(ViewInfos, 16)

#define OrcoTexCoFactors (drw_infos[resource_id].drw_OrcoTexCoFactors)
#define ObjectInfo (drw_infos[resource_id].drw_Infos)
#define ObjectColor (drw_infos[resource_id].drw_ObjectColor)
