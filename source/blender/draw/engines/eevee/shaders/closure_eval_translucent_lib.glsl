
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(lights_lib.glsl)
#pragma BLENDER_REQUIRE(lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(ambient_occlusion_lib.glsl)

struct ClosureInputTranslucent {
  vec3 N; /** Shading normal. */
};

#define CLOSURE_INPUT_Translucent_DEFAULT ClosureInputTranslucent(vec3(0.0))

/* Stubs. */
#define ClosureEvalTranslucent ClosureEvalDummy
#define ClosureOutputTranslucent ClosureOutput
#define closure_Translucent_planar_eval(cl_in, cl_eval, cl_common, data, cl_out)
#define closure_Translucent_cubemap_eval(cl_in, cl_eval, cl_common, data, cl_out)

ClosureEvalTranslucent closure_Translucent_eval_init(inout ClosureInputTranslucent cl_in,
                                                     ClosureEvalCommon cl_common,
                                                     out ClosureOutputTranslucent cl_out)
{
  cl_in.N = safe_normalize(cl_in.N);
  cl_out.radiance = vec3(0.0);
  return CLOSURE_EVAL_DUMMY;
}

void closure_Translucent_light_eval(ClosureInputTranslucent cl_in,
                                    ClosureEvalTranslucent cl_eval,
                                    ClosureEvalCommon cl_common,
                                    ClosureLightData light,
                                    inout ClosureOutputTranslucent cl_out)
{
  float radiance = light_diffuse(light.data, cl_in.N, cl_common.V, light.L);
  cl_out.radiance += light.data.l_color * (light.vis * radiance);
}

void closure_Translucent_grid_eval(ClosureInputTranslucent cl_in,
                                   ClosureEvalTranslucent cl_eval,
                                   ClosureEvalCommon cl_common,
                                   ClosureGridData grid,
                                   inout ClosureOutputTranslucent cl_out)
{
  vec3 probe_radiance = probe_evaluate_grid(grid.data, cl_common.P, cl_in.N, grid.local_pos);
  cl_out.radiance += grid.attenuation * probe_radiance;
}

void closure_Translucent_indirect_end(ClosureInputTranslucent cl_in,
                                      ClosureEvalTranslucent cl_eval,
                                      ClosureEvalCommon cl_common,
                                      inout ClosureOutputTranslucent cl_out)
{
  /* If not enough light has been accumulated from probes, use the world specular cubemap
   * to fill the remaining energy needed. */
  if (cl_common.diffuse_accum > 0.0) {
    vec3 probe_radiance = probe_evaluate_world_diff(cl_in.N);
    cl_out.radiance += cl_common.diffuse_accum * probe_radiance;
  }
}

void closure_Translucent_eval_end(ClosureInputTranslucent cl_in,
                                  ClosureEvalTranslucent cl_eval,
                                  ClosureEvalCommon cl_common,
                                  inout ClosureOutputTranslucent cl_out)
{
#if defined(DEPTH_SHADER) || defined(WORLD_BACKGROUND)
  /* This makes shader resources become unused and avoid issues with samplers. (see T59747) */
  cl_out.radiance = vec3(0.0);
  return;
#endif
}
