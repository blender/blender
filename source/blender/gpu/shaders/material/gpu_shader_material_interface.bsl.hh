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

/* Should eventually carry the needed resource tables. */
// struct KernelGlobals {

/* Closure Nodes. */

Closure closure_add(Closure /*cl1*/, Closure /*cl2*/)
{
  return {};
}
Closure closure_mix(Closure /*cl1*/, Closure /*cl2*/, float /*fac*/)
{
  return {};
}
Closure closure_eval(ClosureDiffuse /*diffuse*/)
{
  return {};
}
Closure closure_eval(ClosureSubsurface /*diffuse*/)
{
  return {};
}
Closure closure_eval(ClosureTranslucent /*translucent*/)
{
  return {};
}
Closure closure_eval(ClosureReflection /*reflection*/)
{
  return {};
}
Closure closure_eval(ClosureRefraction /*refraction*/)
{
  return {};
}
Closure closure_eval(ClosureThinRefraction /*refraction*/)
{
  return {};
}
Closure closure_eval(ClosureEmission /*emission*/)
{
  return {};
}
Closure closure_eval(ClosureTransparency /*transparency*/)
{
  return {};
}
Closure closure_eval(ClosureVolumeScatter /*volume_scatter*/)
{
  return {};
}
Closure closure_eval(ClosureVolumeAbsorption /*volume_absorption*/)
{
  return {};
}
Closure closure_eval(ClosureHair /*hair*/)
{
  return {};
}
Closure closure_eval(ClosureReflection /*reflection*/, ClosureRefraction /*refraction*/)
{
  return {};
}
Closure closure_eval(ClosureDiffuse /*diffuse*/, ClosureReflection /*reflection*/)
{
  return {};
}
Closure closure_eval(ClosureReflection /*reflection*/, ClosureReflection /*coat*/)
{
  return {};
}
Closure closure_eval(ClosureVolumeScatter /*volume_scatter*/,
                     ClosureVolumeAbsorption /*volume_absorption*/,
                     ClosureEmission /*emission*/)
{
  return {};
}
Closure closure_eval(ClosureDiffuse /*diffuse*/,
                     ClosureReflection /*reflection*/,
                     ClosureReflection /*coat*/)
{
  return {};
}
Closure closure_eval(ClosureDiffuse /*diffuse*/,
                     ClosureReflection /*reflection*/,
                     ClosureReflection /*coat*/,
                     ClosureRefraction /*refraction*/)
{
  return {};
}

float4 closure_to_rgba(Closure /*closure*/)
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

void brdf_f82_tint_lut(float3 /*F0*/,
                       float3 /*F82*/,
                       float /*cos_theta*/,
                       float /*roughness*/,
                       bool /*do_multiscatter*/,
                       float3 & /*reflectance*/)
{
}

void bsdf_lut(float3 /*F0*/,
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

float2 bsdf_lut(float /*cos_theta*/, float /*roughness*/, float /*ior*/, bool /*do_multiscatter*/)
{
  return float2(0);
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

float3 coordinate_camera(float3 /*P*/)
{
  return float3(0);
}
float3 coordinate_screen(float3 /*P*/)
{
  return float3(0);
}
float3 coordinate_reflect(float3 /*P*/, float3 /*N*/)
{
  return float3(0);
}
float3 coordinate_incoming(float3 /*P*/)
{
  return float3(0);
}

/* Ambient occlusion node. */

float ambient_occlusion_eval(float3 normal,
                             float max_distance,
                             float inverted,
                             float sample_count);

/* Attribute node occlusion node. */

float4 attr_load_color_post(float4 attr);
float attr_load_temperature_post(float attr);
/* TODO remove attr as parameter. */
float4 attr_load_uniform(float4 /*attr*/, uint /*attr_hash*/)
{
  return float4(0);
}
float4 node_attribute_light_impl(int /*light_index*/, uint /*attr_hash*/)
{
  return float4(0);
}
bool node_attribute_light_is_sun_impl(int /*light_index*/)
{
  return false;
}
bool node_attribute_light_is_point_impl(int /*light_index*/)
{
  return false;
}
bool node_attribute_light_is_spot_impl(int /*light_index*/)
{
  return false;
}
bool node_attribute_light_is_area_impl(int /*light_index*/)
{
  return false;
}
float node_attribute_light_cutoff_distance_impl(int /*light_index*/)
{
  return float(0);
}

/* Scene Time Node. */

void scene_time_uniforms(float & /*seconds*/, float & /*frame*/) {}

/* Shadow Raycast Node. */

void node_shadow_raycast_impl(int /*light_index*/,
                              float3 /*position*/,
                              float /*softness*/,
                              float4 & /*color*/)
{
}

/* Light Accumulation Node. */

void node_light_accumulation_impl(int /*light_index*/,
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

void node_light_info_impl(const int /*light_index*/,
                          float4 & /*color*/,
                          float & /*power*/,
                          float3 & /*position*/)
{
}

/* Light Evaluation Node. */

void node_light_evaluation_common_impl(int /*light_index*/,
                                       float3 /*position*/,
                                       float3 & /*direction*/,
                                       float & /*distance*/,
                                       float & /*mask*/)
{
}

template<bool use_diffuse>
void node_light_evaluation_impl(int /*light_index*/,
                                float3 /*position*/,
                                float3 /*normal*/,
                                float /*roughness*/,
                                float & /*factor*/)
{
}

/* Raycast Node. */

void raycast_eval(float3 /*position*/,
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

float texture_lod_bias_get()
{
  return 0.0;
}

float derivative_scale_get()
{
  return 1.0;
}

/* AOV Output. */

void output_aov(int2 /*texel*/,
                float4 /*color*/,
                float /*value*/,
                uint /*hash*/,
                float /*holdout*/,
                eObjectInfoFlag /*ob_flag*/)
{
}

/* Matrices. */

struct ObjectMatrices {
  float4x4 model;
  float4x4 model_inverse;
};

struct ObjectInfos {
  /** Uploaded as center + size. Converted to mul+bias to local coord. */
  packed_float3 orco_add;
  uint object_attrs_offset;
  packed_float3 orco_mul;
  uint object_attrs_len;

  float4 ob_color;
  uint index;
  /** Used for Light Linking in EEVEE */
  uint light_and_shadow_set_membership;
  float random;
  eObjectInfoFlag flag;
  float shadow_terminator_normal_offset;
  float shadow_terminator_geometry_offset;
};

struct ViewMatrices {
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;
};

ObjectMatrices object_matrices_get()
{
  return {};
}

ObjectInfos object_infos_get()
{
  return {};
}

ViewMatrices view_matrices_get()
{
  return {};
}

//}; // KernelGlobals
