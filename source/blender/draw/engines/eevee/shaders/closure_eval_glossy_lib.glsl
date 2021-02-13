
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(lights_lib.glsl)
#pragma BLENDER_REQUIRE(lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(ambient_occlusion_lib.glsl)

struct ClosureInputGlossy {
  vec3 N;          /** Shading normal. */
  float roughness; /** Input roughness, not squared. */
};

#define CLOSURE_INPUT_Glossy_DEFAULT ClosureInputGlossy(vec3(0.0), 0.0)

struct ClosureEvalGlossy {
  vec4 ltc_mat;            /** LTC matrix values. */
  float ltc_brdf_scale;    /** LTC BRDF scaling. */
  vec3 probe_sampling_dir; /** Direction to sample probes from. */
  float spec_occlusion;    /** Specular Occlusion. */
  vec3 raytrace_radiance;  /** Raytrace reflection to be accumulated after occlusion. */
};

/* Stubs. */
#define ClosureOutputGlossy ClosureOutput
#define closure_Glossy_grid_eval(cl_in, cl_eval, cl_common, data, cl_out)

#ifdef STEP_RESOLVE /* SSR */
/* Prototype. */
void raytrace_resolve(ClosureInputGlossy cl_in,
                      inout ClosureEvalGlossy cl_eval,
                      inout ClosureEvalCommon cl_common,
                      inout ClosureOutputGlossy cl_out);
#endif

ClosureEvalGlossy closure_Glossy_eval_init(inout ClosureInputGlossy cl_in,
                                           inout ClosureEvalCommon cl_common,
                                           out ClosureOutputGlossy cl_out)
{
  cl_in.N = safe_normalize(cl_in.N);
  cl_in.roughness = clamp(cl_in.roughness, 1e-8, 0.9999);
  cl_out.radiance = vec3(0.0);

  float NV = dot(cl_in.N, cl_common.V);
  vec2 lut_uv = lut_coords(NV, cl_in.roughness);

  ClosureEvalGlossy cl_eval;
  cl_eval.ltc_mat = texture(utilTex, vec3(lut_uv, LTC_MAT_LAYER));
  cl_eval.probe_sampling_dir = specular_dominant_dir(cl_in.N, cl_common.V, sqr(cl_in.roughness));
  cl_eval.spec_occlusion = specular_occlusion(NV, cl_common.occlusion, cl_in.roughness);
  cl_eval.raytrace_radiance = vec3(0.0);

#ifdef STEP_RESOLVE /* SSR */
  raytrace_resolve(cl_in, cl_eval, cl_common, cl_out);
#endif

  /* The brdf split sum LUT is applied after the radiance accumulation.
   * Correct the LTC so that its energy is constant. */
  /* TODO(fclem) Optimize this so that only one scale factor is stored. */
  vec4 ltc_brdf = texture(utilTex, vec3(lut_uv, LTC_BRDF_LAYER)).barg;
  vec2 split_sum_brdf = ltc_brdf.zw;
  cl_eval.ltc_brdf_scale = (ltc_brdf.x + ltc_brdf.y) / (split_sum_brdf.x + split_sum_brdf.y);
  return cl_eval;
}

void closure_Glossy_light_eval(ClosureInputGlossy cl_in,
                               ClosureEvalGlossy cl_eval,
                               ClosureEvalCommon cl_common,
                               ClosureLightData light,
                               inout ClosureOutputGlossy cl_out)
{
  float radiance = light_specular(light.data, cl_eval.ltc_mat, cl_in.N, cl_common.V, light.L);
  radiance *= cl_eval.ltc_brdf_scale;
  cl_out.radiance += light.data.l_color *
                     (light.data.l_spec * light.vis * light.contact_shadow * radiance);
}

void closure_Glossy_planar_eval(ClosureInputGlossy cl_in,
                                ClosureEvalGlossy cl_eval,
                                inout ClosureEvalCommon cl_common,
                                ClosurePlanarData planar,
                                inout ClosureOutputGlossy cl_out)
{
#ifndef STEP_RESOLVE /* SSR already evaluates planar reflections. */
  vec3 probe_radiance = probe_evaluate_planar(
      planar.id, planar.data, cl_common.P, cl_in.N, cl_common.V, cl_in.roughness);
  cl_out.radiance += planar.attenuation * probe_radiance;
#else
  /* HACK: Fix an issue with planar reflections still being counted inside the specular
   * accumulator. This only works because we only use one Glossy closure in the resolve pass. */
  cl_common.specular_accum += planar.attenuation;
#endif
}

void closure_Glossy_cubemap_eval(ClosureInputGlossy cl_in,
                                 ClosureEvalGlossy cl_eval,
                                 ClosureEvalCommon cl_common,
                                 ClosureCubemapData cube,
                                 inout ClosureOutputGlossy cl_out)
{
  vec3 probe_radiance = probe_evaluate_cube(
      cube.id, cl_common.P, cl_eval.probe_sampling_dir, cl_in.roughness);
  cl_out.radiance += cube.attenuation * probe_radiance;
}

void closure_Glossy_indirect_end(ClosureInputGlossy cl_in,
                                 ClosureEvalGlossy cl_eval,
                                 ClosureEvalCommon cl_common,
                                 inout ClosureOutputGlossy cl_out)
{
  /* If not enough light has been accumulated from probes, use the world specular cubemap
   * to fill the remaining energy needed. */
  if (specToggle && cl_common.specular_accum > 0.0) {
    vec3 probe_radiance = probe_evaluate_world_spec(cl_eval.probe_sampling_dir, cl_in.roughness);
    cl_out.radiance += cl_common.specular_accum * probe_radiance;
  }

  /* Apply occlusion on distant lighting. */
  cl_out.radiance *= cl_eval.spec_occlusion;
  /* Apply Raytrace reflections after occlusion since they are direct, local reflections. */
  cl_out.radiance += cl_eval.raytrace_radiance;
}

void closure_Glossy_eval_end(ClosureInputGlossy cl_in,
                             ClosureEvalGlossy cl_eval,
                             ClosureEvalCommon cl_common,
                             inout ClosureOutputGlossy cl_out)
{
#if defined(DEPTH_SHADER) || defined(WORLD_BACKGROUND)
  /* This makes shader resources become unused and avoid issues with samplers. (see T59747) */
  cl_out.radiance = vec3(0.0);
  return;
#endif

  if (!specToggle) {
    cl_out.radiance = vec3(0.0);
  }
}
