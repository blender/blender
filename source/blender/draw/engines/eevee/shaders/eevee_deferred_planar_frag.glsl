/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using captured Gbuffer data.
 */

#include "infos/eevee_deferred_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_deferred_planar_eval)

#include "draw_view_lib.glsl"
#include "eevee_closure_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "eevee_light_eval_lib.glsl"
#include "eevee_lightprobe_volume_eval_lib.glsl"

void main()
{
  int2 texel = int2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;

  const gbuffer::Layers gbuf = gbuffer::read_layers(texel);
  const uchar closure_count = gbuf.header.closure_len();
  const float thickness = gbuffer::read_thickness(gbuf.header, texel);

  float3 albedo_front = float3(0.0f);
  float3 albedo_back = float3(0.0f);

  for (uchar i = 0; i < GBUFFER_LAYER_MAX && i < closure_count; i++) {
    ClosureUndetermined cl = gbuf.layer_get(i);
    switch (cl.type) {
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
        albedo_front += cl.color;
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        albedo_back += (thickness != 0.0f) ? square(cl.color) : cl.color;
        break;
      case CLOSURE_NONE_ID:
        /* TODO(fclem): Assert. */
        break;
    }
  }

  float3 P = drw_point_screen_to_world(float3(screen_uv, depth));
  float3 Ng = gbuf.header.geometry_normal(gbuf.surface_N());
  float3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureUndetermined cl;
  cl.N = gbuf.surface_N();
  cl.type = CLOSURE_BSDF_DIFFUSE_ID;

  ClosureUndetermined cl_transmit;
  cl_transmit.N = gbuf.surface_N();
  cl_transmit.type = CLOSURE_BSDF_TRANSLUCENT_ID;

  uchar receiver_light_set = 0;
  float normal_offset = 0.0f;
  float geometry_offset = 0.0f;
  if (gbuf.header.use_object_id()) {
    uint object_id = gbuffer::read_object_id(texel);
    ObjectInfos object_infos = drw_infos[object_id];
    receiver_light_set = receiver_light_set_get(object_infos);
    normal_offset = object_infos.shadow_terminator_normal_offset;
    geometry_offset = object_infos.shadow_terminator_geometry_offset;
  }

  /* Direct light. */
  ClosureLightStack stack;
  stack.cl[0] = closure_light_new(cl, V);
  light_eval_reflection(stack, P, Ng, V, vPz, receiver_light_set, normal_offset, geometry_offset);

  float3 radiance_front = stack.cl[0].light_shadowed;

  stack.cl[0] = closure_light_new(cl_transmit, V, thickness);
  light_eval_transmission(
      stack, P, Ng, V, vPz, thickness, receiver_light_set, normal_offset, geometry_offset);

  float3 radiance_back = stack.cl[0].light_shadowed;

  /* Indirect light. */
  SphericalHarmonicL1 sh = lightprobe_volume_sample(P, V, Ng);

  radiance_front += spherical_harmonics_evaluate_lambert(Ng, sh);
  radiance_back += spherical_harmonics_evaluate_lambert(-Ng, sh);

  out_radiance = float4(radiance_front * albedo_front + radiance_back * albedo_back, 0.0f);
}
