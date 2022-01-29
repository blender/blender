
<<<<<<< HEAD
=======
<<<<<<<< HEAD:source/blender/draw/engines/workbench/workbench_shader_shared.h
>>>>>>> master
#ifndef GPU_SHADER
#  include "gpu_shader_shared_utils.h"
#endif

#define WORKBENCH_SHADER_SHARED_H

<<<<<<< HEAD
=======
========
#ifndef WORKBENCH_SHADER_SHARED_H
>>>>>>>> master:source/blender/draw/engines/workbench/shaders/workbench_data_lib.glsl
>>>>>>> master
struct LightData {
  float4 direction;
  float4 specular_color;
  float4 diffuse_color_wrap; /* rgb: diffuse col a: wrapped lighting factor */
};

struct WorldData {
  float4 viewport_size;
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
  int _pad1;
  int _pad2;
};

<<<<<<< HEAD
#define viewport_size_inv viewport_size.zw
#define packed_rough_metal roughness
=======
<<<<<<<< HEAD:source/blender/draw/engines/workbench/workbench_shader_shared.h
#define viewport_size_inv viewport_size.zw
========
#  define viewport_size_inv viewport_size.zw

layout(std140) uniform world_block
{
  WorldData world_data;
};
#endif
>>>>>>>> master:source/blender/draw/engines/workbench/shaders/workbench_data_lib.glsl
>>>>>>> master
