/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * G-buffer: Packing and unpacking of G-buffer data.
 *
 * See #GBuffer for a breakdown of the G-buffer layout.
 *
 * There is two way of indexing closure data from the GBuffer:
 * - per "bin": same closure indices as during the material evaluation pass.
 *              Can have none-closures.
 * - per "layer": gbuffer internal storage order. Tightly packed, will only have none-closures at
 *                the end of the array.
 *
 * Indexing per bin is better to avoid parameter discontinuity for a given closure
 * (i.e: for denoising), whereas indexing per layer is better for iterating through the closure
 * without dealing with none-closures.
 */

#include "infos/eevee_common_info.hh"

#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Types
 *
 * \{ */

/* NOTE: Only specialized for the gbuffer pass. */
#ifndef GBUFFER_LAYER_MAX
#  define GBUFFER_LAYER_MAX 3
#endif
#define GBUFFER_NORMAL_MAX (GBUFFER_LAYER_MAX + /* Additional data */ 1)
#define GBUFFER_DATA_MAX (GBUFFER_LAYER_MAX * 2)
#define GBUFFER_HEADER_BITS_PER_LAYER 4
/* NOTE: Reserve the last 4 bits for the normal layers ids. */
#define GBUFFER_NORMAL_BITS_SHIFT 12u

struct GBufferData {
  ClosureUndetermined closure[GBUFFER_LAYER_MAX];
  /* True if surface uses a dedicated object id layer. Should only be turned on if needed. */
  bool use_object_id;
  /* Additional object information if any closure needs it. */
  float thickness;
  /* First world normal stored in the gbuffer. Only valid if `has_any_surface` is true. */
  packed_float3 surface_N;
};

/* Result of Packing the GBuffer. */
struct GBufferWriter {
  /* Packed GBuffer data in layer indexing. */
  float4 data[GBUFFER_DATA_MAX];
  /* Packed normal data. Redundant normals are omitted. */
  float2 N[GBUFFER_NORMAL_MAX];
  /* Header containing which closures are encoded and which normals are used. */
  uint header;
  /** Only used for book-keeping. Not actually written. Can be derived from header. */
  /* Number of bins written in the header. Counts empty bins. */
  uchar bins_len;
  /* Number of data written in the data array. */
  uchar data_len;
  /* Number of normal written in the normal array. */
  uchar normal_len;
  /* First world normal stored in the gbuffer (uncompressed). */
  packed_float3 surface_N;
};

/* Result of loading the GBuffer. */
struct GBufferReader {
  ClosureUndetermined closures[GBUFFER_LAYER_MAX];
  /* Texel of the gbuffer being read. */
  int2 texel;

  uint header;

  /* First world normal stored in the gbuffer. Only valid if `has_any_surface` is true. */
  packed_float3 surface_N;
  /* Additional object information if any closure needs it. */
  float thickness;
  /* Number of valid closure encoded in the gbuffer. */
  uchar closure_count;
  /* Only used for book-keeping when reading. */
  uchar data_len;
  /* Only used for debugging and testing. */
  uchar normal_len;
};

ClosureType gbuffer_mode_to_closure_type(uint mode)
{
  switch (mode) {
    case GBUF_DIFFUSE:
      return ClosureType(CLOSURE_BSDF_DIFFUSE_ID);
    case GBUF_TRANSLUCENT:
      return ClosureType(CLOSURE_BSDF_TRANSLUCENT_ID);
    case GBUF_SUBSURFACE:
      return ClosureType(CLOSURE_BSSRDF_BURLEY_ID);
    case GBUF_REFLECTION_COLORLESS:
    case GBUF_REFLECTION:
      return ClosureType(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    case GBUF_REFRACTION_COLORLESS:
    case GBUF_REFRACTION:
      return ClosureType(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    default:
      return ClosureType(CLOSURE_NONE_ID);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load / Store macros
 *
 * This allows for writing unit tests that read and write during the same shader invocation.
 * \{ */

#if defined(GBUFFER_LOAD) || defined(GLSL_CPP_STUBS)
/* Read only shader. Use correct types and functions. */
#  define samplerGBufferHeader usampler2DArray
#  define samplerGBufferClosure sampler2DArray
#  define samplerGBufferNormal sampler2DArray

uint fetchGBuffer(usampler2DArray tx, int2 texel, uchar layer)
{
  return texelFetch(tx, int3(texel, layer), 0).r;
}
float4 fetchGBuffer(sampler2DArray tx, int2 texel, uchar layer)
{
  return texelFetch(tx, int3(texel, layer), 0);
}

#else
#  define samplerGBufferHeader int
#  define samplerGBufferClosure uint
#  define samplerGBufferNormal float

#  ifdef GBUFFER_WRITE
/* Write only shader. Use dummy load functions. */
uint fetchGBuffer(samplerGBufferHeader tx, int2 texel, uchar layer)
{
  return uint(0);
}
float4 fetchGBuffer(samplerGBufferClosure tx, int2 texel, uchar layer)
{
  return float4(0.0f);
}
float4 fetchGBuffer(samplerGBufferNormal tx, int2 texel, uchar layer)
{
  return float4(0.0f);
}

#  else
/* Unit testing setup. Allow read and write in the same shader. */
GBufferWriter g_data_packed;

uint fetchGBuffer(samplerGBufferHeader tx, int2 texel, uchar layer)
{
  return g_data_packed.header;
}
float4 fetchGBuffer(samplerGBufferClosure tx, int2 texel, uchar layer)
{
  return g_data_packed.data[layer];
}
float4 fetchGBuffer(samplerGBufferNormal tx, int2 texel, uchar layer)
{
  return g_data_packed.N[layer].xyyy;
}

#  endif
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack / Unpack Utils
 *
 * \{ */

bool color_is_grayscale(float3 color)
{
  /* This tests is R == G == B. */
  return all(equal(color.rgb, color.gbr));
}

float2 gbuffer_normal_pack(float3 N)
{
  N /= length_manhattan(N);
  float2 _sign = sign(N.xy);
  _sign.x = _sign.x == 0.0f ? 1.0f : _sign.x;
  _sign.y = _sign.y == 0.0f ? 1.0f : _sign.y;
  N.xy = (N.z >= 0.0f) ? N.xy : ((1.0f - abs(N.yx)) * _sign);
  N.xy = N.xy * 0.5f + 0.5f;
  return N.xy;
}

float3 gbuffer_normal_unpack(float2 N_packed)
{
  N_packed = N_packed * 2.0f - 1.0f;
  float3 N = float3(N_packed.x, N_packed.y, 1.0f - abs(N_packed.x) - abs(N_packed.y));
  float t = clamp(-N.z, 0.0f, 1.0f);
  N.x += (N.x >= 0.0f) ? -t : t;
  N.y += (N.y >= 0.0f) ? -t : t;
  return normalize(N);
}

float gbuffer_ior_pack(float ior)
{
  return (ior > 1.0f) ? (1.0f - 0.5f / ior) : (0.5f * ior);
}

float gbuffer_ior_unpack(float ior_packed)
{
  return (ior_packed > 0.5f) ? (0.5f / (1.0f - ior_packed)) : (2.0f * ior_packed);
}

float gbuffer_thickness_pack(float thickness)
{
  /* TODO(fclem): If needed, we could increase precision by defining a ceiling value like the view
   * distance and remap to it. Or tweak the hyperbole equality. */
  /* NOTE: Sign encodes the thickness mode. */
  /* Remap [0..+inf) to [0..1/2]. */
  float thickness_packed = abs(thickness) / (1.0f + 2.0f * abs(thickness));
  /* Mirror the negative from [0..1/2] to [1..1/2]. O is mapped to 0 for precision. */
  return (thickness < 0.0f) ? 1.0f - thickness_packed : thickness_packed;
}
float gbuffer_thickness_unpack(float thickness_packed)
{
  /* Undo mirroring. */
  float thickness = (thickness_packed > 0.5f) ? 1.0f - thickness_packed : thickness_packed;
  /* Remap [0..1/2] to [0..+inf). */
  thickness = thickness / (1.0f - 2.0f * thickness);
  /* Retrieve sign. */
  return (thickness_packed > 0.5f) ? -thickness : thickness;
}

/**
 * Pack color with values in the range of [0..8] using a 2 bit shared exponent.
 * This allows values up to 8 with some color degradation.
 * Above 8, the result will be clamped when writing the data to the output buffer.
 * This is supposed to be stored in a 10_10_10_2_unorm format with exponent in alpha.
 */
float4 gbuffer_closure_color_pack(float3 color)
{
  float max_comp = max(color.x, max(color.y, color.z));
  float exponent = (max_comp > 1) ? ((max_comp > 2) ? ((max_comp > 4) ? 3.0f : 2.0f) : 1.0f) :
                                    0.0f;
  /* TODO(fclem): Could try dithering to avoid banding artifacts on higher exponents. */
  return float4(color / exp2(exponent), exponent / 3.0f);
}
float3 gbuffer_closure_color_unpack(float4 color_packed)
{
  float exponent = color_packed.a * 3.0f;
  return color_packed.rgb * exp2(exponent);
}

float4 gbuffer_sss_radii_pack(float3 sss_radii)
{
  /* TODO(fclem): Something better. */
  return gbuffer_closure_color_pack(float3(gbuffer_ior_pack(sss_radii.x),
                                           gbuffer_ior_pack(sss_radii.y),
                                           gbuffer_ior_pack(sss_radii.z)));
}
float3 gbuffer_sss_radii_unpack(float4 sss_radii_packed)
{
  /* TODO(fclem): Something better. */
  float3 radii_packed = gbuffer_closure_color_unpack(sss_radii_packed);
  return float3(gbuffer_ior_unpack(radii_packed.x),
                gbuffer_ior_unpack(radii_packed.y),
                gbuffer_ior_unpack(radii_packed.z));
}

/**
 * Pack value in the range of [0..8] using a 2 bit exponent.
 * This allows values up to 8 with some color degradation.
 * Above 8, the result will be clamped when writing the data to the output buffer.
 * This is supposed to be stored in a 10_10_10_2_unorm format with exponent in alpha.
 */
float2 gbuffer_closure_intensity_pack(float value)
{
  float exponent = (value > 1) ? ((value > 2) ? ((value > 4) ? 3.0f : 2.0f) : 1.0f) : 0.0f;
  /* TODO(fclem): Could try dithering to avoid banding artifacts on higher exponents. */
  return float2(value / exp2(exponent), exponent / 3.0f);
}
float gbuffer_closure_intensity_unpack(float2 value_packed)
{
  float exponent = value_packed.g * 3.0f;
  return value_packed.r * exp2(exponent);
}

float gbuffer_object_id_f16_pack(uint object_id)
{
  /* TODO(fclem): Make use of all the 16 bits in a half float.
   * This here only correctly represent values up to 1024. */
  return float(object_id);
}

uint gbuffer_object_id_f16_unpack(float object_id_packed)
{
  return uint(object_id_packed);
}

bool gbuffer_is_refraction(float4 gbuffer)
{
  return gbuffer.w < 1.0f;
}

/* Quantize geometric normal to 6 bits. */
uint gbuffer_geometry_normal_pack(float3 Ng, float3 N)
{
  /* This is a threshold that minimizes the error over the sphere. */
  constexpr float quantization_multiplier = 1.360f;
  /* Normalize for comparison. */
  float3 Ng_quantize = normalize(round(quantization_multiplier * Ng));
  /* Note: Comparing the error using cosines. The greater the cosine value, the lower the error. */
  if (dot(N, Ng) > dot(Ng, Ng_quantize)) {
    /* If the error between the default shading normal and the geometric normal is smaller than the
     * compression error, we do not encode the geometric normal and use the shading normal for
     * biasing the shadow rays. This avoid precision issues that comes with the quantization. */
    return 0;
  }
  uint data;
  data = (Ng_quantize.x > 0.0f) ? (1u << 0u) : 0u;
  data |= (Ng_quantize.y > 0.0f) ? (1u << 1u) : 0u;
  data |= (Ng_quantize.z > 0.0f) ? (1u << 2u) : 0u;
  data |= (Ng_quantize.x < 0.0f) ? (1u << 3u) : 0u;
  data |= (Ng_quantize.y < 0.0f) ? (1u << 4u) : 0u;
  data |= (Ng_quantize.z < 0.0f) ? (1u << 5u) : 0u;
  return data << 20u;
}

float3 gbuffer_geometry_normal_unpack(uint data, float3 N)
{
  /* If data is 0 it means the shading normal is representative enough. */
  if ((data & (63u << 20u)) == 0u) {
    return N;
  }
  float3 Ng = float3((uint3(data) >> (uint3(0, 1, 2) + 20u)) & 1u) -
              float3((uint3(data) >> (uint3(3, 4, 5) + 20u)) & 1u);
  return normalize(Ng);
}

/* Light Linking flag. */
uint gbuffer_use_object_id_pack(bool use_object_id)
{
  return int(use_object_id) << 31u;
}

bool gbuffer_use_object_id_unpack(uint header)
{
  return flag_test(header, 1u << 31u);
}

uint gbuffer_header_pack(GBufferMode mode, uint bin)
{
  return (mode << (4u * bin));
}

GBufferMode gbuffer_header_unpack(uint data, uint bin)
{
  return GBufferMode((data >> (4u * bin)) & 15u);
}

void gbuffer_append_closure(inout GBufferWriter gbuf, GBufferMode closure_type)
{
  gbuf.header |= gbuffer_header_pack(closure_type, gbuf.bins_len);
  gbuf.bins_len++;
}
void gbuffer_register_closure(inout GBufferReader gbuf, ClosureUndetermined cl, uchar slot)
{
  switch (slot) {
#if GBUFFER_LAYER_MAX > 0
    case 0:
      gbuf.closures[0] = cl;
      break;
#endif
#if GBUFFER_LAYER_MAX > 1
    case 1:
      gbuf.closures[1] = cl;
      break;
#endif
#if GBUFFER_LAYER_MAX > 2
    case 2:
      gbuf.closures[2] = cl;
      break;
#endif
  }
}
void gbuffer_skip_closure(inout GBufferReader gbuf)
{
  gbuf.closure_count++;
}

ClosureUndetermined gbuffer_closure_get(GBufferReader gbuf, uchar i)
{
  switch (i) {
#if GBUFFER_LAYER_MAX > 0
    case 0:
      return gbuf.closures[0];
#endif
#if GBUFFER_LAYER_MAX > 1
    case 1:
      return gbuf.closures[1];
#endif
#if GBUFFER_LAYER_MAX > 2
    case 2:
      return gbuf.closures[2];
#endif
    default:
      return closure_new(CLOSURE_NONE_ID);
  }
}

void gbuffer_append_data(inout GBufferWriter gbuf, float4 data)
{
  switch (gbuf.data_len) {
#if GBUFFER_DATA_MAX > 0
    case 0:
      gbuf.data[0] = data;
      break;
#endif
#if GBUFFER_DATA_MAX > 1
    case 1:
      gbuf.data[1] = data;
      break;
#endif
#if GBUFFER_DATA_MAX > 2
    case 2:
      gbuf.data[2] = data;
      break;
#endif
#if GBUFFER_DATA_MAX > 3
    case 3:
      gbuf.data[3] = data;
      break;
#endif
#if GBUFFER_DATA_MAX > 4
    case 4:
      gbuf.data[4] = data;
      break;
#endif
#if GBUFFER_DATA_MAX > 5
    case 5:
      gbuf.data[5] = data;
      break;
#endif
  }
  gbuf.data_len++;
}
float4 gbuffer_pop_first_data(inout GBufferReader gbuf, samplerGBufferClosure closure_tx)
{
  float4 data = fetchGBuffer(closure_tx, gbuf.texel, gbuf.data_len);
  gbuf.data_len++;
  return data;
}
void gbuffer_skip_data(inout GBufferReader gbuf)
{
  gbuf.data_len++;
}

/**
 * Set the dedicated normal bit for the last added closure.
 * Expects `bin_id` to be in [0..2].
 * Expects `normal_id` to be in [0..3].
 */
void gbuffer_header_normal_layer_id_set(inout uint header, uint bin_id, uint normal_id)
{
  /* Layer 0 will always have normal id 0. It doesn't have to be encoded. Skip it. */
  if (bin_id == 0u) {
    return;
  }
  /* -2 is to skip the bin_id 0 and start encoding for bin_id 1. This keeps the FMA. */
  header |= normal_id << ((GBUFFER_NORMAL_BITS_SHIFT - 2u) + bin_id * 2u);
}
uint gbuffer_header_normal_layer_id_get(uint header, uint bin_id)
{
  /* Layer 0 will always have normal id 0. */
  if (bin_id == 0u) {
    return 0u;
  }
  /* -2 is to skip the bin_id 0 and start encoding for bin_id 1. This keeps the FMA. */
  return (3u & (header >> ((GBUFFER_NORMAL_BITS_SHIFT - 2u) + bin_id * 2u)));
}

void gbuffer_append_normal(inout GBufferWriter gbuf, float3 normal)
{
  float2 packed_N = gbuffer_normal_pack(normal);
  /* Assumes this function is called after gbuffer_append_closure. */
  uint layer_id = gbuf.bins_len - 1u;
  /* Try to reuse previous normals. */
#if GBUFFER_NORMAL_MAX > 1
  if (gbuf.normal_len > 0 && all(equal(gbuf.N[0], packed_N))) {
    gbuffer_header_normal_layer_id_set(gbuf.header, layer_id, 0u);
    return;
  }
#endif
#if GBUFFER_NORMAL_MAX > 2
  if (gbuf.normal_len > 1 && all(equal(gbuf.N[1], packed_N))) {
    gbuffer_header_normal_layer_id_set(gbuf.header, layer_id, 1u);
    return;
  }
#endif
#if GBUFFER_NORMAL_MAX > 3
  if (gbuf.normal_len > 2 && all(equal(gbuf.N[2], packed_N))) {
    gbuffer_header_normal_layer_id_set(gbuf.header, layer_id, 2u);
    return;
  }
#endif
  /* Could not reuse. Add another normal. */
  gbuffer_header_normal_layer_id_set(gbuf.header, layer_id, uint(gbuf.normal_len));

  switch (gbuf.normal_len) {
#if GBUFFER_NORMAL_MAX > 0
    case 0:
      gbuf.surface_N = normal;
      gbuf.N[0] = packed_N;
      break;
#endif
#if GBUFFER_NORMAL_MAX > 1
    case 1:
      gbuf.N[1] = packed_N;
      break;
#endif
#if GBUFFER_NORMAL_MAX > 2
    case 2:
      gbuf.N[2] = packed_N;
      break;
#endif
  }
  gbuf.normal_len++;
}
float3 gbuffer_normal_get(inout GBufferReader gbuf, uint bin_id, samplerGBufferNormal normal_tx)
{
  uint normal_layer_id = gbuffer_header_normal_layer_id_get(gbuf.header, bin_id);
  float2 normal_packed = fetchGBuffer(normal_tx, gbuf.texel, int(normal_layer_id)).rg;
  gbuf.normal_len = max(gbuf.normal_len, uchar(normal_layer_id + 1u));
  return gbuffer_normal_unpack(normal_packed);
}
void gbuffer_skip_normal(inout GBufferReader gbuf)
{
  /* Nothing to do. Normals are indexed. */
}

/* Pack geometry additional infos onto the normal stack. Needs to be run last. */
void gbuffer_additional_info_pack(inout GBufferWriter gbuf, float thickness)
{
  gbuf.N[gbuf.normal_len] = float2(gbuffer_thickness_pack(thickness), 0.0f /* UNUSED */);
  gbuf.normal_len++;
}
void gbuffer_additional_info_load(inout GBufferReader gbuf, samplerGBufferNormal normal_tx)
{
  float2 data_packed = fetchGBuffer(normal_tx, gbuf.texel, int(gbuf.normal_len)).rg;
  gbuf.normal_len++;
  gbuf.thickness = gbuffer_thickness_unpack(data_packed.x);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack / Unpack Closures
 *
 * \{ */

/** Outputting dummy closure is required for correct render passes in case of unlit materials. */
void gbuffer_closure_unlit_pack(inout GBufferWriter gbuf, float3 N)
{
  gbuffer_append_closure(gbuf, GBUF_UNLIT);
  gbuffer_append_data(gbuf, float4(0.0f));
  gbuffer_append_normal(gbuf, N);
}

void gbuffer_closure_diffuse_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  gbuffer_append_closure(gbuf, GBUF_DIFFUSE);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl.color));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_diffuse_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_diffuse_load(inout GBufferReader gbuf,
                                  uchar layer,
                                  uchar bin_index,
                                  samplerGBufferClosure closure_tx,
                                  samplerGBufferNormal normal_tx)
{
  float4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_DIFFUSE_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.N = gbuffer_normal_get(gbuf, bin_index, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

void gbuffer_closure_translucent_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  gbuffer_append_closure(gbuf, GBUF_TRANSLUCENT);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl.color));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_translucent_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_translucent_load(inout GBufferReader gbuf,
                                      uchar layer,
                                      uchar bin_index,
                                      samplerGBufferClosure closure_tx,
                                      samplerGBufferNormal normal_tx)
{
  float4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_TRANSLUCENT_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.N = gbuffer_normal_get(gbuf, bin_index, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

void gbuffer_closure_subsurface_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  gbuffer_append_closure(gbuf, GBUF_SUBSURFACE);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl.color));
  gbuffer_append_data(gbuf, gbuffer_sss_radii_pack(cl.data.xyz));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_subsurface_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_subsurface_load(inout GBufferReader gbuf,
                                     uchar layer,
                                     uchar bin_index,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx)
{
  float4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  float4 data1 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSSRDF_BURLEY_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.data.rgb = gbuffer_sss_radii_unpack(data1);
  cl.N = gbuffer_normal_get(gbuf, bin_index, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

void gbuffer_closure_reflection_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  gbuffer_append_closure(gbuf, GBUF_REFLECTION);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl.color));
  gbuffer_append_data(gbuf, float4(cl.data.x, 0.0f, 0.0f, 0.0f));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_reflection_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_reflection_load(inout GBufferReader gbuf,
                                     uchar layer,
                                     uchar bin_index,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx)
{
  float4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  float4 data1 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.data.x = data1.x;
  cl.N = gbuffer_normal_get(gbuf, bin_index, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

void gbuffer_closure_refraction_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  gbuffer_append_closure(gbuf, GBUF_REFRACTION);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl.color));
  gbuffer_append_data(gbuf, float4(cl.data.x, gbuffer_ior_pack(cl.data.y), 0.0f, 0.0f));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_refraction_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_refraction_load(inout GBufferReader gbuf,
                                     uchar layer,
                                     uchar bin_index,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx)
{
  float4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  float4 data1 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.data.x = data1.x;
  cl.data.y = gbuffer_ior_unpack(data1.y);
  cl.N = gbuffer_normal_get(gbuf, bin_index, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack / Unpack Parameter Optimized
 *
 * Special cases where we can save some data layers per closure.
 * \{ */

void gbuffer_closure_reflection_colorless_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  float2 intensity_packed = gbuffer_closure_intensity_pack(cl.color.r);
  gbuffer_append_closure(gbuf, GBUF_REFLECTION_COLORLESS);
  gbuffer_append_data(gbuf, float4(cl.data.x, 0.0f, intensity_packed));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_reflection_colorless_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_reflection_colorless_load(inout GBufferReader gbuf,
                                               uchar layer,
                                               uchar bin_index,
                                               samplerGBufferClosure closure_tx,
                                               samplerGBufferNormal normal_tx)
{
  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);

  float4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  cl.data.x = data0.x;
  cl.color = float3(gbuffer_closure_intensity_unpack(data0.zw));

  cl.N = gbuffer_normal_get(gbuf, bin_index, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

void gbuffer_closure_refraction_colorless_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  float2 intensity_packed = gbuffer_closure_intensity_pack(cl.color.r);
  gbuffer_append_closure(gbuf, GBUF_REFRACTION_COLORLESS);
  gbuffer_append_data(gbuf, float4(cl.data.x, gbuffer_ior_pack(cl.data.y), intensity_packed));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_refraction_colorless_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_refraction_colorless_load(inout GBufferReader gbuf,
                                               uchar layer,
                                               uchar bin_index,
                                               samplerGBufferClosure closure_tx,
                                               samplerGBufferNormal normal_tx)
{
  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);

  float4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  cl.data.x = data0.x;
  cl.data.y = gbuffer_ior_unpack(data0.y);
  cl.color = float3(gbuffer_closure_intensity_unpack(data0.zw));

  cl.N = gbuffer_normal_get(gbuf, bin_index, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack / Unpack Special Common Optimized
 *
 * Special cases where we can save some space by packing multiple closures data together.
 * \{ */

/* Still unused. Have to finalize support.
 * Might be difficult to make it work with #gbuffer_read_bin(). */
#if 0

void gbuffer_closure_metal_clear_coat_pack(inout GBufferWriter gbuf,
                                           ClosureUndetermined cl_bottom,
                                           ClosureUndetermined cl_coat)
{
  float2 intensity_packed = gbuffer_closure_intensity_pack(cl_coat.color.r);
  gbuffer_append_closure(gbuf, GBUF_METAL_CLEARCOAT);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl_bottom.color));
  gbuffer_append_data(gbuf, float4(cl_bottom.data.x, cl_coat.data.x, intensity_packed));
  gbuffer_append_normal(gbuf, cl_bottom.N);
}

void gbuffer_closure_metal_clear_coat_load(inout GBufferReader gbuf,
                                           samplerGBufferClosure closure_tx,
                                           samplerGBufferNormal normal_tx)
{
  float4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  float4 data1 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined bottom = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
  bottom.color = gbuffer_closure_color_unpack(data0);
  bottom.data.x = data1.x;

  ClosureUndetermined coat = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
  coat.color = float3(gbuffer_closure_intensity_unpack(data1.zw));
  coat.data.x = data1.y;

  coat.N = bottom.N = gbuffer_normal_get(gbuf, 0u, normal_tx);

  gbuffer_register_closure(gbuf, bottom, 0);
  gbuffer_register_closure(gbuf, coat, 1);
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gbuffer Read / Write
 *
 * \{ */

GBufferWriter gbuffer_pack(GBufferData data_in, float3 Ng)
{
  GBufferWriter gbuf;
  gbuf.header = 0u;
  gbuf.bins_len = 0;
  gbuf.data_len = 0;
  gbuf.normal_len = 0;

  bool has_additional_data = false;
  for (int i = 0; i < GBUFFER_LAYER_MAX; i++) {
    ClosureUndetermined cl = data_in.closure[i];

    if (cl.weight <= 1e-5f) {
      gbuf.bins_len++;
      continue;
    }

    switch (cl.type) {
      case CLOSURE_BSSRDF_BURLEY_ID:
        gbuffer_closure_subsurface_pack(gbuf, cl);
        has_additional_data = true;
        break;
      case CLOSURE_BSDF_DIFFUSE_ID:
        gbuffer_closure_diffuse_pack(gbuf, cl);
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
        gbuffer_closure_translucent_pack(gbuf, cl);
        has_additional_data = true;
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
        if (color_is_grayscale(cl.color)) {
          gbuffer_closure_reflection_colorless_pack(gbuf, cl);
        }
        else {
          gbuffer_closure_reflection_pack(gbuf, cl);
        }
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        if (color_is_grayscale(cl.color)) {
          gbuffer_closure_refraction_colorless_pack(gbuf, cl);
        }
        else {
          gbuffer_closure_refraction_pack(gbuf, cl);
        }
        has_additional_data = true;
        break;
      default:
        gbuf.bins_len++;
        break;
    }
  }

  if (gbuf.normal_len == 0) {
    /* Reset bin count as no bin was written. */
    gbuf.bins_len = 0;
    gbuffer_closure_unlit_pack(gbuf, data_in.surface_N);
  }

  /* Pack geometric normal into the header if needed. */
  gbuf.header |= gbuffer_geometry_normal_pack(Ng, gbuf.surface_N);
  gbuf.header |= gbuffer_use_object_id_pack(data_in.use_object_id);

  if (has_additional_data) {
    gbuffer_additional_info_pack(gbuf, data_in.thickness);
  }

  return gbuf;
}

/* Return the number of closure as encoded in the given header value. */
int gbuffer_closure_count(uint header)
{
  /* NOTE: Need to be adjusted for different global GBUFFER_LAYER_MAX. */
  constexpr uint bits_per_layer = uint(GBUFFER_HEADER_BITS_PER_LAYER);
  uint3 closure_types = (uint3(header) >> (uint3(0u, 1u, 2u) * bits_per_layer)) &
                        ((1u << bits_per_layer) - 1);
  return reduce_add(int3(not(equal(closure_types, uint3(0u)))));
}

bool gbuffer_has_transmission(uint header)
{
  /* NOTE: Need to be adjusted for different global GBUFFER_LAYER_MAX. */
  constexpr uint bits_per_layer = uint(GBUFFER_HEADER_BITS_PER_LAYER);
  constexpr uint header_mask = (GBUF_TRANSMISSION_BIT << (bits_per_layer * 0)) |
                               (GBUF_TRANSMISSION_BIT << (bits_per_layer * 1)) |
                               (GBUF_TRANSMISSION_BIT << (bits_per_layer * 2));
  return (header & header_mask) != 0;
}

/* Return the number of normal layer as encoded in the given header value. */
int gbuffer_normal_count(uint header)
{
  if (header == 0u) {
    return 0;
  }
  /* Count implicit first layer. */
  uint count = 1u;
  count += uint(((header >> 12u) & 3u) != 0);
  count += uint(((header >> 14u) & 3u) != 0);
  return int(count);
}

/* Return the type of a closure using its bin index. */
ClosureType gbuffer_closure_type_get_by_bin(uint header, uchar bin_index)
{
  /* TODO(fclem): Doesn't take GBUF_METAL_CLEARCOAT into account or other mode that could merge two
   * bins into one layer. */
  constexpr int bits_per_layer = GBUFFER_HEADER_BITS_PER_LAYER;
  uint mode = (header >> (bin_index * bits_per_layer)) & ((1u << bits_per_layer) - 1);
  return gbuffer_mode_to_closure_type(mode);
}

/* Return the bin index of a closure using its layer index. */
uchar gbuffer_closure_get_bin_index(GBufferReader gbuf, uchar layer_index)
{
  uchar layer = 0u;
  for (uchar bin = 0u; bin < GBUFFER_LAYER_MAX; bin++) {
    GBufferMode mode = gbuffer_header_unpack(gbuf.header, bin);
    /* Gbuffer header can have holes. Skip GBUF_NONE. */
    if (mode != GBUF_NONE) {
      if (layer == layer_index) {
        return bin;
      }
      layer++;
    }
  }
  /* Should never happen. But avoid out of bound access. */
  return 0u;
}

ClosureUndetermined gbuffer_closure_get_by_bin(GBufferReader gbuf, uchar bin_index)
{
  int layer_index = 0;
  for (uchar bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
    GBufferMode mode = gbuffer_header_unpack(gbuf.header, bin);
    if (bin == bin_index) {
      return gbuffer_closure_get(gbuf, layer_index);
    }
    else {
      if (mode != GBUF_NONE) {
        layer_index++;
      }
    }
  }
  /* Should never happen. */
  return closure_new(CLOSURE_NONE_ID);
}

/* Read the entirety of the GBuffer. */
GBufferReader gbuffer_read(samplerGBufferHeader header_tx,
                           samplerGBufferClosure closure_tx,
                           samplerGBufferNormal normal_tx,
                           int2 texel)
{
  GBufferReader gbuf;
  gbuf.texel = texel;
  gbuf.thickness = 0.0f;
  gbuf.closure_count = 0;
  gbuf.data_len = 0;
  gbuf.normal_len = 0;
  gbuf.surface_N = float3(0.0f);
  for (uchar bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
    gbuffer_register_closure(gbuf, closure_new(CLOSURE_NONE_ID), bin);
  }

  gbuf.header = fetchGBuffer(header_tx, texel, 0);

  if (gbuf.header == 0u) {
    return gbuf;
  }

  /* First closure is always written. */
  gbuf.surface_N = gbuffer_normal_unpack(fetchGBuffer(normal_tx, texel, 0).xy);

  bool has_additional_data = false;
  for (uchar bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
    GBufferMode mode = gbuffer_header_unpack(gbuf.header, bin);
    switch (mode) {
      default:
      case GBUF_NONE:
        break;
      case GBUF_DIFFUSE:
        gbuffer_closure_diffuse_load(gbuf, gbuf.closure_count, bin, closure_tx, normal_tx);
        gbuf.closure_count++;
        break;
      case GBUF_TRANSLUCENT:
        gbuffer_closure_translucent_load(gbuf, gbuf.closure_count, bin, closure_tx, normal_tx);
        gbuf.closure_count++;
        has_additional_data = true;
        break;
      case GBUF_SUBSURFACE:
        gbuffer_closure_subsurface_load(gbuf, gbuf.closure_count, bin, closure_tx, normal_tx);
        gbuf.closure_count++;
        has_additional_data = true;
        break;
      case GBUF_REFLECTION:
        gbuffer_closure_reflection_load(gbuf, gbuf.closure_count, bin, closure_tx, normal_tx);
        gbuf.closure_count++;
        break;
      case GBUF_REFRACTION:
        gbuffer_closure_refraction_load(gbuf, gbuf.closure_count, bin, closure_tx, normal_tx);
        gbuf.closure_count++;
        has_additional_data = true;
        break;
      case GBUF_REFLECTION_COLORLESS:
        gbuffer_closure_reflection_colorless_load(
            gbuf, gbuf.closure_count, bin, closure_tx, normal_tx);
        gbuf.closure_count++;
        break;
      case GBUF_REFRACTION_COLORLESS:
        gbuffer_closure_refraction_colorless_load(
            gbuf, gbuf.closure_count, bin, closure_tx, normal_tx);
        gbuf.closure_count++;
        has_additional_data = true;
        break;
    }
  }

  if (has_additional_data) {
    gbuffer_additional_info_load(gbuf, normal_tx);
  }

  return gbuf;
}

/* Read only one bin from the GBuffer. */
ClosureUndetermined gbuffer_read_bin(uint header,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx,
                                     int2 texel,
                                     uchar bin_index)
{
  GBufferReader gbuf;
  gbuf.texel = texel;
  gbuf.closure_count = 0;
  gbuf.data_len = 0;
  gbuf.normal_len = 0;
  gbuf.header = header;

  if (gbuf.header == 0u) {
    return closure_new(CLOSURE_NONE_ID);
  }

  GBufferMode mode;
  for (uchar bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
    mode = gbuffer_header_unpack(gbuf.header, bin);

    if (bin >= bin_index) {
      break;
    }

    switch (mode) {
      default:
      case GBUF_NONE:
        break;
      case GBUF_DIFFUSE:
        gbuffer_closure_diffuse_skip(gbuf);
        break;
      case GBUF_TRANSLUCENT:
        gbuffer_closure_translucent_skip(gbuf);
        break;
      case GBUF_SUBSURFACE:
        gbuffer_closure_subsurface_skip(gbuf);
        break;
      case GBUF_REFLECTION:
        gbuffer_closure_reflection_skip(gbuf);
        break;
      case GBUF_REFRACTION:
        gbuffer_closure_refraction_skip(gbuf);
        break;
      case GBUF_REFLECTION_COLORLESS:
        gbuffer_closure_reflection_colorless_skip(gbuf);
        break;
      case GBUF_REFRACTION_COLORLESS:
        gbuffer_closure_refraction_colorless_skip(gbuf);
        break;
    }
  }

  switch (mode) {
    default:
    case GBUF_NONE:
      gbuffer_register_closure(gbuf, closure_new(CLOSURE_NONE_ID), gbuf.closure_count);
      break;
    case GBUF_DIFFUSE:
      gbuffer_closure_diffuse_load(gbuf, gbuf.closure_count, bin_index, closure_tx, normal_tx);
      break;
    case GBUF_TRANSLUCENT:
      gbuffer_closure_translucent_load(gbuf, gbuf.closure_count, bin_index, closure_tx, normal_tx);
      break;
    case GBUF_SUBSURFACE:
      gbuffer_closure_subsurface_load(gbuf, gbuf.closure_count, bin_index, closure_tx, normal_tx);
      break;
    case GBUF_REFLECTION:
      gbuffer_closure_reflection_load(gbuf, gbuf.closure_count, bin_index, closure_tx, normal_tx);
      break;
    case GBUF_REFRACTION:
      gbuffer_closure_refraction_load(gbuf, gbuf.closure_count, bin_index, closure_tx, normal_tx);
      break;
    case GBUF_REFLECTION_COLORLESS:
      gbuffer_closure_reflection_colorless_load(
          gbuf, gbuf.closure_count, bin_index, closure_tx, normal_tx);
      break;
    case GBUF_REFRACTION_COLORLESS:
      gbuffer_closure_refraction_colorless_load(
          gbuf, gbuf.closure_count, bin_index, closure_tx, normal_tx);
      break;
  }

  return gbuffer_closure_get(gbuf, gbuf.closure_count);
}
ClosureUndetermined gbuffer_read_bin(samplerGBufferHeader header_tx,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx,
                                     int2 texel,
                                     uchar bin_index)
{
  return gbuffer_read_bin(
      fetchGBuffer(header_tx, texel, 0), closure_tx, normal_tx, texel, bin_index);
}

/* Load thickness data only if available. Return 0 otherwise. */
float gbuffer_read_thickness(uint header, samplerGBufferNormal normal_tx, int2 texel)
{
  /* WATCH: Assumes all closures needing additional data are in first bin. */
  switch (gbuffer_closure_type_get_by_bin(header, 0)) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_TRANSLUCENT_ID:
    case CLOSURE_BSSRDF_BURLEY_ID: {
      int normal_len = gbuffer_normal_count(header);
      float2 data_packed = fetchGBuffer(normal_tx, texel, normal_len).rg;
      return gbuffer_thickness_unpack(data_packed.x);
    }
    default:
      return 0.0f;
  }
}

/* Returns the first world normal stored in the gbuffer. Assume gbuffer header is non-null. */
float3 gbuffer_read_normal(samplerGBufferNormal normal_tx, int2 texel)
{
  float2 normal_packed = fetchGBuffer(normal_tx, texel, 0).rg;
  return gbuffer_normal_unpack(normal_packed);
}

/** \} */
