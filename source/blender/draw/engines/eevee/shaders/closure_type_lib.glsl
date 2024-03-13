/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
/* #pragma (common_math_geom_lib.glsl) */
/* #pragma (common_uniforms_lib.glsl) */
/* #pragma (renderpass_lib.glsl) */

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
#endif

/* Metal Default Constructor - Required for C++ constructor syntax. */
#ifdef GPU_METAL
  inline Closure() = default;
#  ifdef VOLUMETRICS
  /* Explicit Closure constructors -- To support GLSL syntax */
  inline Closure(vec3 in_absorption, vec3 in_scatter, vec3 in_emission, float in_anisotropy)
      : absorption(in_absorption),
        scatter(in_scatter),
        emission(in_emission),
        anisotropy(in_anisotropy)
  {
  }
#  else
  /* Explicit Closure constructors -- To support GLSL syntax */
  inline Closure(vec3 in_radiance, vec3 in_transmittance, float in_holdout)
      : radiance(in_radiance), transmittance(in_transmittance), holdout(in_holdout)
  {
  }
#  endif /* VOLUMETRICS */
#endif   /* GPU_METAL */
};

#ifndef GPU_METAL
/* Prototype */
Closure nodetree_exec();
vec4 closure_to_rgba(Closure cl);
void output_aov(vec4 color, float value, uint hash);
vec3 coordinate_camera(vec3 P);
vec3 coordinate_screen(vec3 P);
vec3 coordinate_reflect(vec3 P, vec3 N);
vec3 coordinate_incoming(vec3 P);
float film_scaling_factor_get();

/* Single BSDFs. */
Closure closure_eval(ClosureDiffuse diffuse);
Closure closure_eval(ClosureSubsurface diffuse);
Closure closure_eval(ClosureTranslucent translucent);
Closure closure_eval(ClosureReflection reflection);
Closure closure_eval(ClosureRefraction refraction);
Closure closure_eval(ClosureEmission emission);
Closure closure_eval(ClosureTransparency transparency);
Closure closure_eval(ClosureVolumeScatter volume_scatter);
Closure closure_eval(ClosureVolumeAbsorption volume_absorption);
Closure closure_eval(ClosureHair hair);

/* Glass BSDF. */
Closure closure_eval(ClosureReflection reflection, ClosureRefraction refraction);
/* Dielectric BSDF. */
Closure closure_eval(ClosureSubsurface diffuse, ClosureReflection reflection);
/* Coat BSDF. */
Closure closure_eval(ClosureReflection reflection, ClosureReflection coat);
/* Volume BSDF. */
Closure closure_eval(ClosureVolumeScatter volume_scatter,
                     ClosureVolumeAbsorption volume_absorption,
                     ClosureEmission emission);
/* Specular BSDF. */
Closure closure_eval(ClosureSubsurface diffuse,
                     ClosureReflection reflection,
                     ClosureReflection coat);
/* Principled BSDF. */
Closure closure_eval(ClosureSubsurface diffuse,
                     ClosureReflection reflection,
                     ClosureReflection coat,
                     ClosureRefraction refraction);

Closure closure_add(inout Closure cl1, inout Closure cl2);
Closure closure_mix(inout Closure cl1, inout Closure cl2, float fac);

float ambient_occlusion_eval(vec3 normal,
                             float distance,
                             const float inverted,
                             const float sample_count);

/* WORKAROUND: Included later with libraries. This is because we are mixing include systems. */
vec3 safe_normalize(vec3 N);
float fast_sqrt(float a);
vec3 cameraVec(vec3 P);
vec2 bsdf_lut(float a, float b, float c, bool d);
void bsdf_lut(vec3 F0,
              vec3 F90,
              vec3 transmission_tint,
              float cos_theta,
              float roughness,
              float ior,
              bool do_multiscatter,
              out vec3 reflectance,
              out vec3 transmittance);
vec2 brdf_lut(float a, float b);
void brdf_f82_tint_lut(vec3 F0,
                       vec3 F82,
                       float cos_theta,
                       float roughness,
                       bool do_multiscatter,
                       out vec3 reflectance);
vec3 F_brdf_multi_scatter(vec3 a, vec3 b, vec2 c);
vec3 F_brdf_single_scatter(vec3 a, vec3 b, vec2 c);
float F_eta(float a, float b);
float F0_from_ior(float a);
#endif

#ifdef VOLUMETRICS
#  define CLOSURE_DEFAULT Closure(vec3(0), vec3(0), vec3(0), 0.0)
#else
#  define CLOSURE_DEFAULT Closure(vec3(0), vec3(0), 0.0)
#endif
