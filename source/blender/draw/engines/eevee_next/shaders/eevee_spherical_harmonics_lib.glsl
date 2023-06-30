
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Spherical Harmonics Functions
 *
 * `L` denote the row and `M` the column in the spherical harmonics table (1).
 * `p` denote positive column and `n` negative ones.
 *
 * Use precomputed constants to avoid constant folding differences across compilers.
 * Note that (2) doesn't use Condon-Shortley phase whereas our implementation does.
 *
 * Reference:
 * (1) https://en.wikipedia.org/wiki/Spherical_harmonics#/media/File:Sphericalfunctions.svg
 * (2) https://en.wikipedia.org/wiki/Table_of_spherical_harmonics#Real_spherical_harmonics
 * (3) https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
 *
 * \{ */

/* L0 Band. */
float spherical_harmonics_L0_M0(vec3 v)
{
  return 0.282094792;
}

/* L1 Band. */
float spherical_harmonics_L1_Mn1(vec3 v)
{
  return -0.488602512 * v.y;
}
float spherical_harmonics_L1_M0(vec3 v)
{
  return 0.488602512 * v.z;
}
float spherical_harmonics_L1_Mp1(vec3 v)
{
  return -0.488602512 * v.x;
}

/* L2 Band. */
float spherical_harmonics_L2_Mn2(vec3 v)
{
  return 1.092548431 * (v.x * v.y);
}
float spherical_harmonics_L2_Mn1(vec3 v)
{
  return -1.092548431 * (v.y * v.z);
}
float spherical_harmonics_L2_M0(vec3 v)
{
  return 0.315391565 * (3.0 * v.z * v.z - 1.0);
}
float spherical_harmonics_L2_Mp1(vec3 v)
{
  return -1.092548431 * (v.x * v.z);
}
float spherical_harmonics_L2_Mp2(vec3 v)
{
  return 0.546274215 * (v.x * v.x - v.y * v.y);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Structure
 * \{ */

struct SphericalHarmonicBandL0 {
  vec4 M0;
};

struct SphericalHarmonicBandL1 {
  vec4 Mn1;
  vec4 M0;
  vec4 Mp1;
};

struct SphericalHarmonicBandL2 {
  vec4 Mn2;
  vec4 Mn1;
  vec4 M0;
  vec4 Mp1;
  vec4 Mp2;
};

struct SphericalHarmonicL0 {
  SphericalHarmonicBandL0 L0;
};

struct SphericalHarmonicL1 {
  SphericalHarmonicBandL0 L0;
  SphericalHarmonicBandL1 L1;
};

struct SphericalHarmonicL2 {
  SphericalHarmonicBandL0 L0;
  SphericalHarmonicBandL1 L1;
  SphericalHarmonicBandL2 L2;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Encode
 *
 * Decompose an input signal into spherical harmonic coefficients.
 * Note that `amplitude` need to be scaled by solid angle.
 * \{ */

void spherical_harmonics_L0_encode_signal_sample(vec3 direction,
                                                 vec4 amplitude,
                                                 inout SphericalHarmonicBandL0 r_L0)
{
  r_L0.M0 += spherical_harmonics_L0_M0(direction) * amplitude;
}

void spherical_harmonics_L1_encode_signal_sample(vec3 direction,
                                                 vec4 amplitude,
                                                 inout SphericalHarmonicBandL1 r_L1)
{
  r_L1.Mn1 += spherical_harmonics_L1_Mn1(direction) * amplitude;
  r_L1.M0 += spherical_harmonics_L1_M0(direction) * amplitude;
  r_L1.Mp1 += spherical_harmonics_L1_Mp1(direction) * amplitude;
}

void spherical_harmonics_L2_encode_signal_sample(vec3 direction,
                                                 vec4 amplitude,
                                                 inout SphericalHarmonicBandL2 r_L2)
{
  r_L2.Mn2 += spherical_harmonics_L2_Mn2(direction) * amplitude;
  r_L2.Mn1 += spherical_harmonics_L2_Mn1(direction) * amplitude;
  r_L2.M0 += spherical_harmonics_L2_M0(direction) * amplitude;
  r_L2.Mp1 += spherical_harmonics_L2_Mp1(direction) * amplitude;
  r_L2.Mp2 += spherical_harmonics_L2_Mp2(direction) * amplitude;
}

void spherical_harmonics_encode_signal_sample(vec3 direction,
                                              vec4 amplitude,
                                              inout SphericalHarmonicL0 sh)
{
  spherical_harmonics_L0_encode_signal_sample(direction, amplitude, sh.L0);
}

void spherical_harmonics_encode_signal_sample(vec3 direction,
                                              vec4 amplitude,
                                              inout SphericalHarmonicL1 sh)
{
  spherical_harmonics_L0_encode_signal_sample(direction, amplitude, sh.L0);
  spherical_harmonics_L1_encode_signal_sample(direction, amplitude, sh.L1);
}

void spherical_harmonics_encode_signal_sample(vec3 direction,
                                              vec4 amplitude,
                                              inout SphericalHarmonicL2 sh)
{
  spherical_harmonics_L0_encode_signal_sample(direction, amplitude, sh.L0);
  spherical_harmonics_L1_encode_signal_sample(direction, amplitude, sh.L1);
  spherical_harmonics_L2_encode_signal_sample(direction, amplitude, sh.L2);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Decode
 *
 * Evaluate an encoded signal in a given unit vector direction.
 * \{ */

vec4 spherical_harmonics_L0_evaluate(vec3 direction, SphericalHarmonicBandL0 L0)
{
  return spherical_harmonics_L0_M0(direction) * L0.M0;
}

vec4 spherical_harmonics_L1_evaluate(vec3 direction, SphericalHarmonicBandL1 L1)
{
  return spherical_harmonics_L1_Mn1(direction) * L1.Mn1 +
         spherical_harmonics_L1_M0(direction) * L1.M0 +
         spherical_harmonics_L1_Mp1(direction) * L1.Mp1;
}

vec4 spherical_harmonics_L2_evaluate(vec3 direction, SphericalHarmonicBandL2 L2)
{
  return spherical_harmonics_L2_Mn2(direction) * L2.Mn2 +
         spherical_harmonics_L2_Mn1(direction) * L2.Mn1 +
         spherical_harmonics_L2_M0(direction) * L2.M0 +
         spherical_harmonics_L2_Mp1(direction) * L2.Mp1 +
         spherical_harmonics_L2_Mp2(direction) * L2.Mp2;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rotation
 * \{ */

void spherical_harmonics_L0_rotate(mat3x3 rotation, inout SphericalHarmonicBandL0 L0)
{
  /* L0 band being a constant function (i.e: there is no directionallity) there is nothing to
   * rotate. This is a no-op. */
}

void spherical_harmonics_L1_rotate(mat3x3 rotation, inout SphericalHarmonicBandL1 L1)
{
  /* Convert L1 coefficients to per channel column.
   * Note the component shuffle to match blender coordinate system. */
  mat4x3 per_channel = transpose(mat3x4(L1.Mp1, L1.Mn1, -L1.M0));
  /* Rotate each channel. */
  per_channel[0] = rotation * per_channel[0];
  per_channel[1] = rotation * per_channel[1];
  per_channel[2] = rotation * per_channel[2];
  /* Convert back to L1 coefficients to per channel column.
   * Note the component shuffle to match blender coordinate system. */
  mat3x4 per_coef = transpose(per_channel);
  L1.Mn1 = per_coef[1];
  L1.M0 = -per_coef[2];
  L1.Mp1 = per_coef[0];
}

void spherical_harmonics_L2_rotate(mat3x3 rotation, inout SphericalHarmonicBandL2 L2)
{
  /* TODO */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation
 * \{ */

/**
 * Convolve a spherical harmonic encoded irradiance signal as a lambertian reflection.
 * Returns the lambertian radiance (cosine lobe divided by PI) so the coefficients simplify to 1,
 * 2/3 and 1/4. See this reference for more explanation:
 * https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
 */
vec3 spherical_harmonics_evaluate_lambert(vec3 N, SphericalHarmonicL0 sh)
{
  vec3 radiance = spherical_harmonics_L0_evaluate(N, sh.L0).rgb;
  return radiance;
}
vec3 spherical_harmonics_evaluate_lambert(vec3 N, SphericalHarmonicL1 sh)
{
  vec3 radiance = spherical_harmonics_L0_evaluate(N, sh.L0).rgb +
                  spherical_harmonics_L1_evaluate(N, sh.L1).rgb * (2.0 / 3.0);
  return radiance;
}
vec3 spherical_harmonics_evaluate_lambert(vec3 N, SphericalHarmonicL2 sh)
{
  vec3 radiance = spherical_harmonics_L0_evaluate(N, sh.L0).rgb +
                  spherical_harmonics_L1_evaluate(N, sh.L1).rgb * (2.0 / 3.0) +
                  spherical_harmonics_L2_evaluate(N, sh.L2).rgb * (1.0 / 4.0);
  return radiance;
}

/**
 * Use non-linear reconstruction method to avoid negative lobe artifacts.
 * See this reference for more explanation:
 * https://grahamhazel.com/blog/2017/12/22/converting-sh-radiance-to-irradiance/
 */
float spherical_harmonics_evaluate_non_linear(vec3 N, float R0, vec3 R1)
{
  /* No idea why this is needed. */
  R1 /= 2.0;

  float R1_len;
  vec3 R1_dir = normalize_and_get_length(R1, R1_len);
  float rcp_R0 = safe_rcp(R0);

  float q = (1.0 + dot(R1_dir, N)) / 2.0;
  float p = 1.0 + 2.0 * R1_len * rcp_R0;
  float a = (1.0 - R1_len * rcp_R0) * safe_rcp(1.0 + R1_len * rcp_R0);

  return R0 * (a + (1.0 - a) * (p + 1.0) * pow(q, p));
}
vec3 spherical_harmonics_evaluate_lambert_non_linear(vec3 N, SphericalHarmonicL1 sh)
{
  /* Shuffling based on spherical_harmonics_L1_* functions. */
  vec3 R1_r = vec3(-sh.L1.Mp1.r, -sh.L1.Mn1.r, sh.L1.M0.r);
  vec3 R1_g = vec3(-sh.L1.Mp1.g, -sh.L1.Mn1.g, sh.L1.M0.g);
  vec3 R1_b = vec3(-sh.L1.Mp1.b, -sh.L1.Mn1.b, sh.L1.M0.b);

  vec3 radiance = vec3(spherical_harmonics_evaluate_non_linear(N, sh.L0.M0.r, R1_r),
                       spherical_harmonics_evaluate_non_linear(N, sh.L0.M0.g, R1_g),
                       spherical_harmonics_evaluate_non_linear(N, sh.L0.M0.b, R1_b));
  /* Return lambertian radiance. So divide by PI. */
  return radiance / M_PI;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load/Store
 *
 * This section define the compression scheme of spherical harmonic data.
 * \{ */

SphericalHarmonicL1 spherical_harmonics_unpack(vec4 L0_L1_a,
                                               vec4 L0_L1_b,
                                               vec4 L0_L1_c,
                                               vec4 L0_L1_vis)
{
  SphericalHarmonicL1 sh;
  sh.L0.M0.xyz = L0_L1_a.xyz;
  sh.L1.Mn1.xyz = L0_L1_b.xyz;
  sh.L1.M0.xyz = L0_L1_c.xyz;
  sh.L1.Mp1.xyz = vec3(L0_L1_a.w, L0_L1_b.w, L0_L1_c.w);
  sh.L0.M0.w = L0_L1_vis.x;
  sh.L1.Mn1.w = L0_L1_vis.y;
  sh.L1.M0.w = L0_L1_vis.z;
  sh.L1.Mp1.w = L0_L1_vis.w;
  return sh;
}

void spherical_harmonics_pack(SphericalHarmonicL1 sh,
                              out vec4 L0_L1_a,
                              out vec4 L0_L1_b,
                              out vec4 L0_L1_c,
                              out vec4 L0_L1_vis)
{
  L0_L1_a.xyz = sh.L0.M0.xyz;
  L0_L1_b.xyz = sh.L1.Mn1.xyz;
  L0_L1_c.xyz = sh.L1.M0.xyz;
  L0_L1_a.w = sh.L1.Mp1.x;
  L0_L1_b.w = sh.L1.Mp1.y;
  L0_L1_c.w = sh.L1.Mp1.z;
  L0_L1_vis = vec4(sh.L0.M0.w, sh.L1.Mn1.w, sh.L1.M0.w, sh.L1.Mp1.w);
}

/** \} */
