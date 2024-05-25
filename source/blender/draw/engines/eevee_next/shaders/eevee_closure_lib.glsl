/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_microfacet_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_diffuse_lib.glsl)

/* Return the apparent roughness of a closure compared to a GGX reflection lobe. */
float closure_apparent_roughness_get(ClosureUndetermined cl)
{
  switch (cl.type) {
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      return bxdf_diffuse_perceived_roughness();
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return bxdf_translucent_perceived_roughness();
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return bxdf_ggx_perceived_roughness_reflection(to_closure_reflection(cl).roughness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return bxdf_ggx_perceived_roughness_transmission(to_closure_refraction(cl).roughness,
                                                       to_closure_refraction(cl).ior);
    default:
      return 0.0;
  }
}

float closure_evaluate_pdf(ClosureUndetermined cl, vec3 L, vec3 V, float thickness)
{
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return bxdf_translucent_eval(cl.N, L, thickness).pdf;
    case CLOSURE_BSSRDF_BURLEY_ID:
      /* TODO(fclem): Sampled BSSDF. */
      return bxdf_diffuse_eval(cl.N, L).pdf;
    case CLOSURE_BSDF_DIFFUSE_ID:
      return bxdf_diffuse_eval(cl.N, L).pdf;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID: {
      ClosureReflection cl_ = to_closure_reflection(cl);
      return bxdf_ggx_eval_reflection(cl.N, L, V, square(cl_.roughness)).pdf;
    }
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
      ClosureRefraction cl_ = to_closure_refraction(cl);
      return bxdf_ggx_eval_transmission(cl.N, L, V, square(cl_.roughness), cl_.ior, thickness).pdf;
    }
  }
  /* TODO(fclem): Assert. */
  return 0.0;
}

LightProbeRay bxdf_lightprobe_ray(ClosureUndetermined cl, vec3 P, vec3 V, float thickness)
{
  switch (cl.type) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      bxdf_ggx_context_amend_transmission(cl, V, thickness);
      break;
  }

  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return bxdf_translucent_lightprobe(cl.N, thickness);
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      return bxdf_diffuse_lightprobe(cl.N);
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return bxdf_ggx_lightprobe_reflection(to_closure_reflection(cl), V);
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return bxdf_ggx_lightprobe_transmission(to_closure_refraction(cl), V, thickness);
    default:
      /* TODO: Assert. */
      break;
  }

  LightProbeRay ray;
  return ray;
}

#ifdef EEVEE_UTILITY_TX

ClosureLight closure_light_new_ex(ClosureUndetermined cl,
                                  vec3 V,
                                  float thickness,
                                  const bool is_transmission)
{
  ClosureLight cl_light;
  if (is_transmission) {
    /* Transmission. */
    switch (cl.type) {
      case CLOSURE_BSSRDF_BURLEY_ID:
        /* If the `thickness / sss_radius` ratio is near 0, this transmission term should converge
         * to a uniform term like the translucent BSDF. But we need to find what to do in other
         * cases. For now, approximate the transmission term as just back-facing. */
        cl_light = bxdf_translucent_light(cl, V, 0.0);
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        cl_light = bxdf_ggx_light_transmission(to_closure_refraction(cl), V, thickness);
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      default:
        cl_light = bxdf_translucent_light(cl, V, thickness);
        break;
    }
  }
  else {
    /* Reflection. */
    switch (cl.type) {
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
        cl_light = bxdf_ggx_light_reflection(to_closure_reflection(cl), V);
        break;
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
      default:
        cl_light = bxdf_diffuse_light(cl);
        break;
    }
  }
  cl_light.light_shadowed = vec3(0.0);
  cl_light.light_unshadowed = vec3(0.0);
  return cl_light;
}

ClosureLight closure_light_new(ClosureUndetermined cl, vec3 V, float thickness)
{
  return closure_light_new_ex(cl, V, thickness, true);
}

ClosureLight closure_light_new(ClosureUndetermined cl, vec3 V)
{
  return closure_light_new_ex(cl, V, 0.0, false);
}

#endif
