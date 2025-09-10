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

#include "eevee_gbuffer_lib.glsl"

/* NOTE: Only specialized for the gbuffer pass. */
#ifndef GBUFFER_LAYER_MAX
#  define GBUFFER_LAYER_MAX 3
#endif

/* TODO(fclem): This should save some compile time per material. */
#define GBUFFER_HAS_REFLECTION
#define GBUFFER_HAS_REFRACTION
#define GBUFFER_HAS_SUBSURFACE
#define GBUFFER_HAS_TRANSLUCENT

namespace gbuffer::detail {

uint fetch_object_id(int2 texel)
{
  return texelFetch(sampler_get(eevee_gbuffer_data, gbuf_header_tx), int3(texel, 1), 0).r;
}

float4 fetch_data(int2 texel, uchar layer)
{
#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
  /* WORKAROUND: Compiler bug where the loads are somehow invalid inside the ray tile
   * classification shader. */
  layer = min(layer, 9999);
#endif
  return texelFetch(sampler_get(eevee_gbuffer_data, gbuf_closure_tx), int3(texel, layer), 0);
}

float4 fetch_normal(int2 texel, uchar layer)
{
#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
  /* WORKAROUND: Compiler bug where the loads are somehow invalid inside the ray tile
   * classification shader. */
  layer = min(layer, 9999);
#endif
  return texelFetch(sampler_get(eevee_gbuffer_data, gbuf_normal_tx), int3(texel, layer), 0);
}

float4 fetch_additional_data(int2 texel)
{
  auto &samp = sampler_get(eevee_gbuffer_data, gbuf_normal_tx);
  return texelFetch(samp, int3(texel, textureSize(samp, 0).z - 1), 0);
}

ClosureUndetermined unpack_closure(gbuffer::ClosurePacking cl_in)
{
  ClosureUndetermined cl;
  cl.type = gbuffer::mode_to_closure_type(cl_in.mode);
  /* Common to all configs. */
  cl.color = gbuffer::closure_color_unpack(cl_in.data0);
  cl.N = cl_in.N;
  /* Some closures require additional unpacking. */
  switch (cl_in.mode) {
#ifdef GBUFFER_HAS_REFLECTION
    case GBUF_REFLECTION:
      gbuffer::Reflection::unpack_additional(cl, cl_in.data1);
      break;
    case GBUF_REFLECTION_COLORLESS:
      gbuffer::ReflectionColorless::unpack_additional(cl, cl_in.data0);
      break;
#endif
#ifdef GBUFFER_HAS_REFRACTION
    case GBUF_REFRACTION:
      gbuffer::Refraction::unpack_additional(cl, cl_in.data1);
      break;
    case GBUF_REFRACTION_COLORLESS:
      gbuffer::RefractionColorless::unpack_additional(cl, cl_in.data0);
      break;
#endif
#ifdef GBUFFER_HAS_SUBSURFACE
    case GBUF_SUBSURFACE:
      gbuffer::Subsurface::unpack_additional(cl, cl_in.data1);
      break;
#endif
    default:
      break;
  }
  return cl;
}

/* Read only one layer from the GBuffer. */
ClosureUndetermined read_layer(
    uchar normal_id, uchar closure_len, GBufferMode bin_mode, int2 texel, uchar layer_id)
{
  if (bin_mode == GBUF_NONE) {
    return closure_new(ClosureType(CLOSURE_NONE_ID));
  }

  gbuffer::ClosurePacking cl_in;
  cl_in.mode = bin_mode;

  float2 packed_N = gbuffer::detail::fetch_normal(texel, normal_id).xy;
  cl_in.N = gbuffer::normal_unpack(packed_N);

  cl_in.data0 = gbuffer::detail::fetch_data(texel, layer_id);
  if (cl_in.use_data1()) {
    cl_in.data1 = gbuffer::detail::fetch_data(texel, layer_id + closure_len);
  }
  return unpack_closure(cl_in);
}

}  // namespace gbuffer::detail

namespace gbuffer {

using Header = gbuffer::Header;

/* Result of reading the GBuffer. Data are to be indexed by layers.
 * Note that the normal of the first closure is always guaranteed to be valid even if the closure
 * has invalid type.*/
struct Layers {
  ClosureUndetermined layer[GBUFFER_LAYER_MAX];
  Header header;

  /* TODO(fclem): Ideally, all loops that index this should be unrolled. */
  ClosureUndetermined layer_get(uchar i) const
  {
    switch (i) {
      case 0:
        return this->layer[0];
#if GBUFFER_LAYER_MAX > 1
      case 1:
        return this->layer[1];
#endif
#if GBUFFER_LAYER_MAX > 2
      case 2:
        return this->layer[2];
#endif
    }
    assert(false);
    return this->layer[0];
  }

  float3 surface_N() const
  {
    return this->layer[0].N;
  }

  bool has_any_closure() const
  {
    return this->layer[0].type != CLOSURE_NONE_ID;
  }
  bool has_no_closure() const
  {
    return this->layer[0].type == CLOSURE_NONE_ID;
  }
};

Header read_header(int2 texel)
{
  auto &tx = sampler_get(eevee_gbuffer_data, gbuf_header_tx);
  return Header::from_data(texelFetch(tx, int3(texel, 0), 0).r);
}

/* Read the entirety of the GBuffer by layer. */
Layers read_layers(int2 texel)
{
  Layers layers;

  layers.header = gbuffer::read_header(texel);
  uint3 layer_types = layers.header.bin_types_per_layer();
  uchar closure_count = layers.header.closure_len();

  layers.layer[0] = gbuffer::detail::read_layer(
      layers.header.tangent_space_id(0), closure_count, GBufferMode(layer_types[0]), texel, 0);

#if GBUFFER_LAYER_MAX > 1
  layers.layer[1] = gbuffer::detail::read_layer(
      layers.header.tangent_space_id(1), closure_count, GBufferMode(layer_types[1]), texel, 1);
#endif

#if GBUFFER_LAYER_MAX > 2
  layers.layer[2] = gbuffer::detail::read_layer(
      layers.header.tangent_space_id(2), closure_count, GBufferMode(layer_types[2]), texel, 2);
#endif
  return layers;
}

/* Read only one bin from the GBuffer. */
ClosureUndetermined read_bin(Header header, int2 texel, uchar bin_index)
{
  GBufferMode bin_mode = header.bin_type(bin_index);

  uchar layer_id = header.bin_to_layer(bin_index);
  uchar normal_id = header.tangent_space_id(layer_id);
  uchar closure_count = header.closure_len();

  return gbuffer::detail::read_layer(normal_id, closure_count, bin_mode, texel, layer_id);
}
ClosureUndetermined read_bin(int2 texel, uchar bin_index)
{
  return read_bin(gbuffer::read_header(texel), texel, bin_index);
}

/* Load thickness data only if available. Return 0 otherwise. */
float read_thickness(Header header, int2 texel)
{
  if (!header.has_additional_data()) {
    return 0.0f;
  }
  float2 data_packed = gbuffer::detail::fetch_additional_data(texel).rg;
  return gbuffer::AdditionalInfo::unpack(data_packed).thickness;
}

/* Returns the first world normal stored in the gbuffer. Assume gbuffer header is non-null. */
float3 read_normal(int2 texel)
{
  return gbuffer::normal_unpack(gbuffer::detail::fetch_normal(texel, 0).rg);
}

/* Returns the object id stored in the gbuffer. Assume gbuffer header is non-null and object id is
 * stored. */
uint read_object_id(int2 texel)
{
  return gbuffer::detail::fetch_object_id(texel);
}

/** \} */

}  // namespace gbuffer
