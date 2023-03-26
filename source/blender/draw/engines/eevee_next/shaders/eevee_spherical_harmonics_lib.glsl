
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
  vec3 M0;
};

struct SphericalHarmonicBandL1 {
  vec3 Mn1;
  vec3 M0;
  vec3 Mp1;
};

struct SphericalHarmonicBandL2 {
  vec3 Mn2;
  vec3 Mn1;
  vec3 M0;
  vec3 Mp1;
  vec3 Mp2;
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
 * \{ */

void spherical_harmonics_L0_encode_signal_sample(vec3 direction,
                                                 vec3 amplitude,
                                                 inout SphericalHarmonicBandL0 r_L0)
{
  r_L0.M0 += spherical_harmonics_L0_M0(direction) * amplitude;
}

void spherical_harmonics_L1_encode_signal_sample(vec3 direction,
                                                 vec3 amplitude,
                                                 inout SphericalHarmonicBandL1 r_L1)
{
  r_L1.Mn1 += spherical_harmonics_L1_Mn1(direction) * amplitude;
  r_L1.M0 += spherical_harmonics_L1_M0(direction) * amplitude;
  r_L1.Mp1 += spherical_harmonics_L1_Mp1(direction) * amplitude;
}

void spherical_harmonics_L2_encode_signal_sample(vec3 direction,
                                                 vec3 amplitude,
                                                 inout SphericalHarmonicBandL2 r_L2)
{
  r_L2.Mn2 += spherical_harmonics_L2_Mn2(direction) * amplitude;
  r_L2.Mn1 += spherical_harmonics_L2_Mn1(direction) * amplitude;
  r_L2.M0 += spherical_harmonics_L2_M0(direction) * amplitude;
  r_L2.Mp1 += spherical_harmonics_L2_Mp1(direction) * amplitude;
  r_L2.Mp2 += spherical_harmonics_L2_Mp2(direction) * amplitude;
}

void spherical_harmonics_encode_signal_sample(vec3 direction,
                                              vec3 amplitude,
                                              inout SphericalHarmonicL0 sh)
{
  spherical_harmonics_L0_encode_signal_sample(direction, amplitude, sh.L0);
}

void spherical_harmonics_encode_signal_sample(vec3 direction,
                                              vec3 amplitude,
                                              inout SphericalHarmonicL1 sh)
{
  spherical_harmonics_L0_encode_signal_sample(direction, amplitude, sh.L0);
  spherical_harmonics_L1_encode_signal_sample(direction, amplitude, sh.L1);
}

void spherical_harmonics_encode_signal_sample(vec3 direction,
                                              vec3 amplitude,
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

vec3 spherical_harmonics_L0_evaluate(vec3 direction, SphericalHarmonicBandL0 L0)
{
  return spherical_harmonics_L0_M0(direction) * L0.M0;
}

vec3 spherical_harmonics_L1_evaluate(vec3 direction, SphericalHarmonicBandL1 L1)
{
  return spherical_harmonics_L1_Mn1(direction) * L1.Mn1 +
         spherical_harmonics_L1_M0(direction) * L1.M0 +
         spherical_harmonics_L1_Mp1(direction) * L1.Mp1;
}

vec3 spherical_harmonics_L2_evaluate(vec3 direction, SphericalHarmonicBandL2 L2)
{
  return spherical_harmonics_L2_Mn2(direction) * L2.Mn2 +
         spherical_harmonics_L2_Mn1(direction) * L2.Mn1 +
         spherical_harmonics_L2_M0(direction) * L2.M0 +
         spherical_harmonics_L2_Mp1(direction) * L2.Mp1 +
         spherical_harmonics_L2_Mp2(direction) * L2.Mp2;
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
  return spherical_harmonics_L0_evaluate(N, sh.L0);
}
vec3 spherical_harmonics_evaluate_lambert(vec3 N, SphericalHarmonicL1 sh)
{
  return spherical_harmonics_L0_evaluate(N, sh.L0) +
         spherical_harmonics_L1_evaluate(N, sh.L1) * (2.0 / 3.0);
}
vec3 spherical_harmonics_evaluate_lambert(vec3 N, SphericalHarmonicL2 sh)
{
  return spherical_harmonics_L0_evaluate(N, sh.L0) +
         spherical_harmonics_L1_evaluate(N, sh.L1) * (2.0 / 3.0) +
         spherical_harmonics_L2_evaluate(N, sh.L2) * (1.0 / 4.0);
}

/** \} */
