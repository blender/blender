/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Combine light passes to the combined color target and apply surface colors.
 * This also fills the different render passes.
 */

#include "infos/eevee_deferred_info.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_debug_gbuffer)

#include "draw_view_lib.glsl"
#include "eevee_gbuffer_lib.glsl"
#include "eevee_renderpass_lib.glsl"
#include "gpu_shader_debug_gradients_lib.glsl"

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  if (gbuf.closure_count == 0) {
    discard;
    return;
  }

  float shade = saturate(drw_normal_world_to_view(gbuf.surface_N).z);

  uint header = texelFetch(gbuf_header_tx, texel, 0).x;
  uvec4 closure_types = (uvec4(header) >> uvec4(0u, 4u, 8u, 12u)) & 15u;
  float storage_cost = reduce_add(vec4(not(equal(closure_types, uvec4(0u)))));

  float eval_cost = 0.0;
  for (uchar i = 0; i < GBUFFER_LAYER_MAX && i < gbuf.closure_count; i++) {
    switch (gbuffer_closure_get(gbuf, i).type) {
      case CLOSURE_BSDF_DIFFUSE_ID:
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        eval_cost += 1.0;
        break;
      case CLOSURE_BSSRDF_BURLEY_ID:
        eval_cost += 2.0;
        break;
      case CLOSURE_NONE_ID:
        /* TODO(fclem): Assert. */
        break;
    }
  }

  switch (eDebugMode(debug_mode)) {
    default:
    case DEBUG_GBUFFER_STORAGE:
      out_color_add = shade * vec4(green_to_red_gradient(storage_cost / 4.0), 0.0);
      break;
    case DEBUG_GBUFFER_EVALUATION:
      out_color_add = shade * vec4(green_to_red_gradient(eval_cost / 4.0), 0.0);
      break;
  }

  out_color_mul = vec4(0.0);
}
