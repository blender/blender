/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Types
 *
 * \{ */

/* Note: Only specialized for the gbuffer pass. */
#ifndef GBUFFER_LAYER_MAX
#  define GBUFFER_LAYER_MAX 3
#endif
#define GBUFFER_NORMAL_MAX (GBUFFER_LAYER_MAX + /* Additional data */ 1)
#define GBUFFER_DATA_MAX (GBUFFER_LAYER_MAX * 2)
#define GBUFFER_HEADER_BITS_PER_LAYER 4
/* Note: Reserve the last 4 bits for the normal layers ids. */
#define GBUFFER_NORMAL_BITS_SHIFT 12

struct GBufferData {
  ClosureUndetermined closure[GBUFFER_LAYER_MAX];
  /* Additional object information if any closure needs it. */
  float thickness;
  uint object_id;
  /* First world normal stored in the gbuffer. Only valid if `has_any_surface` is true. */
  vec3 surface_N;
};

/* Result of Packing the GBuffer. */
struct GBufferWriter {
  /* Packed GBuffer data in layer indexing. */
  vec4 data[GBUFFER_DATA_MAX];
  /* Packed normal data. Redundant normals are omitted. */
  vec2 N[GBUFFER_NORMAL_MAX];
  /* Header containing which closures are encoded and which normals are used. */
  uint header;
  /** Only used for book-keeping. Not actually written. Can be derived from header. */
  /* Number of bins written in the header. Counts empty bins. */
  int bins_len;
  /* Number of data written in the data array. */
  int data_len;
  /* Number of normal written in the normal array. */
  int normal_len;
};

/* Result of loading the GBuffer. */
struct GBufferReader {
  ClosureUndetermined closures[GBUFFER_LAYER_MAX];
  /* First world normal stored in the gbuffer. Only valid if `has_any_surface` is true. */
  vec3 surface_N;
  /* Additional object information if any closure needs it. */
  float thickness;
  uint object_id;

  uint header;
  /* Number of valid closure encoded in the gbuffer. */
  int closure_count;
  /* Only used for book-keeping when reading. */
  int data_len;
  /* Only used for debugging and testing. */
  int normal_len;
  /* Texel of the gbuffer being read. */
  ivec2 texel;
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

#ifdef GBUFFER_LOAD
/* Read only shader. Use correct types and functions. */
#  define samplerGBufferHeader usampler2D
#  define samplerGBufferClosure sampler2DArray
#  define samplerGBufferNormal sampler2DArray

uint fetchGBuffer(usampler2D tx, ivec2 texel)
{
  return texelFetch(tx, texel, 0).r;
}
vec4 fetchGBuffer(sampler2DArray tx, ivec2 texel, int layer)
{
  return texelFetch(tx, ivec3(texel, layer), 0);
}

#else
#  define samplerGBufferHeader int
#  define samplerGBufferClosure uint
#  define samplerGBufferNormal float

#  ifdef GBUFFER_WRITE
/* Write only shader. Use dummy load functions. */
uint fetchGBuffer(samplerGBufferHeader tx, ivec2 texel)
{
  return uint(0);
}
vec4 fetchGBuffer(samplerGBufferClosure tx, ivec2 texel, int layer)
{
  return vec4(0.0);
}
vec4 fetchGBuffer(samplerGBufferNormal tx, ivec2 texel, int layer)
{
  return vec4(0.0);
}

#  else
/* Unit testing setup. Allow read and write in the same shader. */
GBufferWriter g_data_packed;

uint fetchGBuffer(samplerGBufferHeader tx, ivec2 texel)
{
  return g_data_packed.header;
}
vec4 fetchGBuffer(samplerGBufferClosure tx, ivec2 texel, int layer)
{
  return g_data_packed.data[layer];
}
vec4 fetchGBuffer(samplerGBufferNormal tx, ivec2 texel, int layer)
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

bool color_is_grayscale(vec3 color)
{
  /* This tests is R == G == B. */
  return all(equal(color.rgb, color.gbr));
}

vec2 gbuffer_normal_pack(vec3 N)
{
  N /= length_manhattan(N);
  vec2 _sign = sign(N.xy);
  _sign.x = _sign.x == 0.0 ? 1.0 : _sign.x;
  _sign.y = _sign.y == 0.0 ? 1.0 : _sign.y;
  N.xy = (N.z >= 0.0) ? N.xy : ((1.0 - abs(N.yx)) * _sign);
  N.xy = N.xy * 0.5 + 0.5;
  return N.xy;
}

vec3 gbuffer_normal_unpack(vec2 N_packed)
{
  N_packed = N_packed * 2.0 - 1.0;
  vec3 N = vec3(N_packed.x, N_packed.y, 1.0 - abs(N_packed.x) - abs(N_packed.y));
  float t = clamp(-N.z, 0.0, 1.0);
  N.x += (N.x >= 0.0) ? -t : t;
  N.y += (N.y >= 0.0) ? -t : t;
  return normalize(N);
}

float gbuffer_ior_pack(float ior)
{
  return (ior > 1.0) ? (1.0 - 0.5 / ior) : (0.5 * ior);
}

float gbuffer_ior_unpack(float ior_packed)
{
  return (ior_packed > 0.5) ? (0.5 / (1.0 - ior_packed)) : (2.0 * ior_packed);
}

float gbuffer_thickness_pack(float thickness)
{
  /* TODO(fclem): Something better. */
  return gbuffer_ior_pack(thickness);
}

float gbuffer_thickness_unpack(float thickness_packed)
{
  /* TODO(fclem): Something better. */
  return gbuffer_ior_unpack(thickness_packed);
}

/**
 * Pack color with values in the range of [0..8] using a 2 bit shared exponent.
 * This allows values up to 8 with some color degradation.
 * Above 8, the result will be clamped when writing the data to the output buffer.
 * This is supposed to be stored in a 10_10_10_2_unorm format with exponent in alpha.
 */
vec4 gbuffer_closure_color_pack(vec3 color)
{
  float max_comp = max(color.x, max(color.y, color.z));
  float exponent = (max_comp > 1) ? ((max_comp > 2) ? ((max_comp > 4) ? 3.0 : 2.0) : 1.0) : 0.0;
  /* TODO(fclem): Could try dithering to avoid banding artifacts on higher exponents. */
  return vec4(color / exp2(exponent), exponent / 3.0);
}
vec3 gbuffer_closure_color_unpack(vec4 color_packed)
{
  float exponent = color_packed.a * 3.0;
  return color_packed.rgb * exp2(exponent);
}

vec4 gbuffer_sss_radii_pack(vec3 sss_radii)
{
  /* TODO(fclem): Something better. */
  return gbuffer_closure_color_pack(vec3(gbuffer_ior_pack(sss_radii.x),
                                         gbuffer_ior_pack(sss_radii.y),
                                         gbuffer_ior_pack(sss_radii.z)));
}
vec3 gbuffer_sss_radii_unpack(vec4 sss_radii_packed)
{
  /* TODO(fclem): Something better. */
  vec3 radii_packed = gbuffer_closure_color_unpack(sss_radii_packed);
  return vec3(gbuffer_ior_unpack(radii_packed.x),
              gbuffer_ior_unpack(radii_packed.y),
              gbuffer_ior_unpack(radii_packed.z));
}

/**
 * Pack value in the range of [0..8] using a 2 bit exponent.
 * This allows values up to 8 with some color degradation.
 * Above 8, the result will be clamped when writing the data to the output buffer.
 * This is supposed to be stored in a 10_10_10_2_unorm format with exponent in alpha.
 */
vec2 gbuffer_closure_intensity_pack(float value)
{
  float exponent = (value > 1) ? ((value > 2) ? ((value > 4) ? 3.0 : 2.0) : 1.0) : 0.0;
  /* TODO(fclem): Could try dithering to avoid banding artifacts on higher exponents. */
  return vec2(value / exp2(exponent), exponent / 3.0);
}
float gbuffer_closure_intensity_unpack(vec2 value_packed)
{
  float exponent = value_packed.g * 3.0;
  return value_packed.r * exp2(exponent);
}

float gbuffer_object_id_unorm16_pack(uint object_id)
{
  return float(object_id & 0xFFFFu) / float(0xFFFF);
}

uint gbuffer_object_id_unorm16_unpack(float object_id_packed)
{
  return uint(object_id_packed * float(0xFFFF));
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

bool gbuffer_is_refraction(vec4 gbuffer)
{
  return gbuffer.w < 1.0;
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
void gbuffer_register_closure(inout GBufferReader gbuf, ClosureUndetermined cl, int slot)
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

ClosureUndetermined gbuffer_closure_get(GBufferReader gbuf, int i)
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

void gbuffer_append_data(inout GBufferWriter gbuf, vec4 data)
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
vec4 gbuffer_pop_first_data(inout GBufferReader gbuf, samplerGBufferClosure closure_tx)
{
  vec4 data = fetchGBuffer(closure_tx, gbuf.texel, gbuf.data_len);
  gbuf.data_len++;
  return data;
}
void gbuffer_skip_data(inout GBufferReader gbuf)
{
  gbuf.data_len++;
}

/**
 * Set the dedicated normal bit for the last added closure.
 * Expects `layer_id` to be in [0..2].
 * Expects `normal_id` to be in [0..3].
 */
void gbuffer_header_normal_layer_id_set(inout uint header, int layer_id, uint normal_id)
{
  /* Layer 0 will always have normal id 0. It doesn't have to be encoded. Skip it. */
  if (layer_id == 0) {
    return;
  }
  /* -2 is to skip the layer_id 0 and start encoding for layer_id 1. This keeps the FMA. */
  header |= normal_id << ((GBUFFER_NORMAL_BITS_SHIFT - 2) + layer_id * 2);
}
int gbuffer_header_normal_layer_id_get(uint header, int layer_id)
{
  /* Layer 0 will always have normal id 0. */
  if (layer_id == 0) {
    return 0;
  }
  /* -2 is to skip the layer_id 0 and start encoding for layer_id 1. This keeps the FMA. */
  return int(3u & (header >> ((GBUFFER_NORMAL_BITS_SHIFT - 2) + layer_id * 2)));
}

void gbuffer_append_normal(inout GBufferWriter gbuf, vec3 normal)
{
  vec2 packed_N = gbuffer_normal_pack(normal);
  int layer_id = gbuf.bins_len - 1;
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
vec3 gbuffer_normal_get(inout GBufferReader gbuf, int layer_id, samplerGBufferNormal normal_tx)
{
  int normal_layer_id = gbuffer_header_normal_layer_id_get(gbuf.header, layer_id);
  vec2 normal_packed = fetchGBuffer(normal_tx, gbuf.texel, normal_layer_id).rg;
  gbuf.normal_len = max(gbuf.normal_len, normal_layer_id + 1);
  return gbuffer_normal_unpack(normal_packed);
}
void gbuffer_skip_normal(inout GBufferReader gbuf)
{
  /* Nothing to do. Normals are indexed. */
}

/* Pack geometry additional infos onto the normal stack. Needs to be run last. */
void gbuffer_additional_info_pack(inout GBufferWriter gbuf, float thickness, uint object_id)
{
  gbuf.N[gbuf.normal_len] = vec2(gbuffer_thickness_pack(thickness),
                                 gbuffer_object_id_unorm16_pack(object_id));
  gbuf.normal_len++;
}
void gbuffer_additional_info_load(inout GBufferReader gbuf, samplerGBufferNormal normal_tx)
{
  vec2 data_packed = fetchGBuffer(normal_tx, gbuf.texel, gbuf.normal_len).rg;
  gbuf.normal_len++;
  gbuf.thickness = gbuffer_thickness_unpack(data_packed.x);
  gbuf.object_id = gbuffer_object_id_unorm16_unpack(data_packed.y);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack / Unpack Closures
 *
 * \{ */

/** Outputting dummy closure is required for correct render passes in case of unlit materials. */
void gbuffer_closure_unlit_pack(inout GBufferWriter gbuf, vec3 N)
{
  gbuffer_append_closure(gbuf, GBUF_UNLIT);
  gbuffer_append_data(gbuf, vec4(0.0));
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
                                  int layer,
                                  samplerGBufferClosure closure_tx,
                                  samplerGBufferNormal normal_tx)
{
  vec4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_DIFFUSE_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.N = gbuffer_normal_get(gbuf, layer, normal_tx);

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
                                      int layer,
                                      samplerGBufferClosure closure_tx,
                                      samplerGBufferNormal normal_tx)
{
  vec4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_TRANSLUCENT_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.N = gbuffer_normal_get(gbuf, layer, normal_tx);

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
                                     int layer,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx)
{
  vec4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  vec4 data1 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSSRDF_BURLEY_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.data.rgb = gbuffer_sss_radii_unpack(data1);
  cl.N = gbuffer_normal_get(gbuf, layer, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

void gbuffer_closure_reflection_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  gbuffer_append_closure(gbuf, GBUF_REFLECTION);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl.color));
  gbuffer_append_data(gbuf, vec4(cl.data.x, 0.0, 0.0, 0.0));
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
                                     int layer,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx)
{
  vec4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  vec4 data1 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.data.x = data1.x;
  cl.N = gbuffer_normal_get(gbuf, layer, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

void gbuffer_closure_refraction_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  gbuffer_append_closure(gbuf, GBUF_REFRACTION);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl.color));
  gbuffer_append_data(gbuf, vec4(cl.data.x, gbuffer_ior_pack(cl.data.y), 0.0, 0.0));
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
                                     int layer,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx)
{
  vec4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  vec4 data1 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
  cl.color = gbuffer_closure_color_unpack(data0);
  cl.data.x = data1.x;
  cl.data.y = gbuffer_ior_unpack(data1.y);
  cl.N = gbuffer_normal_get(gbuf, layer, normal_tx);

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
  vec2 intensity_packed = gbuffer_closure_intensity_pack(cl.color.r);
  gbuffer_append_closure(gbuf, GBUF_REFLECTION_COLORLESS);
  gbuffer_append_data(gbuf, vec4(cl.data.x, 0.0, intensity_packed));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_reflection_colorless_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_reflection_colorless_load(inout GBufferReader gbuf,
                                               int layer,
                                               samplerGBufferClosure closure_tx,
                                               samplerGBufferNormal normal_tx)
{
  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);

  vec4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  cl.data.x = data0.x;
  cl.color = vec3(gbuffer_closure_intensity_unpack(data0.zw));

  cl.N = gbuffer_normal_get(gbuf, layer, normal_tx);

  gbuffer_register_closure(gbuf, cl, layer);
}

void gbuffer_closure_refraction_colorless_pack(inout GBufferWriter gbuf, ClosureUndetermined cl)
{
  vec2 intensity_packed = gbuffer_closure_intensity_pack(cl.color.r);
  gbuffer_append_closure(gbuf, GBUF_REFRACTION_COLORLESS);
  gbuffer_append_data(gbuf, vec4(cl.data.x, gbuffer_ior_pack(cl.data.y), intensity_packed));
  gbuffer_append_normal(gbuf, cl.N);
}
void gbuffer_closure_refraction_colorless_skip(inout GBufferReader gbuf)
{
  gbuffer_skip_closure(gbuf);
  gbuffer_skip_data(gbuf);
  gbuffer_skip_normal(gbuf);
}
void gbuffer_closure_refraction_colorless_load(inout GBufferReader gbuf,
                                               int layer,
                                               samplerGBufferClosure closure_tx,
                                               samplerGBufferNormal normal_tx)
{
  ClosureUndetermined cl = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);

  vec4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  cl.data.x = data0.x;
  cl.data.y = gbuffer_ior_unpack(data0.y);
  cl.color = vec3(gbuffer_closure_intensity_unpack(data0.zw));

  cl.N = gbuffer_normal_get(gbuf, layer, normal_tx);

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
  vec2 intensity_packed = gbuffer_closure_intensity_pack(cl_coat.color.r);
  gbuffer_append_closure(gbuf, GBUF_METAL_CLEARCOAT);
  gbuffer_append_data(gbuf, gbuffer_closure_color_pack(cl_bottom.color));
  gbuffer_append_data(gbuf, vec4(cl_bottom.data.x, cl_coat.data.x, intensity_packed));
  gbuffer_append_normal(gbuf, cl_bottom.N);
}

void gbuffer_closure_metal_clear_coat_load(inout GBufferReader gbuf,
                                           samplerGBufferClosure closure_tx,
                                           samplerGBufferNormal normal_tx)
{
  vec4 data0 = gbuffer_pop_first_data(gbuf, closure_tx);
  vec4 data1 = gbuffer_pop_first_data(gbuf, closure_tx);

  ClosureUndetermined bottom = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
  bottom.color = gbuffer_closure_color_unpack(data0);
  bottom.data.x = data1.x;

  ClosureUndetermined coat = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
  coat.color = vec3(gbuffer_closure_intensity_unpack(data1.zw));
  coat.data.x = data1.y;

  coat.N = bottom.N = gbuffer_normal_get(gbuf, 0, normal_tx);

  gbuffer_register_closure(gbuf, bottom, 0);
  gbuffer_register_closure(gbuf, coat, 1);
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gbuffer Read / Write
 *
 * \{ */

GBufferWriter gbuffer_pack(GBufferData data_in)
{
  GBufferWriter gbuf;
  gbuf.header = 0u;
  gbuf.bins_len = 0;
  gbuf.data_len = 0;
  gbuf.normal_len = 0;

  /* Check special configurations first. */

  bool has_additional_data = false;
  for (int i = 0; i < GBUFFER_LAYER_MAX; i++) {
    ClosureUndetermined cl = data_in.closure[i];

    if (cl.weight <= 1e-5) {
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
    }
  }

  if (gbuf.normal_len == 0) {
    gbuffer_closure_unlit_pack(gbuf, data_in.surface_N);
  }

  if (has_additional_data) {
    gbuffer_additional_info_pack(gbuf, data_in.thickness, data_in.object_id);
  }

  return gbuf;
}

/* Return the number of closure as encoded in the given header value. */
int gbuffer_closure_count(uint header)
{
  /* Note: Need to be adjusted for different global GBUFFER_LAYER_MAX. */
  const int bits_per_layer = GBUFFER_HEADER_BITS_PER_LAYER;
  uvec3 closure_types = (uvec3(header) >> uvec3(0u, 4u, 8u)) & ((1u << bits_per_layer) - 1);

  if (closure_types.x == GBUF_METAL_CLEARCOAT) {
    return 2;
  }
  return reduce_add(ivec3(not(equal(closure_types, uvec3(0u)))));
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
ClosureType gbuffer_closure_type_get_by_bin(uint header, int bin_index)
{
  /* TODO(fclem): Doesn't take GBUF_METAL_CLEARCOAT into account or other mode that could merge two
   * bins into one layer. */
  const int bits_per_layer = GBUFFER_HEADER_BITS_PER_LAYER;
  uint mode = (header >> (bin_index * bits_per_layer)) & ((1u << bits_per_layer) - 1);
  return gbuffer_mode_to_closure_type(mode);
}

/* Only read closure types out of the header. The rest of GBufferReader is undefined. */
GBufferReader gbuffer_read_header_closure_types(uint header)
{
  GBufferReader gbuf;

  for (int bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
    GBufferMode mode = gbuffer_header_unpack(header, bin);
    ClosureType closure_type = gbuffer_mode_to_closure_type(mode);
    gbuffer_register_closure(gbuf, closure_new(closure_type), bin);
  }
  return gbuf;
}

/* Return the bin index of a closure using its layer index. */
int gbuffer_closure_get_bin_index(GBufferReader gbuf, int layer_index)
{
  int layer = 0;
  for (int bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
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
  return 0;
}

ClosureUndetermined gbuffer_closure_get_by_bin(GBufferReader gbuf, int bin_index)
{
  int layer_index = 0;
  for (int bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
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
                           ivec2 texel)
{
  GBufferReader gbuf;
  gbuf.texel = texel;
  gbuf.thickness = 0.0;
  gbuf.closure_count = 0;
  gbuf.object_id = 0u;
  gbuf.data_len = 0;
  gbuf.normal_len = 0;
  gbuf.surface_N = vec3(0.0);
  for (int bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
    gbuffer_register_closure(gbuf, closure_new(CLOSURE_NONE_ID), bin);
  }

  gbuf.header = fetchGBuffer(header_tx, texel);

  if (gbuf.header == 0u) {
    return gbuf;
  }

  /* First closure is always written. */
  gbuf.surface_N = gbuffer_normal_unpack(fetchGBuffer(normal_tx, texel, 0).xy);

  bool has_additional_data = false;
  for (int bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
    GBufferMode mode = gbuffer_header_unpack(gbuf.header, bin);
    switch (mode) {
      default:
      case GBUF_NONE:
        break;
      case GBUF_DIFFUSE:
        gbuffer_closure_diffuse_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
        gbuf.closure_count++;
        break;
      case GBUF_TRANSLUCENT:
        gbuffer_closure_translucent_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
        gbuf.closure_count++;
        has_additional_data = true;
        break;
      case GBUF_SUBSURFACE:
        gbuffer_closure_subsurface_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
        gbuf.closure_count++;
        has_additional_data = true;
        break;
      case GBUF_REFLECTION:
        gbuffer_closure_reflection_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
        gbuf.closure_count++;
        break;
      case GBUF_REFRACTION:
        gbuffer_closure_refraction_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
        gbuf.closure_count++;
        has_additional_data = true;
        break;
      case GBUF_REFLECTION_COLORLESS:
        gbuffer_closure_reflection_colorless_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
        gbuf.closure_count++;
        break;
      case GBUF_REFRACTION_COLORLESS:
        gbuffer_closure_refraction_colorless_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
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
                                     ivec2 texel,
                                     int bin_index)
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
  for (int bin = 0; bin < GBUFFER_LAYER_MAX; bin++) {
    mode = gbuffer_header_unpack(gbuf.header, bin);

    if (mode != GBUF_NONE && bin >= bin_index) {
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
      gbuffer_closure_diffuse_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
      break;
    case GBUF_TRANSLUCENT:
      gbuffer_closure_translucent_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
      break;
    case GBUF_SUBSURFACE:
      gbuffer_closure_subsurface_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
      break;
    case GBUF_REFLECTION:
      gbuffer_closure_reflection_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
      break;
    case GBUF_REFRACTION:
      gbuffer_closure_refraction_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
      break;
    case GBUF_REFLECTION_COLORLESS:
      gbuffer_closure_reflection_colorless_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
      break;
    case GBUF_REFRACTION_COLORLESS:
      gbuffer_closure_refraction_colorless_load(gbuf, gbuf.closure_count, closure_tx, normal_tx);
      break;
  }

  return gbuffer_closure_get(gbuf, gbuf.closure_count);
}
ClosureUndetermined gbuffer_read_bin(samplerGBufferHeader header_tx,
                                     samplerGBufferClosure closure_tx,
                                     samplerGBufferNormal normal_tx,
                                     ivec2 texel,
                                     int bin_index)
{
  return gbuffer_read_bin(fetchGBuffer(header_tx, texel), closure_tx, normal_tx, texel, bin_index);
}

/* Load thickness data only if available. Return 0 otherwise. */
float gbuffer_read_thickness(uint header, samplerGBufferNormal normal_tx, ivec2 texel)
{
  /* WATCH: Assumes all closures needing additional data are in first bin. */
  switch (gbuffer_closure_type_get_by_bin(header, 0)) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_TRANSLUCENT_ID:
    case CLOSURE_BSSRDF_BURLEY_ID: {
      int normal_len = gbuffer_normal_count(header);
      vec2 data_packed = fetchGBuffer(normal_tx, texel, normal_len).rg;
      return gbuffer_thickness_unpack(data_packed.x);
    }
    default:
      return 0.0;
  }
}

/* Returns the first world normal stored in the gbuffer. Assume gbuffer header is non-null. */
vec3 gbuffer_read_normal(samplerGBufferNormal normal_tx, ivec2 texel)
{
  vec2 normal_packed = fetchGBuffer(normal_tx, texel, 0).rg;
  return gbuffer_normal_unpack(normal_packed);
}

/** \} */
