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

#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "infos/eevee_common_infos.hh"

#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

enum UsedLayerFlag : uchar {
  /* Data 0 is always used. */
  /* Data 1 is always used. */
  CLOSURE_DATA_2 = 1u << 0u,
  CLOSURE_DATA_3 = 1u << 1u,
  CLOSURE_DATA_4 = 1u << 2u,
  CLOSURE_DATA_5 = 1u << 3u,

  /* Normal 0 is always used. */
  NORMAL_DATA_1 = 1u << 4u,
  NORMAL_DATA_2 = 1u << 5u,

  ADDITIONAL_DATA = 1u << 6u,

  OBJECT_ID = 1u << 7u,
};

namespace gbuffer {

/* NOTE: Only specialized for the gbuffer pass. */
#ifndef GBUFFER_LAYER_MAX
#  define GBUFFER_LAYER_MAX 3
#endif

/* TODO(fclem): This should save some compile time per material. */
#define GBUFFER_HAS_REFLECTION
#define GBUFFER_HAS_REFRACTION
#define GBUFFER_HAS_SUBSURFACE
#define GBUFFER_HAS_TRANSLUCENT

/* -------------------------------------------------------------------- */
/** \name Utilities
 *
 * \{ */

enum GBufferMode : uchar {
  /** None mode for pixels not rendered. */
  GBUF_NONE = 0u,

  /* Reflection. */
  GBUF_DIFFUSE = 1u,
  GBUF_REFLECTION = 2u,
  GBUF_REFLECTION_COLORLESS = 3u,

  /** Used for surfaces that have no lit closure and just encode a normal layer. */
  GBUF_UNLIT = 4u,

  /**
   * Special bit that marks all closures with refraction.
   * Allows to detect the presence of transmission more easily.
   * Note that this left only 2^3 values (minus 0) for encoding the BSDF.
   * Could be removed if that's too cumbersome to add more BSDF.
   */
  GBUF_TRANSMISSION_BIT = 1u << 3u,

  /* Transmission. */
  GBUF_REFRACTION = 0u | GBUF_TRANSMISSION_BIT,
  GBUF_REFRACTION_COLORLESS = 1u | GBUF_TRANSMISSION_BIT,
  GBUF_TRANSLUCENT = 2u | GBUF_TRANSMISSION_BIT,
  GBUF_SUBSURFACE = 3u | GBUF_TRANSMISSION_BIT,

  /** IMPORTANT: Needs to be less than 16 for correct packing in g-buffer header. */
};

GBufferMode closure_type_to_mode(ClosureType type, bool is_grayscale)
{
  switch (type) {
    case CLOSURE_BSDF_DIFFUSE_ID:
      return GBUF_DIFFUSE;
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return GBUF_TRANSLUCENT;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return is_grayscale ? GBUF_REFLECTION_COLORLESS : GBUF_REFLECTION;
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return is_grayscale ? GBUF_REFRACTION_COLORLESS : GBUF_REFRACTION;
    case CLOSURE_BSSRDF_BURLEY_ID:
      return GBUF_SUBSURFACE;
    default:
      return GBUF_NONE;
  }
}

ClosureType mode_to_closure_type(uint mode)
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

bool color_is_grayscale(float3 color)
{
  /* This tests is R == G == B. */
  return all(equal(color.rgb, color.gbr));
}

bool closure_is_empty(ClosureUndetermined cl)
{
  return cl.weight <= CLOSURE_WEIGHT_CUTOFF || cl.type == CLOSURE_NONE_ID;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack / Unpack Utils
 *
 * \{ */

float2 normal_pack(float3 N)
{
  N /= length_manhattan(N);
  float2 _sign = sign(N.xy);
  _sign.x = _sign.x == 0.0f ? 1.0f : _sign.x;
  _sign.y = _sign.y == 0.0f ? 1.0f : _sign.y;
  N.xy = (N.z >= 0.0f) ? N.xy : ((1.0f - abs(N.yx)) * _sign);
  N.xy = N.xy * 0.5f + 0.5f;
  return N.xy;
}
float3 normal_unpack(float2 N_packed)
{
  N_packed = N_packed * 2.0f - 1.0f;
  float3 N = float3(N_packed.x, N_packed.y, 1.0f - abs(N_packed.x) - abs(N_packed.y));
  float t = clamp(-N.z, 0.0f, 1.0f);
  N.x += (N.x >= 0.0f) ? -t : t;
  N.y += (N.y >= 0.0f) ? -t : t;
  return normalize(N);
}

float ior_pack(float ior)
{
  return (ior > 1.0f) ? (1.0f - 0.5f / ior) : (0.5f * ior);
}
float ior_unpack(float ior_packed)
{
  return (ior_packed > 0.5f) ? (0.5f / (1.0f - ior_packed)) : (2.0f * ior_packed);
}

float thickness_pack(float thickness)
{
  /* TODO(fclem): If needed, we could increase precision by defining a ceiling value like the view
   * distance and remap to it. Or tweak the hyperbole equality. */
  /* NOTE: Sign encodes the thickness mode. */
  /* Remap [0..+inf) to [0..1/2]. */
  float thickness_packed = abs(thickness) / (1.0f + 2.0f * abs(thickness));
  /* Mirror the negative from [0..1/2] to [1..1/2]. O is mapped to 0 for precision. */
  return (thickness < 0.0f) ? 1.0f - thickness_packed : thickness_packed;
}
float thickness_unpack(float thickness_packed)
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
float4 closure_color_pack(float3 color)
{
  float max_comp = max(color.x, max(color.y, color.z));
  float exponent = (max_comp > 1) ? ((max_comp > 2) ? ((max_comp > 4) ? 3.0f : 2.0f) : 1.0f) :
                                    0.0f;
  /* TODO(fclem): Could try dithering to avoid banding artifacts on higher exponents. */
  return float4(color / exp2(exponent), exponent / 3.0f);
}
float3 closure_color_unpack(float4 color_packed)
{
  float exponent = color_packed.a * 3.0f;
  return color_packed.rgb * exp2(exponent);
}

float4 sss_radii_pack(float3 sss_radii)
{
  /* TODO(fclem): Something better. */
  return closure_color_pack(
      float3(ior_pack(sss_radii.x), ior_pack(sss_radii.y), ior_pack(sss_radii.z)));
}
float3 sss_radii_unpack(float4 sss_radii_packed)
{
  /* TODO(fclem): Something better. */
  float3 radii_packed = closure_color_unpack(sss_radii_packed);
  return float3(
      ior_unpack(radii_packed.x), ior_unpack(radii_packed.y), ior_unpack(radii_packed.z));
}

float object_id_f16_pack(uint object_id)
{
  /* TODO(fclem): Make use of all the 16 bits in a half float.
   * This here only correctly represent values up to 1024. */
  return float(object_id);
}

uint object_id_f16_unpack(float object_id_packed)
{
  return uint(object_id_packed);
}

/* Quantize geometric normal to 6 bits. */
uint geometry_normal_pack(float3 Ng, float3 N)
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

float3 geometry_normal_unpack(uint data, float3 N)
{
  /* If data is 0 it means the shading normal is representative enough. */
  if ((data & (63u << 20u)) == 0u) {
    return N;
  }
  float3 Ng = float3((uint3(data) >> (uint3(0, 1, 2) + 20u)) & 1u) -
              float3((uint3(data) >> (uint3(3, 4, 5) + 20u)) & 1u);
  return normalize(Ng);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Header
 *
 * \{ */

/**
 * The GBuffer is polymorphic and its content varies from pixel to pixel.
 * The header contains some common informations and the layout of the GBuffer content.
 */
struct Header {
#define GBUFFER_NORMAL_BITS_SHIFT 12u
#define GBUFFER_GEOMETRIC_NORMAL_BITS_SHIFT 20u
#define GBUFFER_HEADER_BITS_PER_BIN 4

 private:
  /**
   * Bit packed header.
   *
   *  Use Object ID
   *    |
   *    |
   * |  v |         UNUSED         |       Geometric normal      |       UNUSED      |
   * |....|....|....|....|....|....|....|....|....|....|....|....|....|....|....|....|
   *   31   30   29   28   27   26   25   24   23   22   21   20   19   18   17   16
   *
   *
   * | Tangent Space ID  |                       Closure Types                       |
   * |                   |                   |                   |                   |
   * | Layer 2 | Layer 1 |------ Bin 2 ------|------ Bin 1 ------|------ Bin 0 ------|
   * |....|....|....|....|....|....|....|....|....|....|....|....|....|....|....|....|
   *   15   14   13   12   11   10    9    8    7    6    5    4    3    2    1    0
   */
  uint header_;

 public:
  static Header from_data(uint data)
  {
    Header header;
    header.header_ = data;
    return header;
  }

  static Header zero()
  {
    return Header::from_data(0u);
  }

  bool is_empty() const
  {
    return this->header_ == 0;
  }

  void closure_set(uint bin, GBufferMode mode)
  {
    this->header_ |= (mode << (GBUFFER_HEADER_BITS_PER_BIN * bin));
  }
  GBufferMode closure(uint bin) const
  {
    return GBufferMode((this->header_ >> (GBUFFER_HEADER_BITS_PER_BIN * bin)) & 15u);
  }

  /* If this flag is set, the header texture has a second layer containing the ObjectIDs. */
  bool use_object_id() const
  {
    return flag_test(this->header_, 1u << 31u);
  }
  void use_object_id_set(bool value)
  {
    set_flag_from_test(this->header_, value, 1u << 31u);
  }

  /**
   * Set the dedicated normal bit for the specified layer.
   * Expects `layer_id` to be in [0..2].
   * Expects `normal_id` to be in [0..3] (packed in 2bits).
   */
  void tangent_space_id_set(uint layer_id, uint normal_id)
  {
    /* Layer 0 will always have normal id 0. It doesn't have to be encoded. Skip it. */
    assert(layer_id > 0u);
    /* Note: Keep this in the if statement as it compiles faster somehow. */
    /* -2 is to skip the layer_id 0 and start encoding for layer_id 1. This keeps the FMA. */
    this->header_ |= normal_id << ((GBUFFER_NORMAL_BITS_SHIFT - 2u) + layer_id * 2u);
  }
  uchar tangent_space_id(uint layer_id) const
  {
    /* Layer 0 will always have normal id 0. */
    if (layer_id == 0u) {
      return 0u;
    }
    /* -2 is to skip the layer_id 0 and start encoding for layer_id 1. This keeps the FMA. */
    return uchar(3u & (this->header_ >> ((GBUFFER_NORMAL_BITS_SHIFT - 2u) + layer_id * 2u)));
  }

  /* Pack geometric normal into the header if needed. */
  void geometry_normal_set(float3 Ng, float3 N)
  {
    this->header_ |= geometry_normal_pack(Ng, N);
  }
  float3 geometry_normal(float3 surface_N) const
  {
    return geometry_normal_unpack(this->header_, surface_N);
  }

  /* Return a vector of GBufferMode. */
  uint3 bin_types() const
  {
    /* NOTE: Need to be adjusted for different global GBUFFER_LAYER_MAX. */
    constexpr uchar bits_per_bin = uchar(GBUFFER_HEADER_BITS_PER_BIN);
    uint3 types = (uint3(this->header_) >> (uint3(0u, 1u, 2u) * bits_per_bin)) &
                  ((1u << bits_per_bin) - 1);
    return types;
  }

  GBufferMode bin_type(uchar bin) const
  {
    constexpr uchar bits_per_bin = uchar(GBUFFER_HEADER_BITS_PER_BIN);
    return GBufferMode((this->header_ >> (bin * bits_per_bin)) & ((1u << bits_per_bin) - 1));
  }

  /* Return a vector of GBufferMode.
   * Same as bin_types() but skip empty bins. */
  uint3 bin_types_per_layer() const
  {
    uint3 modes = this->bin_types();
    if (modes.y == GBUF_NONE) {
      modes = modes.xzy;
    }
    if (modes.x == GBUF_NONE) {
      modes = modes.yzx;
    }
    return modes;
  }

  uint3 bin_index_per_layer() const
  {
    uint3 modes = this->bin_types();
    uint3 bins = uint3(0, 1, 2);
    if (modes.y == GBUF_NONE) {
      bins = bins.xzy;
    }
    if (modes.x == GBUF_NONE) {
      bins = bins.yzx;
    }
    return bins;
  }

  /* Return which closures are empty (equal to GBUF_NONE). */
  bool3 empty_bins() const
  {
    return equal(this->bin_types(), uint3(GBUF_NONE));
  }

  uchar closure_len() const
  {
    return reduce_add(int3(not(this->empty_bins())));
  }

  uchar normal_len() const
  {
    if (this->header_ == 0u) {
      return 0;
    }
    /* Count implicit first layer. */
    uchar count = 1u;
    count += uchar(((this->header_ >> 12u) & 3u) != 0);
    count += uchar(((this->header_ >> 14u) & 3u) != 0);
    return count;
  }

  uint raw() const
  {
    return this->header_;
  }

  bool has_transmission() const
  {
    /* NOTE: Need to be adjusted for different global GBUFFER_LAYER_MAX. */
    constexpr uint bits_per_layer = uint(GBUFFER_HEADER_BITS_PER_BIN);
    constexpr uint header_mask = (GBUF_TRANSMISSION_BIT << (bits_per_layer * 0)) |
                                 (GBUF_TRANSMISSION_BIT << (bits_per_layer * 1)) |
                                 (GBUF_TRANSMISSION_BIT << (bits_per_layer * 2));
    return (this->header_ & header_mask) != 0;
  }

  bool has_additional_data() const
  {
    /* For now, this is true. Only the transmission closures use the thickness data. */
    return this->has_transmission();
  }

  /* For a given bin index, return the associated layer index.
   * Result is undefined if bin has no valid closure. */
  uchar bin_to_layer(uchar bin_id) const
  {
    uint3 types = this->bin_types();
    switch (bin_id) {
      case 2u:
        return uchar(types[0] != GBUF_NONE) + uchar(types[1] != GBUF_NONE);
      case 1u:
        return uchar(types[0] != GBUF_NONE);
    }
    /* Default, bin_id == 0 case. */
    return 0u;
  }
};

/* Added data inside the Tangent Space layers. */
struct AdditionalInfo {
  float thickness;

  METAL_CONSTRUCTOR_1(AdditionalInfo, float, thickness)

  static float2 pack(float thickness)
  {
    return float2(thickness_pack(thickness), 0.0f /* UNUSED */);
  }

  static AdditionalInfo unpack(float2 data)
  {
    return AdditionalInfo(thickness_unpack(data.x));
  }
};

/* Almost 1:1 match with ClosureUndetermined but has packed data. */
struct ClosurePacking {
  /* Packed data layer 0. Always used. */
  float4 data0;
  /* Packed data layer 1. Might not be used. */
  float4 data1;
  /* Unpacked normal. Normal packing happens later. */
  float3 N;
  /* Gbuffer packing mode. */
  GBufferMode mode;

  static ClosurePacking fallback(float3 surface_N)
  {
    ClosurePacking cl;
    cl.mode = GBUF_UNLIT;
    cl.data0 = float4(0);
    cl.data1 = float4(0);
    cl.N = surface_N;
    return cl;
  }

  bool use_data1() const
  {
    return this->mode == GBUF_REFLECTION || this->mode == GBUF_REFRACTION ||
           this->mode == GBUF_SUBSURFACE;
  }

  bool is_empty() const
  {
    return this->mode == GBUF_NONE;
  }
};

/** \} */

}  // namespace gbuffer

namespace gbuffer {

using ClosurePacking = gbuffer::ClosurePacking;

/* -------------------------------------------------------------------- */
/** \name Pack / Unpack Closures
 *
 * \{ */

struct Subsurface {
  static void pack_additional(ClosurePacking &cl_packed, ClosureUndetermined cl)
  {
    cl_packed.data1 = gbuffer::sss_radii_pack(cl.data.xyz);
  }

  static void unpack_additional(ClosureUndetermined &cl, float4 data1)
  {
    cl.data.rgb = gbuffer::sss_radii_unpack(data1);
  }
};

struct Reflection {
  static void pack_additional(ClosurePacking &cl_packed, ClosureUndetermined cl)
  {
    cl_packed.data1 = float4(cl.data.x, 0.0f, 0.0f, 0.0f);
  }

  static void unpack_additional(ClosureUndetermined &cl, float4 data1)
  {
    cl.data.x = data1.x; /* Roughness. */
  }
};

struct Refraction {
  static void pack_additional(ClosurePacking &cl_packed, ClosureUndetermined cl)
  {
    cl_packed.data1 = float4(cl.data.x, gbuffer::ior_pack(cl.data.y), 0.0f, 0.0f);
  }

  static void unpack_additional(ClosureUndetermined &cl, float4 data1)
  {
    cl.data.x = data1.x; /* Roughness. */
    cl.data.y = gbuffer::ior_unpack(data1.y);
  }
};

/* Special case where we can save 1 data layers per closure. */
struct ReflectionColorless {
  static void pack_additional(ClosurePacking &cl_packed, ClosureUndetermined cl)
  {
    cl_packed.data0 = float4(cl.data.x, 0.0f, cl_packed.data0.zw);
  }

  static void unpack_additional(ClosureUndetermined &cl, float4 data0)
  {
    cl.color = cl.color.zzz;
    cl.data.x = data0.x; /* Roughness. */
  }
};

/* Special case where we can save 1 data layers per closure. */
struct RefractionColorless {
  static void pack_additional(ClosurePacking &cl_packed, ClosureUndetermined cl)
  {
    cl_packed.data0 = float4(cl.data.x, gbuffer::ior_pack(cl.data.y), cl_packed.data0.zw);
  }

  static void unpack_additional(ClosureUndetermined &cl, float4 data0)
  {
    cl.color = cl.color.zzz;
    cl.data.x = data0.x; /* Roughness. */
    cl.data.y = gbuffer::ior_unpack(data0.y);
  }
};

/** \} */

}  // namespace gbuffer
