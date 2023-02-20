
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
/* Random number per sampled closure type. */
float g_diffuse_rand;
float g_reflection_rand;
float g_refraction_rand;

/**
 * Returns true if the closure is to be selected based on the input weight.
 */
bool closure_select(float weight, inout float total_weight, inout float r)
{
  total_weight += weight;
  float x = weight / total_weight;
  bool chosen = (r < x);
  /* Assuming that if r is in the interval [0,x] or [x,1], it's still uniformly distributed within
   * that interval, so you remapping to [0,1] again to explore this space of probability. */
  r = (chosen) ? (r / x) : ((r - x) / (1.0 - x));
  return chosen;
}

#define SELECT_CLOSURE(destination, random, candidate) \
  if (closure_select(candidate.weight, destination.weight, random)) { \
    destination = candidate; \
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

#if defined(GPU_FRAGMENT_SHADER)
  g_diffuse_rand = g_reflection_rand = g_refraction_rand = g_closure_rand;
#else
  g_diffuse_rand = 0.0;
  g_reflection_rand = 0.0;
  g_refraction_rand = 0.0;
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
  /* TODO */
  return Closure(0);
}

Closure closure_eval(ClosureVolumeAbsorption volume_absorption)
{
  /* TODO */
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
  /* TODO */
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
                             float distance,
                             const float inverted,
                             const float sample_count)
{
  /* TODO */
  return 1.0;
}

#ifndef GPU_METAL
void attrib_load();
Closure nodetree_surface();
Closure nodetree_volume();
vec3 nodetree_displacement();
float nodetree_thickness();
vec4 closure_to_rgba(Closure cl);
#endif

/* Stubs. */
vec2 btdf_lut(float a, float b, float c)
{
  return vec2(1, 0);
}
vec2 brdf_lut(float a, float b)
{
  return vec2(1, 0);
}
vec3 F_brdf_multi_scatter(vec3 a, vec3 b, vec2 c)
{
  return a;
}
vec3 F_brdf_single_scatter(vec3 a, vec3 b, vec2 c)
{
  return a;
}
float F_eta(float a, float b)
{
  return a;
}
void output_aov(vec4 color, float value, uint hash)
{
#if defined(MAT_AOV_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < aov_buf.color_len; i++) {
    if (aov_buf.hash_color[i] == hash) {
      imageStore(aov_color_img, ivec3(gl_FragCoord.xy, i), color);
      return;
    }
  }
  for (int i = 0; i < AOV_MAX && i < aov_buf.value_len; i++) {
    if (aov_buf.hash_value[i] == hash) {
      imageStore(aov_value_img, ivec3(gl_FragCoord.xy, i), vec4(value));
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

#if defined(MAT_GEOM_VOLUME)

float attr_load_temperature_post(float attr)
{
  /* Bring the into standard range without having to modify the grid values */
  attr = (attr > 0.01) ? (attr * drw_volume.temperature_mul + drw_volume.temperature_bias) : 0.0;
  return attr;
}
vec4 attr_load_color_post(vec4 attr)
{
  /* Density is premultiplied for interpolation, divide it out here. */
  attr.rgb *= safe_rcp(attr.a);
  attr.rgb *= drw_volume.color_mul.rgb;
  attr.a = 1.0;
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
