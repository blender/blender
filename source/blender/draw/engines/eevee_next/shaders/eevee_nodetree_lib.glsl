/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

vec3 g_emission;
vec3 g_transmittance;
float g_holdout;

/* The Closure type is never used. Use float as dummy type. */
#define Closure float
#define CLOSURE_DEFAULT 0.0

/* Sampled closure parameters. */
ClosureDiffuse g_diffuse_data;
ClosureReflection g_reflection_data;
ClosureRefraction g_refraction_data;
ClosureVolumeScatter g_volume_scatter_data;
ClosureVolumeAbsorption g_volume_absorption_data;
/* Random number per sampled closure type. */
float g_diffuse_rand;
float g_reflection_rand;
float g_refraction_rand;
float g_volume_scatter_rand;
float g_volume_absorption_rand;

/**
 * Returns true if the closure is to be selected based on the input weight.
 */
bool closure_select(float weight, inout float total_weight, inout float r)
{
  total_weight += weight;
  float x = weight / total_weight;
  bool chosen = (r < x);
  /* Assuming that if r is in the interval [0,x] or [x,1], it's still uniformly distributed within
   * that interval, so remapping to [0,1] again to explore this space of probability. */
  r = (chosen) ? (r / x) : ((r - x) / (1.0 - x));
  return chosen;
}

#define SELECT_CLOSURE(destination, random, candidate) \
  if (closure_select(candidate.weight, destination.weight, random)) { \
    float tmp = destination.weight; \
    destination = candidate; \
    destination.weight = tmp; \
  }

float g_closure_rand;

void closure_weights_reset()
{
  g_diffuse_data.weight = 0.0;
  g_diffuse_data.color = vec3(0.0);
  g_diffuse_data.N = vec3(0.0);
  g_diffuse_data.sss_radius = vec3(0.0);
  g_diffuse_data.sss_id = uint(0);

  g_reflection_data.weight = 0.0;
  g_reflection_data.color = vec3(0.0);
  g_reflection_data.N = vec3(0.0);
  g_reflection_data.roughness = 0.0;

  g_refraction_data.weight = 0.0;
  g_refraction_data.color = vec3(0.0);
  g_refraction_data.N = vec3(0.0);
  g_refraction_data.roughness = 0.0;
  g_refraction_data.ior = 0.0;

  g_volume_scatter_data.weight = 0.0;
  g_volume_scatter_data.scattering = vec3(0.0);
  g_volume_scatter_data.anisotropy = 0.0;

  g_volume_absorption_data.weight = 0.0;
  g_volume_absorption_data.absorption = vec3(0.0);

#if defined(GPU_FRAGMENT_SHADER)
  g_diffuse_rand = g_reflection_rand = g_refraction_rand = g_closure_rand;
  g_volume_scatter_rand = g_volume_absorption_rand = g_closure_rand;
#else
  g_diffuse_rand = 0.0;
  g_reflection_rand = 0.0;
  g_refraction_rand = 0.0;
  g_volume_scatter_rand = 0.0;
  g_volume_absorption_rand = 0.0;
#endif

  g_emission = vec3(0.0);
  g_transmittance = vec3(0.0);
  g_holdout = 0.0;
}

/* Single BSDFs. */
Closure closure_eval(ClosureDiffuse diffuse)
{
  SELECT_CLOSURE(g_diffuse_data, g_diffuse_rand, diffuse);
  return Closure(0);
}

Closure closure_eval(ClosureTranslucent translucent)
{
  /* TODO */
  return Closure(0);
}

Closure closure_eval(ClosureReflection reflection)
{
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, reflection);
  return Closure(0);
}

Closure closure_eval(ClosureRefraction refraction)
{
  SELECT_CLOSURE(g_refraction_data, g_refraction_rand, refraction);
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
  /* TODO: Combine instead of selecting. */
  SELECT_CLOSURE(g_volume_scatter_data, g_volume_scatter_rand, volume_scatter);
  return Closure(0);
}

Closure closure_eval(ClosureVolumeAbsorption volume_absorption)
{
  /* TODO: Combine instead of selecting. */
  SELECT_CLOSURE(g_volume_absorption_data, g_volume_absorption_rand, volume_absorption);
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
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, reflection);
  SELECT_CLOSURE(g_refraction_data, g_refraction_rand, refraction);
  return Closure(0);
}

/* Dielectric BSDF. */
Closure closure_eval(ClosureDiffuse diffuse, ClosureReflection reflection)
{
  SELECT_CLOSURE(g_diffuse_data, g_diffuse_rand, diffuse);
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, reflection);
  return Closure(0);
}

/* ClearCoat BSDF. */
Closure closure_eval(ClosureReflection reflection, ClosureReflection clearcoat)
{
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, reflection);
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, clearcoat);
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
Closure closure_eval(ClosureDiffuse diffuse,
                     ClosureReflection reflection,
                     ClosureReflection clearcoat)
{
  SELECT_CLOSURE(g_diffuse_data, g_diffuse_rand, diffuse);
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, reflection);
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, clearcoat);
  return Closure(0);
}

/* Principled BSDF. */
Closure closure_eval(ClosureDiffuse diffuse,
                     ClosureReflection reflection,
                     ClosureReflection clearcoat,
                     ClosureRefraction refraction)
{
  SELECT_CLOSURE(g_diffuse_data, g_diffuse_rand, diffuse);
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, reflection);
  SELECT_CLOSURE(g_reflection_data, g_reflection_rand, clearcoat);
  SELECT_CLOSURE(g_refraction_data, g_refraction_rand, refraction);
  return Closure(0);
}

/* Noop since we are sampling closures. */
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
  vec3 vP = transform_point(ViewMatrix, g_data.P);
  ivec2 texel = ivec2(gl_FragCoord.xy);
  OcclusionData data = ambient_occlusion_search(
      vP, hiz_tx, texel, max_distance, inverted, sample_count);

  vec3 V = cameraVec(g_data.P);
  vec3 N = g_data.N;
  vec3 Ng = g_data.Ng;

  float unused_error, visibility;
  vec3 unused;
  ambient_occlusion_eval(data, texel, V, N, Ng, inverted, visibility, unused_error, unused);
  return visibility;
#else
  return 1.0;
#endif
}

#ifndef GPU_METAL
void attrib_load();
Closure nodetree_surface();
/* Closure nodetree_volume(); */
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
   * "A Journey Through Implementing Multiscattering BRDFs and Area Lights" by Steve McAuley
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

vec2 btdf_lut(float cos_theta, float roughness, float ior, float do_multiscatter)
{
  if (ior <= 1e-5) {
    return vec2(0.0);
  }

  if (ior >= 1.0) {
    vec2 split_sum = brdf_lut(cos_theta, roughness);
    float f0 = F0_from_ior(ior);
    /* Gradually increase `f90` from 0 to 1 when IOR is in the range of [1.0, 1.33], to avoid harsh
     * transition at `IOR == 1`. */
    float f90 = fast_sqrt(saturate(f0 / 0.02));

    float brdf = F_brdf_multi_scatter(vec3(f0), vec3(f90), split_sum).r;
    /* Energy conservation. */
    float btdf = 1.0 - brdf;
    /* Assuming the energy loss caused by single-scattering is distributed proportionally in the
     * reflection and refraction lobes. */
    return vec2(btdf, brdf) * ((do_multiscatter == 0.0) ? sum(split_sum) : 1.0);
  }

  /* IOR is sin of critical angle. */
  float critical_cos = sqrt(1.0 - ior * ior);

  vec3 coords;
  coords.x = sqr(ior);
  coords.y = cos_theta;
  coords.y -= critical_cos;
  coords.y /= (coords.y > 0.0) ? (1.0 - critical_cos) : critical_cos;
  coords.y = coords.y * 0.5 + 0.5;
  coords.z = roughness;

  coords = saturate(coords);

  float layer = coords.z * UTIL_BTDF_LAYER_COUNT;
  float layer_floored = floor(layer);

#ifdef EEVEE_UTILITY_TX
  coords.z = UTIL_BTDF_LAYER + layer_floored;
  vec2 btdf_brdf_low = utility_tx_sample_lut(utility_tx, coords.xy, coords.z).rg;
  vec2 btdf_brdf_high = utility_tx_sample_lut(utility_tx, coords.xy, coords.z + 1.0).rg;
  /* Manual trilinear interpolation. */
  vec2 btdf_brdf = mix(btdf_brdf_low, btdf_brdf_high, layer - layer_floored);

  if (do_multiscatter != 0.0) {
    /* For energy-conserving BSDF the reflection and refraction lobes should sum to one. Assuming
     * the energy loss of single-scattering is distributed proportionally in the two lobes. */
    btdf_brdf /= (btdf_brdf.x + btdf_brdf.y);
  }

  return btdf_brdf;
#else
  return vec2(0.0);
#endif
}

void output_renderpass_color(int id, vec4 color)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  if (id >= 0) {
    ivec2 texel = ivec2(gl_FragCoord.xy);
    imageStore(rp_color_img, ivec3(texel, id), color);
  }
#endif
}

void output_renderpass_value(int id, float value)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  if (id >= 0) {
    ivec2 texel = ivec2(gl_FragCoord.xy);
    imageStore(rp_value_img, ivec3(texel, id), vec4(value));
  }
#endif
}

void clear_aovs()
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < rp_buf.aovs.color_len; i++) {
    output_renderpass_color(rp_buf.color_len + i, vec4(0));
  }
  for (int i = 0; i < AOV_MAX && i < rp_buf.aovs.value_len; i++) {
    output_renderpass_value(rp_buf.value_len + i, 0.0);
  }
#endif
}

void output_aov(vec4 color, float value, uint hash)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < rp_buf.aovs.color_len; i++) {
    if (rp_buf.aovs.hash_color[i].x == hash) {
      imageStore(rp_color_img, ivec3(ivec2(gl_FragCoord.xy), rp_buf.color_len + i), color);
      return;
    }
  }
  for (int i = 0; i < AOV_MAX && i < rp_buf.aovs.value_len; i++) {
    if (rp_buf.aovs.hash_value[i].x == hash) {
      imageStore(rp_value_img, ivec3(ivec2(gl_FragCoord.xy), rp_buf.value_len + i), vec4(value));
      return;
    }
  }
#endif
}

#ifdef EEVEE_MATERIAL_STUBS
#  define attrib_load()
#  define nodetree_displacement() vec3(0.0)
#  define nodetree_surface() Closure(0)
#  define nodetree_volume() Closure(0)
#  define nodetree_thickness() 0.1
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
  if (false /* probe */) {
    /* Unsupported. It would make the probe camera-dependent. */
    vP = P;
  }
  else {
#ifdef MAT_WORLD
    vP = transform_direction(ViewMatrix, P);
#else
    vP = transform_point(ViewMatrix, P);
#endif
  }
  vP.z = -vP.z;
  return vP;
}

vec3 coordinate_screen(vec3 P)
{
  vec3 window = vec3(0.0);
  if (false /* probe */) {
    /* Unsupported. It would make the probe camera-dependent. */
    window.xy = vec2(0.5);
  }
  else {
    /* TODO(fclem): Actual camera transform. */
    window.xy = project_point(ProjectionMatrix, transform_point(ViewMatrix, P)).xy * 0.5 + 0.5;
    window.xy = window.xy * camera_buf.uv_scale + camera_buf.uv_bias;
  }
  return window;
}

vec3 coordinate_reflect(vec3 P, vec3 N)
{
#ifdef MAT_WORLD
  return N;
#else
  return -reflect(cameraVec(P), N);
#endif
}

vec3 coordinate_incoming(vec3 P)
{
#ifdef MAT_WORLD
  return -P;
#else
  return cameraVec(P);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Attribute post
 *
 * TODO(@fclem): These implementation details should concern the DRWManager and not be a fix on
 * the engine side. But as of now, the engines are responsible for loading the attributes.
 *
 * \{ */

#if defined(MAT_GEOM_VOLUME_OBJECT) || defined(MAT_GEOM_VOLUME_WORLD)

float attr_load_temperature_post(float attr)
{
#  ifdef MAT_GEOM_VOLUME_OBJECT
  /* Bring the into standard range without having to modify the grid values */
  attr = (attr > 0.01) ? (attr * drw_volume.temperature_mul + drw_volume.temperature_bias) : 0.0;
#  endif
  return attr;
}
vec4 attr_load_color_post(vec4 attr)
{
#  ifdef MAT_GEOM_VOLUME_OBJECT
  /* Density is premultiplied for interpolation, divide it out here. */
  attr.rgb *= safe_rcp(attr.a);
  attr.rgb *= drw_volume.color_mul.rgb;
  attr.a = 1.0;
#  endif
  return attr;
}

#else /* Noop for any other surface. */

float attr_load_temperature_post(float attr)
{
  return attr;
}
vec4 attr_load_color_post(vec4 attr)
{
  return attr;
}

#endif

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
