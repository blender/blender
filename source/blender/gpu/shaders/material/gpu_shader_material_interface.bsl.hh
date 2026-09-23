/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Engine interface expected from the node tree.
 *
 * The engine must implement all of these functions.
 */

#pragma once

#include "gpu_shader_codegen_lib.glsl"

/* This file must replaced at runtime. The following content is only a possible implementation. */
#pragma runtime_generated

struct Closure {};
#define CLOSURE_DEFAULT \
  { \
  }

enum eObjectInfoFlag : uint32_t {
  OBJECT_SELECTED = (1u << 0u),
  OBJECT_FROM_DUPLI = (1u << 1u),
  OBJECT_FROM_SET = (1u << 2u),
  OBJECT_ACTIVE = (1u << 3u),
  OBJECT_NEGATIVE_SCALE = (1u << 4u),
  OBJECT_HOLDOUT = (1u << 5u),
  /* Implies all objects that match the current active object's mode and able to be edited
   * simultaneously. Currently only applicable for edit mode. */
  OBJECT_ACTIVE_EDIT_MODE = (1u << 6u),
  /* Avoid skipped info to change culling. */
  OBJECT_NO_INFO = ~OBJECT_HOLDOUT
};

#define RAY_TYPE_CAMERA 0
#define RAY_TYPE_SHADOW 1
#define RAY_TYPE_DIFFUSE 2
#define RAY_TYPE_GLOSSY 3

/* Expected members of ViewMatrices. */
struct ViewMatrices {
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;
};

/* Expected members of ObjectMatrices. */
struct ObjectMatrices {
  float4x4 model;
  float4x4 model_inverse;
};

/* Expected members of ObjectInfos. */
struct ObjectInfos {
  float4 ob_color;
  uint index;
  float random;
  eObjectInfoFlag flag;
};

struct ShadingData {
  /** Fragment coordinate. */
  float4 frag_co;
  /** World position. */
  packed_float3 P;
  /** Surface Normal. Normalized, overridden by bump displacement. */
  packed_float3 N;
  /** Raw interpolated normal (non-normalized) data. */
  packed_float3 Ni;
  /** Geometric Normal. */
  packed_float3 Ng;
  /** Curve Tangent Space. */
  packed_float3 curve_T, curve_B, curve_N;
  /** Barycentric coordinates. */
  packed_float2 barycentric_coords;
  packed_float3 barycentric_dists;
  /** Hair thickness in world space. */
  float hair_diameter;
  /** Index of the strand for per strand effects. */
  int hair_strand_id;
  /** Pointcloud infos. */
  packed_float3 point_position;
  float point_radius;
  int point_id;
  /** Ray properties (approximation). */
  float ray_depth;
  float ray_length;
  uchar ray_type;
  /** Is hair. */
  bool is_strand;
  /** Fragment front_facing value or true for other stages. */
  bool front_facing;

  /* TODO(fclem): Supposed to be an implementation detail. */
  float holdout;
};

/* Should eventually carry the needed resource tables. */
struct KernelGlobals {
  /* Add dummy push constant to tag struct as resource table.. */
  [[push_constant]] int dummy_;

  ViewMatrices view_matrices_get(const ShadingData & /*sd*/)
  {
    return {};
  }

  ObjectMatrices object_matrices_get(const ShadingData & /*sd*/)
  {
    return {};
  }

  ObjectInfos object_infos_get(const ShadingData & /*sd*/)
  {
    return {};
  }

  ObjectMatrices light_matrices_get(int /*light_index*/)
  {
    return {};
  }
};

/* Closure Nodes. */

Closure closure_add(Closure /*cl1*/, Closure /*cl2*/)
{
  return {};
}
Closure closure_mix(Closure /*cl1*/, Closure /*cl2*/, float /*fac*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureDiffuse /*diffuse*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureSubsurface /*diffuse*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureTranslucent /*translucent*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureReflection /*reflection*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureRefraction /*refraction*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureThinRefraction /*refraction*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureEmission /*emission*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureTransparency /*transparency*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureVolumeScatter /*volume_scatter*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureVolumeAbsorption /*volume_absorption*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/, ClosureHair /*hair*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/,
                     ClosureReflection /*reflection*/,
                     ClosureRefraction /*refraction*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/,
                     ClosureDiffuse /*diffuse*/,
                     ClosureReflection /*reflection*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/,
                     ClosureReflection /*reflection*/,
                     ClosureReflection /*coat*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/,
                     ClosureVolumeScatter /*volume_scatter*/,
                     ClosureVolumeAbsorption /*volume_absorption*/,
                     ClosureEmission /*emission*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/,
                     ClosureDiffuse /*diffuse*/,
                     ClosureReflection /*reflection*/,
                     ClosureReflection /*coat*/)
{
  return {};
}
Closure closure_eval(ShadingData & /*sd*/,
                     ClosureDiffuse /*diffuse*/,
                     ClosureReflection /*reflection*/,
                     ClosureReflection /*coat*/,
                     ClosureRefraction /*refraction*/)
{
  return {};
}

float4 closure_to_rgba([[resource_table]] KernelGlobals & /*kg*/,
                       ShadingData & /*sd*/,
                       Closure /*closure*/)
{
  return float4(0);
}

template<typename T> float3 F_brdf_single_scatter(float3 /*f0*/, float3 /*f90*/, T /*lut*/)
{
  return float3(0);
}

template<typename T> float3 F_brdf_multi_scatter(float3 /*f0*/, float3 /*f90*/, T /*lut*/)
{
  return float3(0);
}

void brdf_f82_tint_lut([[resource_table]] const KernelGlobals & /*kg*/,
                       float3 /*F0*/,
                       float3 /*F82*/,
                       float /*cos_theta*/,
                       float /*roughness*/,
                       bool /*do_multiscatter*/,
                       float3 & /*reflectance*/)
{
}

void bsdf_lut([[resource_table]] const KernelGlobals & /*kg*/,
              float3 /*F0*/,
              float3 /*F90*/,
              float3 /*transmission_tint*/,
              float /*cos_theta*/,
              float /*roughness*/,
              float /*ior*/,
              bool /*do_multiscatter*/,
              float3 & /*reflectance*/,
              float3 & /*transmittance*/)
{
}

float2 bsdf_lut([[resource_table]] const KernelGlobals & /*kg*/,
                float /*cos_theta*/,
                float /*roughness*/,
                float /*ior*/,
                bool /*do_multiscatter*/)
{
  return float2(0);
}

float3 brdf_lut([[resource_table]] KernelGlobals & /*kg*/,
                float3 /*F0*/,
                float3 /*F90*/,
                float /*cos_theta*/,
                float /*roughness*/,
                bool /*do_multiscatter*/)
{
  return float3(0.0f);
}

float f0_from_ior(float /*eta*/)
{
  return 0.0f;
}

float F0_from_ior(float /*eta*/)
{
  return 0.0f;
}

/* Coordinates. */

struct Coordinates {
  float3 camera;
  float3 screen;
  float3 reflect;
  float3 incoming;
};

Coordinates coordinate_impl([[resource_table]] KernelGlobals & /*kg*/,
                            const ShadingData & /*sd*/,
                            float3 /*P*/,
                            float3 /*N*/)
{
  return {};
}

/* Ambient occlusion node. */

float ambient_occlusion_eval([[resource_table]] const KernelGlobals & /*kg*/,
                             const ShadingData & /*sd*/,
                             float3 /*normal*/,
                             float /*max_distance*/,
                             float /*inverted*/,
                             float /*sample_count*/)
{
  return 0.0f;
}

/* Attribute node occlusion node. */

float4 attr_load_color_post(float4 attr)
{
  return attr;
}
float attr_load_temperature_post(float attr)
{
  return attr;
}
float4 attr_load_radiance_post(float4 attr)
{
  return attr;
}

/* TODO remove attr as parameter. */
float4 attr_load_uniform([[resource_table]] KernelGlobals & /*kg*/,
                         const ShadingData & /*sd*/,
                         float4 /*attr*/,
                         uint /*attr_hash*/)
{
  return float4(0);
}
float4 node_attribute_layer_impl([[resource_table]] KernelGlobals & /*kg*/,
                                 const uint /*attr_hash*/)
{
  return float4(0);
}
float4 node_attribute_light_impl([[resource_table]] KernelGlobals & /*kg*/,
                                 int /*light_index*/,
                                 uint /*attr_hash*/)
{
  return float4(0);
}
bool node_attribute_light_is_sun_impl([[resource_table]] KernelGlobals & /*kg*/,
                                      int /*light_index*/)
{
  return false;
}
bool node_attribute_light_is_point_impl([[resource_table]] KernelGlobals & /*kg*/,
                                        int /*light_index*/)
{
  return false;
}
bool node_attribute_light_is_spot_impl([[resource_table]] KernelGlobals & /*kg*/,
                                       int /*light_index*/)
{
  return false;
}
bool node_attribute_light_is_area_impl([[resource_table]] KernelGlobals & /*kg*/,
                                       int /*light_index*/)
{
  return false;
}
float node_attribute_light_cutoff_distance_impl([[resource_table]] KernelGlobals & /*kg*/,
                                                int /*light_index*/)
{
  return float(0);
}

/* Scene Time Node. */

void scene_time_uniforms([[resource_table]] KernelGlobals & /*kg*/,
                         float & /*seconds*/,
                         float & /*frame*/)
{
}

/* Shadow Raycast Node. */

void node_shadow_raycast_impl([[resource_table]] KernelGlobals & /*kg*/,
                              const ShadingData & /*sd*/,
                              int /*light_index*/,
                              float3 /*position*/,
                              float /*softness*/,
                              float4 & /*color*/)
{
}

/* Light Accumulation Node. */

void node_light_accumulation_impl([[resource_table]] KernelGlobals & /*kg*/,
                                  ShadingData & /*sd*/,
                                  int /*light_index*/,
                                  float3 /*diffuse_light*/,
                                  float3 /*diffuse_color*/,
                                  float3 /*glossy_light*/,
                                  float3 /*glossy_color*/,
                                  float3 /*transmission_light*/,
                                  float3 /*transmission_color*/,
                                  float /*weight*/,
                                  Closure & /*result*/)
{
}

/* Light Info Node. */

void node_light_info_impl([[resource_table]] KernelGlobals & /*kg*/,
                          const int /*light_index*/,
                          float4 & /*color*/,
                          float & /*power*/,
                          float3 & /*position*/)
{
}

/* Light Evaluation Node. */

void node_light_evaluation_common_impl([[resource_table]] KernelGlobals & /*kg*/,
                                       int /*light_index*/,
                                       float3 /*position*/,
                                       float3 & /*direction*/,
                                       float & /*distance*/,
                                       float & /*mask*/)
{
}

template<bool use_diffuse>
void node_light_evaluation_impl([[resource_table]] KernelGlobals & /*kg*/,
                                const ShadingData & /*sd*/,
                                int /*light_index*/,
                                float3 /*position*/,
                                float3 /*normal*/,
                                float /*roughness*/,
                                float & /*factor*/)
{
}

template void node_light_evaluation_impl<true>(
    KernelGlobals &, const ShadingData &, int, float3, float3, float, float &);
template void node_light_evaluation_impl<false>(
    KernelGlobals &, const ShadingData &, int, float3, float3, float, float &);

/* Raycast Node. */

void raycast_eval([[resource_table]] KernelGlobals & /*kg*/,
                  const ShadingData & /*sd*/,
                  float3 /*position*/,
                  float3 /*direction*/,
                  float /*max_distance*/,
                  bool /*self_only*/,
                  bool & /*is_hit*/,
                  bool & /*self_hit*/,
                  float & /*hit_distance*/,
                  float3 & /*hit_position*/,
                  float3 & /*hit_normal*/)
{
}

/* Image Texture Node. */

float texture_lod_bias_get([[resource_table]] KernelGlobals & /*kg*/)
{
  return 0.0;
}

float derivative_scale_get([[resource_table]] KernelGlobals & /*kg*/)
{
  return 1.0;
}

/* AOV Output. */

void output_aov([[resource_table]] KernelGlobals & /* kg */,
                int2 /*texel*/,
                float4 /*color*/,
                float /*value*/,
                uint /*hash*/,
                float /*holdout*/,
                eObjectInfoFlag /*ob_flag*/)
{
}

/* Matrices. */
