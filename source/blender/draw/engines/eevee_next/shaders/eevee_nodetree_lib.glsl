/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(draw_model_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_renderpass_lib.glsl)

#define filmScalingFactor float(uniform_buf.film.scaling_factor)

vec3 g_emission;
vec3 g_transmittance;
float g_holdout;

vec3 g_volume_scattering;
float g_volume_anisotropy;
vec3 g_volume_absorption;

/* The Closure type is never used. Use float as dummy type. */
#define Closure float
#define CLOSURE_DEFAULT 0.0

/* Maximum number of picked closure. */
#ifndef CLOSURE_BIN_COUNT
#  define CLOSURE_BIN_COUNT 1
#endif
/* Sampled closure parameters. */
ClosureUndetermined g_closure_bins[CLOSURE_BIN_COUNT];
/* Random number per sampled closure type. */
float g_closure_rand[CLOSURE_BIN_COUNT];

ClosureUndetermined g_closure_get(int i)
{
  switch (i) {
    default:
    case 0:
      return g_closure_bins[0];
#if CLOSURE_BIN_COUNT > 1
    case 1:
      return g_closure_bins[1];
#endif
#if CLOSURE_BIN_COUNT > 2
    case 2:
      return g_closure_bins[2];
#endif
  }
}

ClosureUndetermined g_closure_get_resolved(int i, float weight_fac)
{
  ClosureUndetermined cl = g_closure_get(i);
  cl.color *= cl.weight * weight_fac;
  return cl;
}

ClosureType closure_type_get(ClosureDiffuse cl)
{
  return CLOSURE_BSDF_DIFFUSE_ID;
}

ClosureType closure_type_get(ClosureTranslucent cl)
{
  return CLOSURE_BSDF_TRANSLUCENT_ID;
}

ClosureType closure_type_get(ClosureReflection cl)
{
  return CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
}

ClosureType closure_type_get(ClosureRefraction cl)
{
  return CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
}

ClosureType closure_type_get(ClosureSubsurface cl)
{
  return CLOSURE_BSSRDF_BURLEY_ID;
}

/**
 * Returns true if the closure is to be selected based on the input weight.
 */
bool closure_select_check(float weight, inout float total_weight, inout float r)
{
  if (weight < 1e-5) {
    return false;
  }
  total_weight += weight;
  float x = weight / total_weight;
  bool chosen = (r < x);
  /* Assuming that if r is in the interval [0,x] or [x,1], it's still uniformly distributed within
   * that interval, so remapping to [0,1] again to explore this space of probability. */
  r = (chosen) ? (r / x) : ((r - x) / (1.0 - x));
  return chosen;
}

/**
 * Assign `candidate` to `destination` based on a random value and the respective weights.
 */
void closure_select(inout ClosureUndetermined destination,
                    inout float random,
                    ClosureUndetermined candidate)
{
  if (closure_select_check(candidate.weight, destination.weight, random)) {
    float tmp = destination.weight;
    destination = candidate;
    destination.weight = tmp;
  }
}

void closure_weights_reset(float closure_rand)
{
  g_closure_rand[0] = closure_rand;
  g_closure_bins[0].weight = 0.0;
#if CLOSURE_BIN_COUNT > 1
  g_closure_rand[1] = closure_rand;
  g_closure_bins[1].weight = 0.0;
#endif
#if CLOSURE_BIN_COUNT > 2
  g_closure_rand[2] = closure_rand;
  g_closure_bins[2].weight = 0.0;
#endif

  g_volume_scattering = vec3(0.0);
  g_volume_anisotropy = 0.0;
  g_volume_absorption = vec3(0.0);

  g_emission = vec3(0.0);
  g_transmittance = vec3(0.0);
  g_volume_scattering = vec3(0.0);
  g_volume_absorption = vec3(0.0);
  g_holdout = 0.0;
}

#define closure_base_copy(cl, in_cl) \
  cl.weight = in_cl.weight; \
  cl.color = in_cl.color; \
  cl.N = in_cl.N; \
  cl.type = closure_type_get(in_cl);

/* Single BSDFs. */
Closure closure_eval(ClosureDiffuse diffuse)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, diffuse);
#if (CLOSURE_BIN_COUNT > 1) && defined(MAT_TRANSLUCENT) && !defined(MAT_CLEARCOAT)
  /* Use second slot so we can have diffuse + translucent without noise. */
  closure_select(g_closure_bins[1], g_closure_rand[1], cl);
#else
  /* Either is single closure or use same bin as transmission bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
#endif
  return Closure(0);
}

Closure closure_eval(ClosureSubsurface diffuse)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, diffuse);
  cl.data.rgb = diffuse.sss_radius;
  /* Transmission Closures are always in first bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
  return Closure(0);
}

Closure closure_eval(ClosureTranslucent translucent)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, translucent);
  /* Transmission Closures are always in first bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
  return Closure(0);
}

/* Alternate between two bins on a per closure basis.
 * Allow clearcoat layer without noise.
 * Choosing the bin with the least weight can choose a
 * different bin for the same closure and
 * produce issue with ray-tracing denoiser.
 * Always start with the second bin, this one doesn't
 * overlap with other closure. */
bool g_closure_reflection_bin = true;
#define CHOOSE_MIN_WEIGHT_CLOSURE_BIN(a, b) \
  if (g_closure_reflection_bin) { \
    closure_select(g_closure_bins[b], g_closure_rand[b], cl); \
  } \
  else { \
    closure_select(g_closure_bins[a], g_closure_rand[a], cl); \
  } \
  g_closure_reflection_bin = !g_closure_reflection_bin;

Closure closure_eval(ClosureReflection reflection)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, reflection);
  cl.data.r = reflection.roughness;

#ifdef MAT_CLEARCOAT
#  if CLOSURE_BIN_COUNT == 2
  /* Multiple reflection closures. */
  CHOOSE_MIN_WEIGHT_CLOSURE_BIN(0, 1);
#  elif CLOSURE_BIN_COUNT == 3
  /* Multiple reflection closures and one other closure. */
  CHOOSE_MIN_WEIGHT_CLOSURE_BIN(1, 2);
#  else
#    error Clearcoat should always have at least 2 bins
#  endif
#else
#  if CLOSURE_BIN_COUNT == 1
  /* Only one reflection closure is present in the whole tree. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
#  elif CLOSURE_BIN_COUNT == 2
  /* Only one reflection and one other closure. */
  closure_select(g_closure_bins[1], g_closure_rand[1], cl);
#  elif CLOSURE_BIN_COUNT == 3
  /* Only one reflection and two other closures. */
  closure_select(g_closure_bins[2], g_closure_rand[2], cl);
#  endif
#endif

#undef CHOOSE_MIN_WEIGHT_CLOSURE_BIN

  return Closure(0);
}

Closure closure_eval(ClosureRefraction refraction)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, refraction);
  cl.data.r = refraction.roughness;
  cl.data.g = refraction.ior;
  /* Transmission Closures are always in first bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
  return Closure(0);
}

Closure closure_eval(ClosureEmission emission)
{
  g_emission += emission.emission * emission.weight;
  return Closure(0);
}

Closure closure_eval(ClosureTransparency transparency)
{
  g_transmittance += transparency.transmittance * transparency.weight;
  g_holdout += transparency.holdout * transparency.weight;
  return Closure(0);
}

Closure closure_eval(ClosureVolumeScatter volume_scatter)
{
  g_volume_scattering += volume_scatter.scattering * volume_scatter.weight;
  g_volume_anisotropy += volume_scatter.anisotropy * volume_scatter.weight;
  return Closure(0);
}

Closure closure_eval(ClosureVolumeAbsorption volume_absorption)
{
  g_volume_absorption += volume_absorption.absorption * volume_absorption.weight;
  return Closure(0);
}

Closure closure_eval(ClosureHair hair)
{
  /* TODO */
  return Closure(0);
}

/* Glass BSDF. */
Closure closure_eval(ClosureReflection reflection, ClosureRefraction refraction)
{
  closure_eval(reflection);
  closure_eval(refraction);
  return Closure(0);
}

/* Dielectric BSDF. */
Closure closure_eval(ClosureDiffuse diffuse, ClosureReflection reflection)
{
  closure_eval(diffuse);
  closure_eval(reflection);
  return Closure(0);
}

/* Coat BSDF. */
Closure closure_eval(ClosureReflection reflection, ClosureReflection coat)
{
  closure_eval(reflection);
  closure_eval(coat);
  return Closure(0);
}

/* Volume BSDF. */
Closure closure_eval(ClosureVolumeScatter volume_scatter,
                     ClosureVolumeAbsorption volume_absorption,
                     ClosureEmission emission)
{
  closure_eval(volume_scatter);
  closure_eval(volume_absorption);
  closure_eval(emission);
  return Closure(0);
}

/* Specular BSDF. */
Closure closure_eval(ClosureDiffuse diffuse, ClosureReflection reflection, ClosureReflection coat)
{
  closure_eval(diffuse);
  closure_eval(reflection);
  closure_eval(coat);
  return Closure(0);
}

/* Principled BSDF. */
Closure closure_eval(ClosureDiffuse diffuse,
                     ClosureReflection reflection,
                     ClosureReflection coat,
                     ClosureRefraction refraction)
{
  closure_eval(diffuse);
  closure_eval(reflection);
  closure_eval(coat);
  closure_eval(refraction);
  return Closure(0);
}

/* NOP since we are sampling closures. */
Closure closure_add(Closure cl1, Closure cl2)
{
  return Closure(0);
}
Closure closure_mix(Closure cl1, Closure cl2, float fac)
{
  return Closure(0);
}

float ambient_occlusion_eval(vec3 normal,
                             float max_distance,
                             const float inverted,
                             const float sample_count)
{
  /* Avoid multi-line pre-processor conditionals.
   * Some drivers don't handle them correctly. */
  // clang-format off
#if defined(GPU_FRAGMENT_SHADER) && defined(MAT_AMBIENT_OCCLUSION) && !defined(MAT_DEPTH) && !defined(MAT_SHADOW)
  // clang-format on
#  if 0 /* TODO(fclem): Finish inverted horizon scan. */
  /* TODO(fclem): Replace eevee_ambient_occlusion_lib by eevee_horizon_scan_eval_lib when this is
   * finished. */
  vec3 vP = drw_point_world_to_view(g_data.P);
  vec3 vN = drw_normal_world_to_view(normal);

  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec2 noise;
  noise.x = interlieved_gradient_noise(vec2(texel), 3.0, 0.0);
  noise.y = utility_tx_fetch(utility_tx, vec2(texel), UTIL_BLUE_NOISE_LAYER).r;
  noise = fract(noise + sampling_rng_2D_get(SAMPLING_AO_U));

  ClosureOcclusion occlusion;
  occlusion.N = (inverted != 0.0) ? -vN : vN;

  HorizonScanContext ctx;
  ctx.occlusion = occlusion;

  horizon_scan_eval(vP,
                    ctx,
                    noise,
                    uniform_buf.ao.pixel_size,
                    max_distance,
                    uniform_buf.ao.thickness,
                    uniform_buf.ao.angle_bias,
                    10,
                    inverted != 0.0);

  return saturate(ctx.occlusion_result.r);
#  else
  vec3 vP = drw_point_world_to_view(g_data.P);
  ivec2 texel = ivec2(gl_FragCoord.xy);
  OcclusionData data = ambient_occlusion_search(
      vP, hiz_tx, texel, max_distance, inverted, sample_count);

  vec3 V = drw_world_incident_vector(g_data.P);
  vec3 N = normal;
  vec3 Ng = g_data.Ng;

  float unused_error, visibility;
  vec3 unused;
  ambient_occlusion_eval(data, texel, V, N, Ng, inverted, visibility, unused_error, unused);
  return visibility;
#  endif
#else
  return 1.0;
#endif
}

#ifndef GPU_METAL
void attrib_load();
Closure nodetree_surface(float closure_rand);
Closure nodetree_volume();
vec3 nodetree_displacement();
float nodetree_thickness();
vec4 closure_to_rgba(Closure cl);
#endif

/* Simplified form of F_eta(eta, 1.0). */
float F0_from_ior(float eta)
{
  float A = (eta - 1.0) / (eta + 1.0);
  return A * A;
}

/* Return the fresnel color from a precomputed LUT value (from brdf_lut). */
vec3 F_brdf_single_scatter(vec3 f0, vec3 f90, vec2 lut)
{
  return f0 * lut.x + f90 * lut.y;
}

/* Multi-scattering brdf approximation from
 * "A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting"
 * https://jcgt.org/published/0008/01/03/paper.pdf by Carmelo J. Fdez-Agüera. */
vec3 F_brdf_multi_scatter(vec3 f0, vec3 f90, vec2 lut)
{
  vec3 FssEss = F_brdf_single_scatter(f0, f90, lut);

  float Ess = lut.x + lut.y;
  float Ems = 1.0 - Ess;
  vec3 Favg = f0 + (f90 - f0) / 21.0;

  /* The original paper uses `FssEss * radiance + Fms*Ems * irradiance`, but
   * "A Journey Through Implementing Multi-scattering BRDFs and Area Lights" by Steve McAuley
   * suggests to use `FssEss * radiance + Fms*Ems * radiance` which results in comparable quality.
   * We handle `radiance` outside of this function, so the result simplifies to:
   * `FssEss + Fms*Ems = FssEss * (1 + Ems*Favg / (1 - Ems*Favg)) = FssEss / (1 - Ems*Favg)`.
   * This is a simple albedo scaling very similar to the approach used by Cycles:
   * "Practical multiple scattering compensation for microfacet model". */
  return FssEss / (1.0 - Ems * Favg);
}

vec2 brdf_lut(float cos_theta, float roughness)
{
#ifdef EEVEE_UTILITY_TX
  return utility_tx_sample_lut(utility_tx, cos_theta, roughness, UTIL_BSDF_LAYER).rg;
#else
  return vec2(1.0, 0.0);
#endif
}

void brdf_f82_tint_lut(vec3 F0,
                       vec3 F82,
                       float cos_theta,
                       float roughness,
                       bool do_multiscatter,
                       out vec3 reflectance)
{
#ifdef EEVEE_UTILITY_TX
  vec3 split_sum = utility_tx_sample_lut(utility_tx, cos_theta, roughness, UTIL_BSDF_LAYER).rgb;
#else
  vec3 split_sum = vec3(1.0, 0.0, 0.0);
#endif

  reflectance = do_multiscatter ? F_brdf_multi_scatter(F0, vec3(1.0), split_sum.xy) :
                                  F_brdf_single_scatter(F0, vec3(1.0), split_sum.xy);

  /* Precompute the F82 term factor for the Fresnel model.
   * In the classic F82 model, the F82 input directly determines the value of the Fresnel
   * model at ~82°, similar to F0 and F90.
   * With F82-Tint, on the other hand, the value at 82° is the value of the classic Schlick
   * model multiplied by the tint input.
   * Therefore, the factor follows by setting `F82Tint(cosI) = FSchlick(cosI) - b*cosI*(1-cosI)^6`
   * and `F82Tint(acos(1/7)) = FSchlick(acos(1/7)) * f82_tint` and solving for `b`. */
  const float f = 6.0 / 7.0;
  const float f5 = (f * f) * (f * f) * f;
  const float f6 = (f * f) * (f * f) * (f * f);
  vec3 F_schlick = mix(F0, vec3(1.0), f5);
  vec3 b = F_schlick * (7.0 / f6) * (1.0 - F82);
  reflectance -= b * split_sum.z;
}

/* Return texture coordinates to sample BSDF LUT. */
vec3 lut_coords_bsdf(float cos_theta, float roughness, float ior)
{
  /* IOR is the sine of the critical angle. */
  float critical_cos = sqrt(1.0 - ior * ior);

  vec3 coords;
  coords.x = square(ior);
  coords.y = cos_theta;
  coords.y -= critical_cos;
  coords.y /= (coords.y > 0.0) ? (1.0 - critical_cos) : critical_cos;
  coords.y = coords.y * 0.5 + 0.5;
  coords.z = roughness;

  return saturate(coords);
}

/* Return texture coordinates to sample Surface LUT. */
vec3 lut_coords_btdf(float cos_theta, float roughness, float ior)
{
  return vec3(sqrt((ior - 1.0) / (ior + 1.0)), sqrt(1.0 - cos_theta), roughness);
}

/* Computes the reflectance and transmittance based on the tint (`f0`, `f90`, `transmission_tint`)
 * and the BSDF LUT. */
void bsdf_lut(vec3 F0,
              vec3 F90,
              vec3 transmission_tint,
              float cos_theta,
              float roughness,
              float ior,
              bool do_multiscatter,
              out vec3 reflectance,
              out vec3 transmittance)
{
#ifdef EEVEE_UTILITY_TX
  if (ior == 1.0) {
    reflectance = vec3(0.0);
    transmittance = transmission_tint;
    return;
  }

  vec2 split_sum;
  float transmission_factor;

  if (ior > 1.0) {
    split_sum = brdf_lut(cos_theta, roughness);
    vec3 coords = lut_coords_btdf(cos_theta, roughness, ior);
    transmission_factor = utility_tx_sample_bsdf_lut(utility_tx, coords.xy, coords.z).a;
    /* Gradually increase `f90` from 0 to 1 when IOR is in the range of [1.0, 1.33], to avoid harsh
     * transition at `IOR == 1`. */
    if (all(equal(F90, vec3(1.0)))) {
      F90 = vec3(saturate(2.33 / 0.33 * (ior - 1.0) / (ior + 1.0)));
    }
  }
  else {
    vec3 coords = lut_coords_bsdf(cos_theta, roughness, ior);
    vec3 bsdf = utility_tx_sample_bsdf_lut(utility_tx, coords.xy, coords.z).rgb;
    split_sum = bsdf.rg;
    transmission_factor = bsdf.b;
  }

  reflectance = F_brdf_single_scatter(F0, F90, split_sum);
  transmittance = (vec3(1.0) - F0) * transmission_factor * transmission_tint;

  if (do_multiscatter) {
    float real_F0 = F0_from_ior(ior);
    float Ess = real_F0 * split_sum.x + split_sum.y + (1.0 - real_F0) * transmission_factor;
    float Ems = 1.0 - Ess;
    /* Assume that the transmissive tint makes up most of the overall color if it's not zero. */
    vec3 Favg = all(equal(transmission_tint, vec3(0.0))) ? F0 + (F90 - F0) / 21.0 :
                                                           transmission_tint;

    vec3 scale = 1.0 / (1.0 - Ems * Favg);
    reflectance *= scale;
    transmittance *= scale;
  }
#else
  reflectance = vec3(0.0);
  transmittance = vec3(0.0);
#endif
  return;
}

/* Computes the reflectance and transmittance based on the BSDF LUT. */
vec2 bsdf_lut(float cos_theta, float roughness, float ior, bool do_multiscatter)
{
  float F0 = F0_from_ior(ior);
  vec3 color = vec3(1.0);
  vec3 reflectance, transmittance;
  bsdf_lut(vec3(F0),
           color,
           color,
           cos_theta,
           roughness,
           ior,
           do_multiscatter,
           reflectance,
           transmittance);
  return vec2(reflectance.r, transmittance.r);
}

#ifdef EEVEE_MATERIAL_STUBS
#  define attrib_load()
#  define nodetree_displacement() vec3(0.0)
#  define nodetree_surface(closure_rand) Closure(0)
#  define nodetree_volume() Closure(0)
#  define nodetree_thickness() 0.1
#  define thickness_mode 1.0
#endif

#ifdef GPU_VERTEX_SHADER
#  define closure_to_rgba(a) vec4(0.0)
#endif

/* -------------------------------------------------------------------- */
/** \name Fragment Displacement
 *
 * Displacement happening in the fragment shader.
 * Can be used in conjunction with a per vertex displacement.
 *
 * \{ */

#ifdef MAT_DISPLACEMENT_BUMP
/* Return new shading normal. */
vec3 displacement_bump()
{
#  if defined(GPU_FRAGMENT_SHADER) && !defined(MAT_GEOM_CURVES)
  vec2 dHd;
  dF_branch(dot(nodetree_displacement(), g_data.N + dF_impl(g_data.N)), dHd);

  vec3 dPdx = dFdx(g_data.P);
  vec3 dPdy = dFdy(g_data.P);

  /* Get surface tangents from normal. */
  vec3 Rx = cross(dPdy, g_data.N);
  vec3 Ry = cross(g_data.N, dPdx);

  /* Compute surface gradient and determinant. */
  float det = dot(dPdx, Rx);

  vec3 surfgrad = dHd.x * Rx + dHd.y * Ry;

  float facing = FrontFacing ? 1.0 : -1.0;
  return normalize(abs(det) * g_data.N - facing * sign(det) * surfgrad);
#  else
  return g_data.N;
#  endif
}
#endif

void fragment_displacement()
{
#ifdef MAT_DISPLACEMENT_BUMP
  g_data.N = displacement_bump();
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Coordinate implementations
 *
 * Callbacks for the texture coordinate node.
 *
 * \{ */

vec3 coordinate_camera(vec3 P)
{
  vec3 vP;
  if (false /* Probe. */) {
    /* Unsupported. It would make the probe camera-dependent. */
    vP = P;
  }
  else {
#ifdef MAT_GEOM_WORLD
    vP = drw_normal_world_to_view(P);
#else
    vP = drw_point_world_to_view(P);
#endif
  }
  vP.z = -vP.z;
  return vP;
}

vec3 coordinate_screen(vec3 P)
{
  vec3 window = vec3(0.0);
  if (false /* Probe. */) {
    /* Unsupported. It would make the probe camera-dependent. */
    window.xy = vec2(0.5);
  }
  else {
#ifdef MAT_GEOM_WORLD
    window.xy = drw_point_view_to_screen(interp.P).xy;
#else
    /* TODO(fclem): Actual camera transform. */
    window.xy = drw_point_world_to_screen(P).xy;
#endif
    window.xy = window.xy * uniform_buf.camera.uv_scale + uniform_buf.camera.uv_bias;
  }
  return window;
}

vec3 coordinate_reflect(vec3 P, vec3 N)
{
#ifdef MAT_GEOM_WORLD
  return N;
#else
  return -reflect(drw_world_incident_vector(P), N);
#endif
}

vec3 coordinate_incoming(vec3 P)
{
#ifdef MAT_GEOM_WORLD
  return -P;
#else
  return drw_world_incident_vector(P);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mixed render resolution
 *
 * Callbacks image texture sampling.
 *
 * \{ */

float film_scaling_factor_get()
{
  return float(uniform_buf.film.scaling_factor);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Attribute post
 *
 * TODO(@fclem): These implementation details should concern the DRWManager and not be a fix on
 * the engine side. But as of now, the engines are responsible for loading the attributes.
 *
 * \{ */

/* Point clouds and curves are not compatible with volume grids.
 * They will fallback to their own attributes loading. */
#if defined(MAT_VOLUME) && !defined(MAT_GEOM_CURVES) && !defined(MAT_GEOM_POINT_CLOUD)
#  if defined(OBINFO_LIB) && !defined(MAT_GEOM_WORLD)
/* We could just check for GRID_ATTRIBUTES but this avoids for header dependency. */
#    define GRID_ATTRIBUTES_LOAD_POST
#  endif
#endif

float attr_load_temperature_post(float attr)
{
#ifdef GRID_ATTRIBUTES_LOAD_POST
  /* Bring the into standard range without having to modify the grid values */
  attr = (attr > 0.01) ? (attr * drw_volume.temperature_mul + drw_volume.temperature_bias) : 0.0;
#endif
  return attr;
}
vec4 attr_load_color_post(vec4 attr)
{
#ifdef GRID_ATTRIBUTES_LOAD_POST
  /* Density is premultiplied for interpolation, divide it out here. */
  attr.rgb *= safe_rcp(attr.a);
  attr.rgb *= drw_volume.color_mul.rgb;
  attr.a = 1.0;
#endif
  return attr;
}

#undef GRID_ATTRIBUTES_LOAD_POST

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Attributes
 *
 * TODO(@fclem): These implementation details should concern the DRWManager and not be a fix on
 * the engine side. But as of now, the engines are responsible for loading the attributes.
 *
 * \{ */

vec4 attr_load_uniform(vec4 attr, const uint attr_hash)
{
#if defined(OBATTR_LIB)
  uint index = floatBitsToUint(ObjectAttributeStart);
  for (uint i = 0; i < floatBitsToUint(ObjectAttributeLen); i++, index++) {
    if (drw_attrs[index].hash_code == attr_hash) {
      return vec4(drw_attrs[index].data_x,
                  drw_attrs[index].data_y,
                  drw_attrs[index].data_z,
                  drw_attrs[index].data_w);
    }
  }
  return vec4(0.0);
#else
  return attr;
#endif
}

/** \} */
