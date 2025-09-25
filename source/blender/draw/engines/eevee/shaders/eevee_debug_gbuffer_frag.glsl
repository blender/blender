/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Combine light passes to the combined color target and apply surface colors.
 * This also fills the different render passes.
 */

#include "infos/eevee_deferred_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_debug_gbuffer)

#include "draw_view_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "gpu_shader_debug_gradients_lib.glsl"

void main()
{
  int2 texel = int2(gl_FragCoord.xy);

  const gbuffer::Layers gbuf = gbuffer::read_layers(texel);

  if (gbuf.has_no_closure()) {
    gpu_discard_fragment();
    return;
  }

  float shade = saturate(drw_normal_world_to_view(gbuf.surface_N()).z);

  gbuffer::Header header = gbuffer::read_header(texel);
  uint4 closure_types = (uint4(header.raw()) >> uint4(0u, 4u, 8u, 12u)) & 15u;
  float storage_cost = reduce_add(float4(not(equal(closure_types, uint4(0u)))));

  float eval_cost = 0.0f;
  for (uchar i = 0; i < GBUFFER_LAYER_MAX; i++) {
    switch (gbuf.layer[i].type) {
      case CLOSURE_BSDF_DIFFUSE_ID:
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        eval_cost += 1.0f;
        break;
      case CLOSURE_BSSRDF_BURLEY_ID:
        eval_cost += 2.0f;
        break;
      case CLOSURE_NONE_ID:
        break;
    }
  }

  switch (eDebugMode(debug_mode)) {
    default:
    case DEBUG_GBUFFER_STORAGE:
      out_color_add = shade * float4(green_to_red_gradient(storage_cost / 4.0f), 0.0f);
      break;
    case DEBUG_GBUFFER_EVALUATION:
      out_color_add = shade * float4(green_to_red_gradient(eval_cost / 4.0f), 0.0f);
      break;
  }

  out_color_mul = float4(0.0f);
}
