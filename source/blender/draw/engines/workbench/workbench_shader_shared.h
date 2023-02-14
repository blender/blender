/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.h"
#endif

#define WORKBENCH_SHADER_SHARED_H

struct LightData {
  float4 direction;
  float4 specular_color;
  float4 diffuse_color_wrap; /* rgb: diffuse col a: wrapped lighting factor */
};

struct WorldData {
  float2 viewport_size;
  float2 viewport_size_inv;
  float4 object_outline_color;
  float4 shadow_direction_vs;
  float shadow_focus;
  float shadow_shift;
  float shadow_mul;
  float shadow_add;
  /* - 16 bytes alignment - */
  LightData lights[4];
  float4 ambient_color;

  int cavity_sample_start;
  int cavity_sample_end;
  float cavity_sample_count_inv;
  float cavity_jitter_scale;

  float cavity_valley_factor;
  float cavity_ridge_factor;
  float cavity_attenuation;
  float cavity_distance;

  float curvature_ridge;
  float curvature_valley;
  float ui_scale;
  float _pad0;

  int matcap_orientation;
  bool use_specular;
  float xray_alpha;
  int _pad1;

  float4 background_color;
};

struct ExtrudedFrustum {
  /** \note vec3 array padded to vec4. */
  float4 corners[16];
  float4 planes[12];
  int corners_count;
  int planes_count;
  int _padding[2];
};

struct ShadowPassData {
  float4 far_plane;
  packed_float3 light_direction_ws;
  int _padding;
};
