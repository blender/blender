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
#include "eevee_lightprobe_eval_lib.glsl"
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

  ClosureUndetermined cl_reflect;
  cl_reflect.type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
  cl_reflect.color = float3(0.0);
  cl_reflect.N = float3(0.0);
  cl_reflect.data = float4(0.0);
  float reflect_weight = 0.0;

  ClosureUndetermined cl_refract;
  cl_refract.type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
  cl_refract.color = float3(0.0);
  cl_refract.N = float3(0.0);
  cl_refract.data = float4(0.0);
  float refract_weight = 0.0;

  for (uchar i = 0; i < GBUFFER_LAYER_MAX && i < closure_count; i++) {
    ClosureUndetermined cl = gbuf.layer_get(i);
    switch (cl.type) {
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID: {
        cl_reflect.color += cl.color;
        /* Average roughness and normals. */
        float weight = reduce_add(cl.color);
        cl_reflect.N += cl.N * weight;
        cl_reflect.data += cl.data * weight;
        reflect_weight += weight;
        break;
      }
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
        albedo_front += cl.color;
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
        albedo_back += (thickness != 0.0f) ? square(cl.color) : cl.color;
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
        cl_refract.color += (thickness != 0.0f) ? square(cl.color) : cl.color;
        /* Average roughness and normals. */
        float weight = reduce_add(cl.color);
        cl_refract.N += cl.N * weight;
        cl_refract.data += cl.data * weight;
        refract_weight += weight;
        break;
      }
      case CLOSURE_NONE_ID:
        /* TODO(fclem): Assert. */
        break;
    }
  }

  {
    float inv_weight = safe_rcp(reflect_weight);
    cl_reflect.N *= inv_weight;
    cl_reflect.data *= inv_weight;
  }
  {
    float inv_weight = safe_rcp(refract_weight);
    cl_refract.N *= inv_weight;
    cl_refract.data *= inv_weight;
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
  stack.cl[1] = closure_light_new(cl_reflect, V);
  light_eval_reflection(stack, P, Ng, V, vPz, receiver_light_set, normal_offset, geometry_offset);

  float3 radiance_front = stack.cl[0].light_shadowed;
  float3 radiance_reflect = stack.cl[1].light_shadowed;

  stack.cl[0] = closure_light_new(cl_transmit, V, thickness);
  stack.cl[1] = closure_light_new(cl_refract, V, thickness);
  light_eval_transmission(
      stack, P, Ng, V, vPz, thickness, receiver_light_set, normal_offset, geometry_offset);

  float3 radiance_back = stack.cl[0].light_shadowed;
  float3 radiance_refract = stack.cl[1].light_shadowed;

  /* Indirect light. */
  SphericalHarmonicL1 sh = lightprobe_volume_sample(P, V, Ng);
  LightProbeSample samp = lightprobe_load(g_data.P, g_data.Ng, V);

  radiance_front += spherical_harmonics_evaluate_lambert(Ng, sh);
  radiance_back += spherical_harmonics_evaluate_lambert(-Ng, sh);
  radiance_reflect += lightprobe_eval(samp, cl_reflect, g_data.P, V, thickness);
  radiance_refract += lightprobe_eval(samp, cl_refract, g_data.P, V, thickness);

  out_radiance = float4(0.0f);
  out_radiance.xyz += radiance_reflect * cl_reflect.color;
  out_radiance.xyz += radiance_refract * cl_refract.color;
  out_radiance.xyz += radiance_front * albedo_front;
  out_radiance.xyz += radiance_back * albedo_back;
}
