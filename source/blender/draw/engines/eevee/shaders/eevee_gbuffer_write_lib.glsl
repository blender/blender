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

#include "infos/eevee_common_infos.hh"

#include "eevee_gbuffer_lib.glsl"

/* Allows to reduce shader complexity and compilation time.
 * Prefer removing the defines to let the loading lib have all cases by default. */
#ifndef MAT_REFLECTION
#  undef GBUFFER_HAS_REFLECTION
#endif
#ifndef MAT_REFRACTION
#  undef GBUFFER_HAS_REFRACTION
#endif
#ifndef MAT_SUBSURFACE
#  undef GBUFFER_HAS_SUBSURFACE
#endif
#ifndef MAT_TRANSLUCENT
#  undef GBUFFER_HAS_TRANSLUCENT
#endif

namespace gbuffer {

using ClosurePacking = gbuffer::ClosurePacking;
using Header = gbuffer::Header;

ClosurePacking pack_closure(ClosureUndetermined cl)
{
  ClosurePacking cl_packed;
  cl_packed.mode = gbuffer::closure_type_to_mode(cl.type, gbuffer::color_is_grayscale(cl.color));

  if (cl.weight <= CLOSURE_WEIGHT_CUTOFF) {
    cl_packed.mode = GBUF_NONE;
  }
  /* Common to all configs. */
  cl_packed.N = cl.N;
  cl_packed.data0 = gbuffer::closure_color_pack(cl.color);
  /* Some closures require additional packing. */
  switch (cl_packed.mode) {
#ifdef GBUFFER_HAS_REFLECTION
#  ifndef MAT_REFLECTION_COLORLESS
    case GBUF_REFLECTION:
      gbuffer::Reflection::pack_additional(cl_packed, cl);
      break;
#  endif
    case GBUF_REFLECTION_COLORLESS:
      gbuffer::ReflectionColorless::pack_additional(cl_packed, cl);
      break;
#endif
#ifdef GBUFFER_HAS_REFRACTION
#  ifndef MAT_REFRACTION_COLORLESS
    case GBUF_REFRACTION:
      gbuffer::Refraction::pack_additional(cl_packed, cl);
      break;
#  endif
    case GBUF_REFRACTION_COLORLESS:
      gbuffer::RefractionColorless::pack_additional(cl_packed, cl);
      break;
#endif
#ifdef GBUFFER_HAS_SUBSURFACE
    case GBUF_SUBSURFACE:
      gbuffer::Subsurface::pack_additional(cl_packed, cl);
      break;
#endif
    default:
      break;
  }
  return cl_packed;
}

/* Data laid-out as stored in the gbuffer. */
struct Packed {
  float4 closure[GBUFFER_LAYER_MAX * 2];
  float2 normal[GBUFFER_LAYER_MAX];
  float2 additional_info;
  uint header;
  uint object_id;
  UsedLayerFlag used_layers;
};

/* Transient data used during packing. */
struct Packer {
  /* Packed GBuffer data in layer indexing. */
  ClosurePacking closures[GBUFFER_LAYER_MAX];
  /* Additional info to be stored inside the normal stack. */
  float additional_info;
  /* Header containing which closures are encoded and which normals are used. */
  Header header;

  /* Swap closures to avoid gap in data. Closures are then in layer order. */
  void closures_to_layer_order()
  {
/* NOTE: 4 closures mode are not yet supported but might be in the future. */
#if GBUFFER_LAYER_MAX > 3
    if (this->closures[2].is_empty()) {
      this->closures[2] = this->closures[3];
      this->closures[3].mode = GBUF_NONE;
    }
#endif
#if GBUFFER_LAYER_MAX > 2
    if (this->closures[1].is_empty()) {
      this->closures[1] = this->closures[2];
      this->closures[2].mode = GBUF_NONE;
#  if GBUFFER_LAYER_MAX > 3
      this->closures[2] = this->closures[3];
      this->closures[3].mode = GBUF_NONE;
#  endif
    }
#endif
#if GBUFFER_LAYER_MAX > 1
    if (this->closures[0].is_empty()) {
      this->closures[0] = this->closures[1];
      this->closures[1].mode = GBUF_NONE;
#  if GBUFFER_LAYER_MAX > 2
      this->closures[1] = this->closures[2];
      this->closures[2].mode = GBUF_NONE;
#  endif
#  if GBUFFER_LAYER_MAX > 3
      this->closures[2] = this->closures[3];
      this->closures[3].mode = GBUF_NONE;
#  endif
    }
#endif
  }

  /* Needs to happen in layer order. */
  void reuse_tangent_spaces()
  {
    /* Assume that the header was cleared to 0 and all layers point to the 1st tangent (0 id). */
    /* Since this function runs in layer ordering (after compaction) each layer (if non-empty) can
     * rely on the previous one to also be non-empty. */
#if GBUFFER_LAYER_MAX > 1
    if (!this->closures[1].is_empty()) {
      if (!all(equal(this->closures[0].N, this->closures[1].N))) {
        /* Unique tangent space. */
        this->header.tangent_space_id_set(1, 1);
      }
      else {
        /* Reuse layer 0 tangent space. */
      }
    }
#endif
#if GBUFFER_LAYER_MAX > 2
    if (!this->closures[2].is_empty()) {
      if (!all(equal(this->closures[0].N, this->closures[2].N))) {
        if (!all(equal(this->closures[1].N, this->closures[2].N))) {
          /* Unique tangent space. */
          this->header.tangent_space_id_set(2, 2);
        }
        else {
          /* Reuse layer 1 tangent space. */
          this->header.tangent_space_id_set(2, 1);
        }
      }
      else {
        /* Reuse layer 0 tangent space. */
      }
    }
#endif
  }

  UsedLayerFlag get_used_normal_layers()
  {
    uchar flag = 0;
    if (this->header.tangent_space_id(1) == 1u) {
      flag |= NORMAL_DATA_1;
    }
    if (this->header.tangent_space_id(2) == 2u) {
      flag |= NORMAL_DATA_2;
    }
    return UsedLayerFlag(flag);
  }

  Packed result_get()
  {
    Packed data;
    /* Note: Normals are not interleaved or packed together.
     * Even if they are packed, only the first one is required to be written in the gbuffer.
     * The other ones are optional. This means layer's tangent space are indexed using layer id. */
    data.normal[0] = gbuffer::normal_pack(this->closures[0].N);
#if GBUFFER_LAYER_MAX > 1
    data.normal[1] = gbuffer::normal_pack(this->closures[1].N);
#endif
#if GBUFFER_LAYER_MAX > 2
    data.normal[2] = gbuffer::normal_pack(this->closures[2].N);
#endif
    uint used_layers = this->get_used_normal_layers();

    uint closure_count = this->header.closure_len();

    /* Interleave data to simplify loading code and keep packed storage. */
    switch (closure_count) {
      case 1u:
        data.closure[0] = this->closures[0].data0;
        data.closure[1] = this->closures[0].data1;
        break;
#if GBUFFER_LAYER_MAX > 1
      case 2u:
        data.closure[0] = this->closures[0].data0;
        data.closure[1] = this->closures[1].data0;
#  ifndef GBUFFER_SIMPLE_CLOSURE_LAYOUT
        data.closure[2] = this->closures[0].data1;
        data.closure[3] = this->closures[1].data1;
        set_flag_from_test(used_layers, this->closures[0].use_data1(), CLOSURE_DATA_2);
        set_flag_from_test(used_layers, this->closures[1].use_data1(), CLOSURE_DATA_3);
#  endif
        break;
#endif
#if GBUFFER_LAYER_MAX > 2
      case 3u:
        data.closure[0] = this->closures[0].data0;
        data.closure[1] = this->closures[1].data0;
        data.closure[2] = this->closures[2].data0;
        data.closure[3] = this->closures[0].data1;
        data.closure[4] = this->closures[1].data1;
        data.closure[5] = this->closures[2].data1;
        set_flag_from_test(used_layers, true, CLOSURE_DATA_2);
        set_flag_from_test(used_layers, this->closures[0].use_data1(), CLOSURE_DATA_3);
        set_flag_from_test(used_layers, this->closures[1].use_data1(), CLOSURE_DATA_4);
        set_flag_from_test(used_layers, this->closures[2].use_data1(), CLOSURE_DATA_5);
        break;
#endif
    }

    if (this->header.has_additional_data()) {
      data.additional_info = float2(this->additional_info);
      set_flag_from_test(used_layers, true, ADDITIONAL_DATA);
    }

    if (this->header.use_object_id()) {
      set_flag_from_test(used_layers, true, OBJECT_ID);
    }

    data.used_layers = UsedLayerFlag(used_layers);
    data.header = this->header.raw();

    return data;
  }
};

struct InputClosures {
  ClosureUndetermined closure[GBUFFER_LAYER_MAX];
};

/**
  * surface_N: Fallback normal is there is no closure.
  * thickness: Additional object information if any closure needs it.
  float thickness;
  * use_object_id: True if surface uses a dedicated object id layer. Should only be turned on if
  needed. */
Packed pack(
    InputClosures cl_data, float3 Ng, packed_float3 surface_N, float thickness, bool use_object_id)
{
  Packer packer;
  packer.header = Header::zero();
  packer.header.use_object_id_set(use_object_id);

  for (int i = 0; i < GBUFFER_LAYER_MAX; i++) {
    packer.closures[i] = pack_closure(cl_data.closure[i]);
  }

  for (int i = 0; i < GBUFFER_LAYER_MAX; i++) {
    packer.header.closure_set(i, packer.closures[i].mode);
  }

  if (packer.header.has_additional_data()) {
    packer.additional_info = gbuffer::AdditionalInfo::pack(thickness).x;
  }

  /* ---- Switch from Bin to Layer order. ---- */
  packer.closures_to_layer_order();

  /* This is correct in layer order. */
  bool has_any_closure = packer.closures[0].mode != GBUF_NONE;
  if (!has_any_closure) {
    /* Output dummy closure in the case of unlit materials for correct render passes data. */
    packer.closures[0] = ClosurePacking::fallback(surface_N);
    packer.header.closure_set(0, packer.closures[0].mode);
  }

  packer.reuse_tangent_spaces();

  /* Needs to happen in layer order and after normal fallback. */
  packer.header.geometry_normal_set(Ng, packer.closures[0].N);

  return packer.result_get();
}

/** \} */

}  // namespace gbuffer
