
vec3 calc_barycentric_distances(vec3 pos0, vec3 pos1, vec3 pos2)
{
  vec3 edge21 = pos2 - pos1;
  vec3 edge10 = pos1 - pos0;
  vec3 edge02 = pos0 - pos2;
  vec3 d21 = normalize(edge21);
  vec3 d10 = normalize(edge10);
  vec3 d02 = normalize(edge02);

  vec3 dists;
  float d = dot(d21, edge02);
  dists.x = sqrt(dot(edge02, edge02) - d * d);
  d = dot(d02, edge10);
  dists.y = sqrt(dot(edge10, edge10) - d * d);
  d = dot(d10, edge21);
  dists.z = sqrt(dot(edge21, edge21) - d * d);
  return dists;
}

vec2 calc_barycentric_co(int vertid)
{
  vec2 bary;
  bary.x = float((vertid % 3) == 0);
  bary.y = float((vertid % 3) == 1);
  return bary;
}

#ifdef HAIR_SHADER

/* Hairs uv and col attributes are passed by bufferTextures. */
#  define DEFINE_ATTR(type, attr) uniform samplerBuffer attr
#  define GET_ATTR(type, attr) hair_get_customdata_##type(attr)

#  define barycentric_get() hair_get_barycentric()
#  define barycentric_resolve(bary) hair_resolve_barycentric(bary)

vec3 orco_get(vec3 local_pos, mat4 modelmatinv, vec4 orco_madd[2], const samplerBuffer orco_samp)
{
  /* TODO: fix ORCO with modifiers. */
  vec3 orco = (modelmatinv * vec4(local_pos, 1.0)).xyz;
  return orco_madd[0].xyz + orco * orco_madd[1].xyz;
}

float hair_len_get(int id, const samplerBuffer len)
{
  return texelFetch(len, id).x;
}

vec4 tangent_get(const samplerBuffer attr, mat3 normalmat)
{
  /* Unsupported */
  return vec4(0.0);
}

#else /* MESH_SHADER */

#  define DEFINE_ATTR(type, attr) in type attr
#  define GET_ATTR(type, attr) attr

/* Calculated in geom shader later with calc_barycentric_co. */
#  define barycentric_get() vec2(0)
#  define barycentric_resolve(bary) bary

vec3 orco_get(vec3 local_pos, mat4 modelmatinv, vec4 orco_madd[2], vec4 orco)
{
  /* If the object does not have any deformation, the orco layer calculation is done on the fly
   * using the orco_madd factors.
   * We know when there is no orco layer when orco.w is 1.0 because it uses the generic vertex
   * attribute (which is [0,0,0,1]). */
  if (orco.w == 0.0) {
    return orco.xyz * 0.5 + 0.5;
  }
  else {
    return orco_madd[0].xyz + local_pos * orco_madd[1].xyz;
  }
}

float hair_len_get(int id, const float len)
{
  return len;
}

vec4 tangent_get(vec4 attr, mat3 normalmat)
{
  vec4 tangent;
  tangent.xyz = normalmat * attr.xyz;
  tangent.w = attr.w;
  float len_sqr = dot(tangent.xyz, tangent.xyz);
  /* Normalize only if vector is not null. */
  if (len_sqr > 0.0) {
    tangent.xyz *= inversesqrt(len_sqr);
  }
  return tangent;
}

#endif

/* Assumes GPU_VEC4 is color data. So converting to luminance like cycles. */
#define float_from_vec4(v) dot(v.rgb, vec3(0.2126, 0.7152, 0.0722))
#define float_from_vec3(v) avg(v.rgb)
#define float_from_vec2(v) v.r

#define vec2_from_vec4(v) vec2(avg(v.rgb), v.a)
#define vec2_from_vec3(v) vec2(avg(v.rgb), 1.0)
#define vec2_from_float(v) vec2(v)

#define vec3_from_vec4(v) v.rgb
#define vec3_from_vec2(v) v.rrr
#define vec3_from_float(v) vec3(v)

#define vec4_from_vec3(v) vec4(v, 1.0)
#define vec4_from_vec2(v) v.rrrg
#define vec4_from_float(v) vec4(vec3(v), 1.0)

/* TODO: Move to shader_shared. */
#define RAY_TYPE_CAMERA 0
#define RAY_TYPE_SHADOW 1
#define RAY_TYPE_DIFFUSE 2
#define RAY_TYPE_GLOSSY 3

#ifdef GPU_FRAGMENT_SHADER
#  define FrontFacing gl_FrontFacing
#else
#  define FrontFacing true
#endif

struct ClosureDiffuse {
  float weight;
  vec3 color;
  vec3 N;
  vec3 sss_radius;
  uint sss_id;
};

struct ClosureTranslucent {
  float weight;
  vec3 color;
  vec3 N;
};

struct ClosureReflection {
  float weight;
  vec3 color;
  vec3 N;
  float roughness;
};

struct ClosureRefraction {
  float weight;
  vec3 color;
  vec3 N;
  float roughness;
  float ior;
};

struct ClosureHair {
  float weight;
  vec3 color;
  float offset;
  vec2 roughness;
  vec3 T;
};

struct ClosureVolumeScatter {
  float weight;
  vec3 scattering;
  float anisotropy;
};

struct ClosureVolumeAbsorption {
  float weight;
  vec3 absorption;
};

struct ClosureEmission {
  float weight;
  vec3 emission;
};

struct ClosureTransparency {
  float weight;
  vec3 transmittance;
  float holdout;
};

struct GlobalData {
  /** World position. */
  vec3 P;
  /** Surface Normal. Normalized, overridden by bump displacement. */
  vec3 N;
  /** Raw interpolated normal (non-normalized) data. */
  vec3 Ni;
  /** Geometric Normal. */
  vec3 Ng;
  /** Curve Tangent Space. */
  vec3 curve_T, curve_B, curve_N;
  /** Barycentric coordinates. */
  vec2 barycentric_coords;
  vec3 barycentric_dists;
  /** Ray properties (approximation). */
  int ray_type;
  float ray_depth;
  float ray_length;
  /** Hair time along hair length. 0 at base 1 at tip. */
  float hair_time;
  /** Hair time along width of the hair. */
  float hair_time_width;
  /** Hair thickness in world space. */
  float hair_thickness;
  /** Index of the strand for per strand effects. */
  int hair_strand_id;
  /** Is hair. */
  bool is_strand;
};

GlobalData g_data;

#ifndef GPU_FRAGMENT_SHADER
/* Stubs. */
vec3 dF_impl(vec3 v)
{
  return vec3(0.0);
}

void dF_branch(float fn, out vec2 result)
{
  result = vec2(0.0);
}

void dF_branch_incomplete(float fn, out vec2 result)
{
  result = vec2(0.0);
}

#elif 0 /* TODO(@fclem): User Option? */
/* Fast derivatives */
vec3 dF_impl(vec3 v)
{
  return vec3(0.0);
}

void dF_branch(float fn, out vec2 result)
{
  result.x = DFDX_SIGN * dFdx(fn);
  result.y = DFDY_SIGN * dFdy(fn);
}

#else
/* Precise derivatives */
int g_derivative_flag = 0;

vec3 dF_impl(vec3 v)
{
  if (g_derivative_flag > 0) {
    return DFDX_SIGN * dFdx(v);
  }
  else if (g_derivative_flag < 0) {
    return DFDY_SIGN * dFdy(v);
  }
  return vec3(0.0);
}

#  define dF_branch(fn, result) \
    if (true) { \
      g_derivative_flag = 1; \
      result.x = (fn); \
      g_derivative_flag = -1; \
      result.y = (fn); \
      g_derivative_flag = 0; \
      result -= vec2((fn)); \
    }

/* Used when the non-offset value is already computed elsewhere */
#  define dF_branch_incomplete(fn, result) \
    if (true) { \
      g_derivative_flag = 1; \
      result.x = (fn); \
      g_derivative_flag = -1; \
      result.y = (fn); \
      g_derivative_flag = 0; \
    }
#endif

/* TODO(fclem): Remove. */
#define CODEGEN_LIB
