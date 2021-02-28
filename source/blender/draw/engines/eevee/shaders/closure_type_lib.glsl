
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(renderpass_lib.glsl)

#ifndef VOLUMETRICS

uniform int outputSsrId = 1;
uniform int outputSssId = 1;

#endif

struct Closure {
#ifdef VOLUMETRICS
  vec3 absorption;
  vec3 scatter;
  vec3 emission;
  float anisotropy;

#else /* SURFACE */
  vec3 radiance;
  vec3 transmittance;
  float holdout;
  vec4 ssr_data;
  vec2 ssr_normal;
  int flag;
#  ifdef USE_SSS
  vec3 sss_irradiance;
  vec3 sss_albedo;
  float sss_radius;
#  endif

#endif
};

/* Prototype */
Closure nodetree_exec(void);

/* clang-format off */
/* Avoid multi-line defines. */
#ifdef VOLUMETRICS
#  define CLOSURE_DEFAULT Closure(vec3(0), vec3(0), vec3(0), 0.0)
#elif !defined(USE_SSS)
#  define CLOSURE_DEFAULT Closure(vec3(0), vec3(0), 0.0, vec4(0), vec2(0), 0)
#else
#  define CLOSURE_DEFAULT Closure(vec3(0), vec3(0), 0.0, vec4(0), vec2(0), 0, vec3(0), vec3(0), 0.0)
#endif
/* clang-format on */

#define FLAG_TEST(flag, val) (((flag) & (val)) != 0)

#define CLOSURE_SSR_FLAG 1
#define CLOSURE_SSS_FLAG 2
#define CLOSURE_HOLDOUT_FLAG 4

#ifdef VOLUMETRICS
Closure closure_mix(Closure cl1, Closure cl2, float fac)
{
  Closure cl;
  cl.absorption = mix(cl1.absorption, cl2.absorption, fac);
  cl.scatter = mix(cl1.scatter, cl2.scatter, fac);
  cl.emission = mix(cl1.emission, cl2.emission, fac);
  cl.anisotropy = mix(cl1.anisotropy, cl2.anisotropy, fac);
  return cl;
}

Closure closure_add(Closure cl1, Closure cl2)
{
  Closure cl;
  cl.absorption = cl1.absorption + cl2.absorption;
  cl.scatter = cl1.scatter + cl2.scatter;
  cl.emission = cl1.emission + cl2.emission;
  cl.anisotropy = (cl1.anisotropy + cl2.anisotropy) / 2.0; /* Average phase (no multi lobe) */
  return cl;
}

Closure closure_emission(vec3 rgb)
{
  Closure cl = CLOSURE_DEFAULT;
  cl.emission = rgb;
  return cl;
}

#else /* SURFACE */

Closure closure_mix(Closure cl1, Closure cl2, float fac)
{
  Closure cl;
  cl.holdout = mix(cl1.holdout, cl2.holdout, fac);

  if (FLAG_TEST(cl1.flag, CLOSURE_HOLDOUT_FLAG)) {
    fac = 1.0;
  }
  else if (FLAG_TEST(cl2.flag, CLOSURE_HOLDOUT_FLAG)) {
    fac = 0.0;
  }

  cl.transmittance = mix(cl1.transmittance, cl2.transmittance, fac);
  cl.radiance = mix(cl1.radiance, cl2.radiance, fac);
  cl.flag = cl1.flag | cl2.flag;
  cl.ssr_data = mix(cl1.ssr_data, cl2.ssr_data, fac);
  bool use_cl1_ssr = FLAG_TEST(cl1.flag, CLOSURE_SSR_FLAG);
  /* When mixing SSR don't blend roughness and normals but only specular (ssr_data.xyz).*/
  cl.ssr_data.w = (use_cl1_ssr) ? cl1.ssr_data.w : cl2.ssr_data.w;
  cl.ssr_normal = (use_cl1_ssr) ? cl1.ssr_normal : cl2.ssr_normal;

#  ifdef USE_SSS
  cl.sss_albedo = mix(cl1.sss_albedo, cl2.sss_albedo, fac);
  bool use_cl1_sss = FLAG_TEST(cl1.flag, CLOSURE_SSS_FLAG);
  /* It also does not make sense to mix SSS radius or irradiance. */
  cl.sss_radius = (use_cl1_sss) ? cl1.sss_radius : cl2.sss_radius;
  cl.sss_irradiance = (use_cl1_sss) ? cl1.sss_irradiance : cl2.sss_irradiance;
#  endif
  return cl;
}

Closure closure_add(Closure cl1, Closure cl2)
{
  Closure cl;
  cl.transmittance = cl1.transmittance + cl2.transmittance;
  cl.radiance = cl1.radiance + cl2.radiance;
  cl.holdout = cl1.holdout + cl2.holdout;
  cl.flag = cl1.flag | cl2.flag;
  cl.ssr_data = cl1.ssr_data + cl2.ssr_data;
  bool use_cl1_ssr = FLAG_TEST(cl1.flag, CLOSURE_SSR_FLAG);
  /* When mixing SSR don't blend roughness and normals.*/
  cl.ssr_data.w = (use_cl1_ssr) ? cl1.ssr_data.w : cl2.ssr_data.w;
  cl.ssr_normal = (use_cl1_ssr) ? cl1.ssr_normal : cl2.ssr_normal;

#  ifdef USE_SSS
  cl.sss_albedo = cl1.sss_albedo + cl2.sss_albedo;
  bool use_cl1_sss = FLAG_TEST(cl1.flag, CLOSURE_SSS_FLAG);
  /* It also does not make sense to mix SSS radius or irradiance. */
  cl.sss_radius = (use_cl1_sss) ? cl1.sss_radius : cl2.sss_radius;
  cl.sss_irradiance = (use_cl1_sss) ? cl1.sss_irradiance : cl2.sss_irradiance;
#  endif
  return cl;
}

Closure closure_emission(vec3 rgb)
{
  Closure cl = CLOSURE_DEFAULT;
  cl.radiance = rgb;
  return cl;
}

#endif

#ifndef VOLUMETRICS

/* Let radiance passthrough or replace it to get the BRDF and color
 * to applied to the SSR result. */
vec3 closure_mask_ssr_radiance(vec3 radiance, float ssr_id)
{
  return (ssrToggle && int(ssr_id) == outputSsrId) ? vec3(1.0) : radiance;
}

void closure_load_ssr_data(
    vec3 ssr_radiance, float roughness, vec3 N, float ssr_id, inout Closure cl)
{
  /* Still encode to avoid artifacts in the SSR pass. */
  vec3 vN = normalize(mat3(ViewMatrix) * N);
  cl.ssr_normal = normal_encode(vN, viewCameraVec(viewPosition));

  if (ssrToggle && int(ssr_id) == outputSsrId) {
    cl.ssr_data = vec4(ssr_radiance, roughness);
    cl.flag |= CLOSURE_SSR_FLAG;
  }
  else {
    cl.radiance += ssr_radiance;
  }
}

void closure_load_sss_data(
    float radius, vec3 sss_irradiance, vec3 sss_albedo, int sss_id, inout Closure cl)
{
#  ifdef USE_SSS
  if (sss_id == outputSssId) {
    cl.sss_irradiance = sss_irradiance;
    cl.sss_radius = radius;
    cl.sss_albedo = sss_albedo;
    cl.flag |= CLOSURE_SSS_FLAG;
    /* Irradiance will be convolved by SSSS pass. Do not add to radiance. */
    sss_irradiance = vec3(0);
  }
#  endif
  cl.radiance += render_pass_diffuse_mask(vec3(1), sss_irradiance) * sss_albedo;
}

#endif
