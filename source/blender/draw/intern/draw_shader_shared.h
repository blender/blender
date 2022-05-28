/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  include "GPU_shader.h"
#  include "GPU_shader_shared_utils.h"

typedef struct ViewInfos ViewInfos;
typedef struct ObjectMatrices ObjectMatrices;
typedef struct ObjectInfos ObjectInfos;
typedef struct VolumeInfos VolumeInfos;
typedef struct CurvesInfos CurvesInfos;
#endif

#define DRW_SHADER_SHARED_H

#define DRW_RESOURCE_CHUNK_LEN 512

/* Define the maximum number of grid we allow in a volume UBO. */
#define DRW_GRID_PER_VOLUME_MAX 16

/* Define the maximum number of attribute we allow in a curves UBO.
 * This should be kept in sync with `GPU_ATTR_MAX` */
#define DRW_ATTRIBUTE_PER_CURVES_MAX 15

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

  float2 viewport_size;
  float2 viewport_size_inverse;

  /** Frustum culling data. */
  /** NOTE: vec3 arrays are padded to vec4. */
  float4 frustum_corners[8];
  float4 frustum_planes[6];
};
BLI_STATIC_ASSERT_ALIGN(ViewInfos, 16)

/* Do not override old definitions if the shader uses this header but not shader info. */
#ifdef USE_GPU_SHADER_CREATE_INFO
/* TODO(@fclem): Mass rename. */
#  define ViewProjectionMatrix drw_view.persmat
#  define ViewProjectionMatrixInverse drw_view.persinv
#  define ViewMatrix drw_view.viewmat
#  define ViewMatrixInverse drw_view.viewinv
#  define ProjectionMatrix drw_view.winmat
#  define ProjectionMatrixInverse drw_view.wininv
#  define clipPlanes drw_view.clip_planes
#  define ViewVecs drw_view.viewvecs
#  define CameraTexCoFactors drw_view.viewcamtexcofac
#endif

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

struct VolumeInfos {
  /* Object to grid-space. */
  float4x4 grids_xform[DRW_GRID_PER_VOLUME_MAX];
  /* NOTE: vec4 for alignment. Only float3 needed. */
  float4 color_mul;
  float density_scale;
  float temperature_mul;
  float temperature_bias;
  float _pad;
};
BLI_STATIC_ASSERT_ALIGN(VolumeInfos, 16)

struct CurvesInfos {
  /* Per attribute scope, follows loading order.
   * NOTE: uint as bool in GLSL is 4 bytes. */
  uint is_point_attribute[DRW_ATTRIBUTE_PER_CURVES_MAX];
  int _pad;
};
BLI_STATIC_ASSERT_ALIGN(CurvesInfos, 16)

#define OrcoTexCoFactors (drw_infos[resource_id].drw_OrcoTexCoFactors)
#define ObjectInfo (drw_infos[resource_id].drw_Infos)
#define ObjectColor (drw_infos[resource_id].drw_ObjectColor)
