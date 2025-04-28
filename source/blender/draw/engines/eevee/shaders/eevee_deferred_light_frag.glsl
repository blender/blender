/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using Gbuffer data.
 */

#include "infos/eevee_deferred_info.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_deferred_light)

#include "draw_view_lib.glsl"
#include "eevee_gbuffer_lib.glsl"
#include "eevee_light_eval_lib.glsl"
#include "eevee_lightprobe_eval_lib.glsl"
#include "eevee_renderpass_lib.glsl"
#include "eevee_subsurface_lib.glsl"
#include "eevee_thickness_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_shared_exponent_lib.glsl"

void write_radiance_direct(uchar layer_index, int2 texel, float3 radiance)
{
  /* TODO(fclem): Layered texture. */
  uint data = rgb9e5_encode(radiance);
  if (layer_index == 0u) {
    imageStore(direct_radiance_1_img, texel, uint4(data));
  }
  else if (layer_index == 1u) {
    imageStore(direct_radiance_2_img, texel, uint4(data));
  }
  else if (layer_index == 2u) {
    imageStore(direct_radiance_3_img, texel, uint4(data));
  }
}

void write_radiance_indirect(uchar layer_index, int2 texel, float3 radiance)
{
  /* TODO(fclem): Layered texture. */
  if (layer_index == 0u) {
    imageStore(indirect_radiance_1_img, texel, float4(radiance, 1.0f));
  }
  else if (layer_index == 1u) {
    imageStore(indirect_radiance_2_img, texel, float4(radiance, 1.0f));
  }
  else if (layer_index == 2u) {
    imageStore(indirect_radiance_3_img, texel, float4(radiance, 1.0f));
  }
}

void main()
{
  int2 texel = int2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  /* Bias the shading point position because of depth buffer precision.
   * Constant is taken from https://www.terathon.com/gdc07_lengyel.pdf. */
  constexpr float bias = 2.4e-7f;
  depth -= bias;

  float3 P = drw_point_screen_to_world(float3(screen_uv, depth));
  float3 Ng = gbuffer_geometry_normal_unpack(gbuf.header, gbuf.surface_N);
  float3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureLightStack stack;
  /* Unroll light stack array assignments to avoid non-constant indexing. */
  for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
    closure_light_set(stack, i, closure_light_new(gbuffer_closure_get(gbuf, i), V));
  }

  uchar receiver_light_set = 0;
  float normal_offset = 0.0f;
  float geometry_offset = 0.0f;
  if (gbuffer_use_object_id_unpack(gbuf.header)) {
    uint object_id = texelFetch(gbuf_header_tx, int3(texel, 1), 0).x;
    ObjectInfos object_infos = drw_infos[object_id];
    receiver_light_set = receiver_light_set_get(object_infos);
    normal_offset = object_infos.shadow_terminator_normal_offset;
    geometry_offset = object_infos.shadow_terminator_geometry_offset;
  }

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluation and skip evaluating the transmission closure twice. */
  light_eval_reflection(stack, P, Ng, V, vPz, receiver_light_set, normal_offset, geometry_offset);

  if (use_transmission) {
    ClosureUndetermined cl_transmit = gbuffer_closure_get(gbuf, 0);
#if 1 /* TODO Limit to SSS. */
    float3 sss_reflect_shadowed, sss_reflect_unshadowed;
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      sss_reflect_shadowed = stack.cl[0].light_shadowed;
      sss_reflect_unshadowed = stack.cl[0].light_unshadowed;
    }
#endif

    stack.cl[0] = closure_light_new(cl_transmit, V, gbuf.thickness);

    /* NOTE: Only evaluates `stack.cl[0]`. */
    light_eval_transmission(
        stack, P, Ng, V, vPz, gbuf.thickness, receiver_light_set, normal_offset, geometry_offset);

#if 1 /* TODO Limit to SSS. */
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      /* Apply transmission profile onto transmitted light and sum with reflected light. */
      float3 sss_profile = subsurface_transmission(to_closure_subsurface(cl_transmit).sss_radius,
                                                   abs(gbuf.thickness));
      stack.cl[0].light_shadowed *= sss_profile;
      stack.cl[0].light_unshadowed *= sss_profile;
      stack.cl[0].light_shadowed += sss_reflect_shadowed;
      stack.cl[0].light_unshadowed += sss_reflect_unshadowed;
    }
#endif
  }

  if (render_pass_shadow_id != -1) {
    float3 radiance_shadowed = float3(0);
    float3 radiance_unshadowed = float3(0);
    for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
      radiance_shadowed += closure_light_get(stack, i).light_shadowed;
      radiance_unshadowed += closure_light_get(stack, i).light_unshadowed;
    }
    float3 shadows = radiance_shadowed * safe_rcp(radiance_unshadowed);
    output_renderpass_value(render_pass_shadow_id, average(shadows));
  }

  if (use_lightprobe_eval) {
    LightProbeSample samp = lightprobe_load(P, Ng, V);

    float clamp_indirect = uniform_buf.clamp.surface_indirect;
    samp.volume_irradiance = spherical_harmonics_clamp(samp.volume_irradiance, clamp_indirect);

    for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
      ClosureUndetermined cl = gbuffer_closure_get(gbuf, i);
      float3 indirect_light = lightprobe_eval(samp, cl, P, V, gbuf.thickness);

      uchar layer_index = gbuffer_closure_get_bin_index(gbuf, i);
      float3 direct_light = closure_light_get(stack, i).light_shadowed;
      if (use_split_indirect) {
        write_radiance_indirect(layer_index, texel, indirect_light);
        write_radiance_direct(layer_index, texel, direct_light);
      }
      else {
        write_radiance_direct(layer_index, texel, direct_light + indirect_light);
      }
    }
  }
  else {
    for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
      uchar layer_index = gbuffer_closure_get_bin_index(gbuf, i);
      float3 direct_light = closure_light_get(stack, i).light_shadowed;
      write_radiance_direct(layer_index, texel, direct_light);
    }
  }
}
