
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(lights_lib.glsl)
#pragma BLENDER_REQUIRE(lightprobe_lib.glsl)

/**
 * Extensive use of Macros to be able to change the maximum amount of evaluated closure easily.
 * NOTE: GLSL does not support variadic macros.
 *
 * Example
 * // Declare the cl_eval function
 * CLOSURE_EVAL_FUNCTION_DECLARE_3(name, Diffuse, Glossy, Refraction);
 * // Declare the inputs & outputs
 * CLOSURE_VARS_DECLARE(Diffuse, Glossy, Refraction);
 * // Specify inputs
 * in_Diffuse_0.N = N;
 * ...
 * // Call the cl_eval function
 * CLOSURE_EVAL_FUNCTION_3(name, Diffuse, Glossy, Refraction);
 * // Get the cl_out
 * closure.radiance = out_Diffuse_0.radiance + out_Glossy_1.radiance + out_Refraction_2.radiance;
 **/

#define CLOSURE_VARS_DECLARE(t0, t1, t2, t3) \
  ClosureInputCommon in_common = CLOSURE_INPUT_COMMON_DEFAULT; \
  ClosureInput##t0 in_##t0##_0 = CLOSURE_INPUT_##t0##_DEFAULT; \
  ClosureInput##t1 in_##t1##_1 = CLOSURE_INPUT_##t1##_DEFAULT; \
  ClosureInput##t2 in_##t2##_2 = CLOSURE_INPUT_##t2##_DEFAULT; \
  ClosureInput##t3 in_##t3##_3 = CLOSURE_INPUT_##t3##_DEFAULT; \
  ClosureOutput##t0 out_##t0##_0; \
  ClosureOutput##t1 out_##t1##_1; \
  ClosureOutput##t2 out_##t2##_2; \
  ClosureOutput##t3 out_##t3##_3;

#define CLOSURE_EVAL_DECLARE(t0, t1, t2, t3) \
  ClosureEvalCommon cl_common = closure_Common_eval_init(in_common); \
  ClosureEval##t0 eval_##t0##_0 = closure_##t0##_eval_init(in_##t0##_0, cl_common, out_##t0##_0); \
  ClosureEval##t1 eval_##t1##_1 = closure_##t1##_eval_init(in_##t1##_1, cl_common, out_##t1##_1); \
  ClosureEval##t2 eval_##t2##_2 = closure_##t2##_eval_init(in_##t2##_2, cl_common, out_##t2##_2); \
  ClosureEval##t3 eval_##t3##_3 = closure_##t3##_eval_init(in_##t3##_3, cl_common, out_##t3##_3);

#define CLOSURE_META_SUBROUTINE(subroutine, t0, t1, t2, t3) \
  closure_##t0##_##subroutine(in_##t0##_0, eval_##t0##_0, cl_common, out_##t0##_0); \
  closure_##t1##_##subroutine(in_##t1##_1, eval_##t1##_1, cl_common, out_##t1##_1); \
  closure_##t2##_##subroutine(in_##t2##_2, eval_##t2##_2, cl_common, out_##t2##_2); \
  closure_##t3##_##subroutine(in_##t3##_3, eval_##t3##_3, cl_common, out_##t3##_3);

#define CLOSURE_META_SUBROUTINE_DATA(subroutine, sub_data, t0, t1, t2, t3) \
  closure_##t0##_##subroutine(in_##t0##_0, eval_##t0##_0, cl_common, sub_data, out_##t0##_0); \
  closure_##t1##_##subroutine(in_##t1##_1, eval_##t1##_1, cl_common, sub_data, out_##t1##_1); \
  closure_##t2##_##subroutine(in_##t2##_2, eval_##t2##_2, cl_common, sub_data, out_##t2##_2); \
  closure_##t3##_##subroutine(in_##t3##_3, eval_##t3##_3, cl_common, sub_data, out_##t3##_3);

/* Inputs are inout so that callers can get the final inputs used for evaluation. */
#define CLOSURE_EVAL_FUNCTION_DECLARE(name, t0, t1, t2, t3) \
  void closure_##name##_eval(ClosureInputCommon in_common, \
                             inout ClosureInput##t0 in_##t0##_0, \
                             inout ClosureInput##t1 in_##t1##_1, \
                             inout ClosureInput##t2 in_##t2##_2, \
                             inout ClosureInput##t3 in_##t3##_3, \
                             out ClosureOutput##t0 out_##t0##_0, \
                             out ClosureOutput##t1 out_##t1##_1, \
                             out ClosureOutput##t2 out_##t2##_2, \
                             out ClosureOutput##t3 out_##t3##_3) \
  { \
    CLOSURE_EVAL_DECLARE(t0, t1, t2, t3); \
\
    /* Starts at 1 because 0 is world cubemap. */ \
    for (int i = 1; cl_common.specular_accum > 0.0 && i < prbNumRenderCube && i < MAX_PROBE; \
         i++) { \
      ClosureCubemapData cube = closure_cubemap_eval_init(i, cl_common); \
      if (cube.attenuation > 1e-8) { \
        CLOSURE_META_SUBROUTINE_DATA(cubemap_eval, cube, t0, t1, t2, t3); \
      } \
    } \
\
    /* Starts at 1 because 0 is world irradiance. */ \
    for (int i = 1; cl_common.diffuse_accum > 0.0 && i < prbNumRenderGrid && i < MAX_GRID; i++) { \
      ClosureGridData grid = closure_grid_eval_init(i, cl_common); \
      if (grid.attenuation > 1e-8) { \
        CLOSURE_META_SUBROUTINE_DATA(grid_eval, grid, t0, t1, t2, t3); \
      } \
    } \
\
    CLOSURE_META_SUBROUTINE(indirect_end, t0, t1, t2, t3); \
\
    ClosurePlanarData planar = closure_planar_eval_init(cl_common); \
    if (planar.attenuation > 1e-8) { \
      CLOSURE_META_SUBROUTINE_DATA(planar_eval, planar, t0, t1, t2, t3); \
    } \
\
    for (int i = 0; i < laNumLight && i < MAX_LIGHT; i++) { \
      ClosureLightData light = closure_light_eval_init(cl_common, i); \
      if (light.vis > 1e-8) { \
        CLOSURE_META_SUBROUTINE_DATA(light_eval, light, t0, t1, t2, t3); \
      } \
    } \
\
    CLOSURE_META_SUBROUTINE(eval_end, t0, t1, t2, t3); \
  }

#define CLOSURE_EVAL_FUNCTION(name, t0, t1, t2, t3) \
  closure_##name##_eval(in_common, \
                        in_##t0##_0, \
                        in_##t1##_1, \
                        in_##t2##_2, \
                        in_##t3##_3, \
                        out_##t0##_0, \
                        out_##t1##_1, \
                        out_##t2##_2, \
                        out_##t3##_3)

#define CLOSURE_EVAL_FUNCTION_DECLARE_1(name, t0) \
  CLOSURE_EVAL_FUNCTION_DECLARE(name, t0, Dummy, Dummy, Dummy)
#define CLOSURE_EVAL_FUNCTION_DECLARE_2(name, t0, t1) \
  CLOSURE_EVAL_FUNCTION_DECLARE(name, t0, t1, Dummy, Dummy)
#define CLOSURE_EVAL_FUNCTION_DECLARE_3(name, t0, t1, t2) \
  CLOSURE_EVAL_FUNCTION_DECLARE(name, t0, t1, t2, Dummy)
#define CLOSURE_EVAL_FUNCTION_DECLARE_4(name, t0, t1, t2, t3) \
  CLOSURE_EVAL_FUNCTION_DECLARE(name, t0, t1, t2, t3)

#define CLOSURE_VARS_DECLARE_1(t0) CLOSURE_VARS_DECLARE(t0, Dummy, Dummy, Dummy)
#define CLOSURE_VARS_DECLARE_2(t0, t1) CLOSURE_VARS_DECLARE(t0, t1, Dummy, Dummy)
#define CLOSURE_VARS_DECLARE_3(t0, t1, t2) CLOSURE_VARS_DECLARE(t0, t1, t2, Dummy)
#define CLOSURE_VARS_DECLARE_4(t0, t1, t2, t3) CLOSURE_VARS_DECLARE(t0, t1, t2, t3)

#define CLOSURE_EVAL_FUNCTION_1(name, t0) CLOSURE_EVAL_FUNCTION(name, t0, Dummy, Dummy, Dummy)
#define CLOSURE_EVAL_FUNCTION_2(name, t0, t1) CLOSURE_EVAL_FUNCTION(name, t0, t1, Dummy, Dummy)
#define CLOSURE_EVAL_FUNCTION_3(name, t0, t1, t2) CLOSURE_EVAL_FUNCTION(name, t0, t1, t2, Dummy)
#define CLOSURE_EVAL_FUNCTION_4(name, t0, t1, t2, t3) CLOSURE_EVAL_FUNCTION(name, t0, t1, t2, t3)

/* -------------------------------------------------------------------- */
/** \name Dummy Closure
 *
 * Dummy closure type that will be optimized out by the compiler.
 * \{ */

#define ClosureInputDummy ClosureOutput
#define ClosureOutputDummy ClosureOutput
#define ClosureEvalDummy ClosureOutput
#define CLOSURE_EVAL_DUMMY ClosureOutput(vec3(0))
#define CLOSURE_INPUT_Dummy_DEFAULT CLOSURE_EVAL_DUMMY
#define closure_Dummy_eval_init(cl_in, cl_common, cl_out) CLOSURE_EVAL_DUMMY
#define closure_Dummy_planar_eval(cl_in, cl_eval, cl_common, data, cl_out)
#define closure_Dummy_cubemap_eval(cl_in, cl_eval, cl_common, data, cl_out)
#define closure_Dummy_grid_eval(cl_in, cl_eval, cl_common, data, cl_out)
#define closure_Dummy_indirect_end(cl_in, cl_eval, cl_common, cl_out)
#define closure_Dummy_light_eval(cl_in, cl_eval, cl_common, data, cl_out)
#define closure_Dummy_eval_end(cl_in, cl_eval, cl_common, cl_out)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common cl_eval data
 *
 * Eval data not dependant on input parameters. All might not be used but unused ones
 * will be optimized out.
 * \{ */

struct ClosureInputCommon {
  /** Custom occlusion value set by the user. */
  float occlusion;
};

#define CLOSURE_INPUT_COMMON_DEFAULT ClosureInputCommon(1.0)

struct ClosureEvalCommon {
  /** Result of SSAO. */
  OcclusionData occlusion_data;
  /** View vector. */
  vec3 V;
  /** Surface position. */
  vec3 P;
  /** Normal vector, always facing camera. */
  vec3 N;
  /** Normal vector, always facing camera. (viewspace) */
  vec3 vN;
  /** Surface position. (viewspace) */
  vec3 vP;
  /** Geometric normal, always facing camera. */
  vec3 Ng;
  /** Geometric normal, always facing camera. (viewspace) */
  vec3 vNg;
  /** Random numbers. 3 random sequences. zw is a random point on a circle. */
  vec4 rand;
  /** Specular probe accumulator. Shared between planar and cubemap probe. */
  float specular_accum;
  /** Diffuse probe accumulator. */
  float diffuse_accum;
};

/* Common cl_out struct used by most closures. */
struct ClosureOutput {
  vec3 radiance;
};

/* Workaround for screenspace shadows in SSR pass. */
float FragDepth;

ClosureEvalCommon closure_Common_eval_init(ClosureInputCommon cl_in)
{
  ClosureEvalCommon cl_eval;
  cl_eval.rand = texelfetch_noise_tex(gl_FragCoord.xy);
  cl_eval.V = cameraVec(worldPosition);
  cl_eval.P = worldPosition;
  cl_eval.N = safe_normalize(gl_FrontFacing ? worldNormal : -worldNormal);
  cl_eval.vN = safe_normalize(gl_FrontFacing ? viewNormal : -viewNormal);
  cl_eval.vP = viewPosition;
  cl_eval.Ng = safe_normalize(cross(dFdx(cl_eval.P), dFdy(cl_eval.P)));
  cl_eval.vNg = transform_direction(ViewMatrix, cl_eval.Ng);

  cl_eval.occlusion_data = occlusion_load(cl_eval.vP, cl_in.occlusion);

  cl_eval.specular_accum = 1.0;
  cl_eval.diffuse_accum = 1.0;
  return cl_eval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop data
 *
 * Loop datas are conveniently packed into struct to make it future proof.
 * \{ */

struct ClosureLightData {
  LightData data; /** Light Data. */
  vec4 L;         /** Non-Normalized Light Vector (surface to light) with length in W component. */
  float vis;      /** Light visibility. */
  float contact_shadow; /** Result of contact shadow tracing. */
};

ClosureLightData closure_light_eval_init(ClosureEvalCommon cl_common, int light_id)
{
  ClosureLightData light;
  light.data = lights_data[light_id];

  light.L.xyz = light.data.l_position - cl_common.P;
  light.L.w = length(light.L.xyz);

  light.vis = light_visibility(light.data, cl_common.P, light.L);
  light.contact_shadow = light_contact_shadows(
      light.data, cl_common.P, cl_common.vP, cl_common.vNg, cl_common.rand.x, light.vis);

  return light;
}

struct ClosureCubemapData {
  int id;            /** Probe id. */
  float attenuation; /** Attenuation. */
};

ClosureCubemapData closure_cubemap_eval_init(int cube_id, inout ClosureEvalCommon cl_common)
{
  ClosureCubemapData cube;
  cube.id = cube_id;
  cube.attenuation = probe_attenuation_cube(cube_id, cl_common.P);
  cube.attenuation = min(cube.attenuation, cl_common.specular_accum);
  cl_common.specular_accum -= cube.attenuation;
  return cube;
}

struct ClosurePlanarData {
  int id;            /** Probe id. */
  PlanarData data;   /** planars_data[id]. */
  float attenuation; /** Attenuation. */
};

ClosurePlanarData closure_planar_eval_init(inout ClosureEvalCommon cl_common)
{
  ClosurePlanarData planar;
  planar.attenuation = 0.0;

  /* Find planar with the maximum weight. TODO(fclem)  */
  for (int i = 0; i < prbNumPlanar && i < MAX_PLANAR; i++) {
    float attenuation = probe_attenuation_planar(planars_data[i], cl_common.P);
    if (attenuation > planar.attenuation) {
      planar.id = i;
      planar.attenuation = attenuation;
      planar.data = planars_data[i];
    }
  }
  return planar;
}

struct ClosureGridData {
  int id;            /** Grid id. */
  GridData data;     /** grids_data[id] */
  float attenuation; /** Attenuation. */
  vec3 local_pos;    /** Local position inside the grid. */
};

ClosureGridData closure_grid_eval_init(int id, inout ClosureEvalCommon cl_common)
{
  ClosureGridData grid;
  grid.id = id;
  grid.data = grids_data[id];
  grid.attenuation = probe_attenuation_grid(grid.data, cl_common.P, grid.local_pos);
  grid.attenuation = min(grid.attenuation, cl_common.diffuse_accum);
  cl_common.diffuse_accum -= grid.attenuation;
  return grid;
}

/** \} */
