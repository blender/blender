/* SPDX-FileCopyrightText: 2011-2020 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * \ingroup intern_sky_modal
 */

#include <algorithm>

#include "sky_math.h"
#include "sky_nishita.h"

/* Constants. */
static const float RAYLEIGH_SCALE = 8e3f;       /* Rayleigh scale height (m). */
static const float MIE_SCALE = 1.2e3f;          /* Mie scale height (m). */
static const float MIE_COEFF = 2e-5f;           /* Mie scattering coefficient (m^-1). */
static const float MIE_G = 0.76f;               /* Aerosols anisotropy. */
static const float SQR_G = MIE_G * MIE_G;       /* Squared aerosols anisotropy. */
static const float EARTH_RADIUS = 6360e3f;      /* Radius of Earth (m). */
static const float ATMOSPHERE_RADIUS = 6420e3f; /* Radius of atmosphere (m). */
static const int STEPS = 32;                    /* Segments of primary ray. */
static const int NUM_WAVELENGTHS = 21;          /* Number of wavelengths. */
static const int MIN_WAVELENGTH = 380;          /* Lowest sampled wavelength (nm). */
static const int MAX_WAVELENGTH = 780;          /* Highest sampled wavelength (nm). */
/* Step between each sampled wavelength (nm). */
static const float STEP_LAMBDA = (MAX_WAVELENGTH - MIN_WAVELENGTH) / (NUM_WAVELENGTHS - 1);
/* Sun irradiance on top of the atmosphere (W*m^-2*nm^-1). */
static const float IRRADIANCE[] = {
    1.45756829855592995315f, 1.56596305559738380175f, 1.65148449067670455293f,
    1.71496242737209314555f, 1.75797983805020541226f, 1.78256407885924539336f,
    1.79095108475838560302f, 1.78541550133410664714f, 1.76815554864306845317f,
    1.74122069647250410362f, 1.70647127164943679389f, 1.66556087452739887134f,
    1.61993437242451854274f, 1.57083597368892080581f, 1.51932335059305478886f,
    1.46628494965214395407f, 1.41245852740172450623f, 1.35844961970384092709f,
    1.30474913844739281998f, 1.25174963272610817455f, 1.19975998755420620867f};
/* Rayleigh scattering coefficient (m^-1). */
static const float RAYLEIGH_COEFF[] = {
    0.00005424820087636473f, 0.00004418549866505454f, 0.00003635151910165377f,
    0.00003017929012024763f, 0.00002526320226989157f, 0.00002130859310621843f,
    0.00001809838025320633f, 0.00001547057129129042f, 0.00001330284977336850f,
    0.00001150184784075764f, 0.00000999557429990163f, 0.00000872799973630707f,
    0.00000765513700977967f, 0.00000674217203751443f, 0.00000596134125832052f,
    0.00000529034598065810f, 0.00000471115687557433f, 0.00000420910481110487f,
    0.00000377218381260133f, 0.00000339051255477280f, 0.00000305591531679811f};
/* Ozone absorption coefficient (m^-1). */
static const float OZONE_COEFF[] = {
    0.00000000325126849861f, 0.00000000585395365047f, 0.00000001977191155085f,
    0.00000007309568762914f, 0.00000020084561514287f, 0.00000040383958096161f,
    0.00000063551335912363f, 0.00000096707041180970f, 0.00000154797400424410f,
    0.00000209038647223331f, 0.00000246128056164565f, 0.00000273551299461512f,
    0.00000215125863128643f, 0.00000159051840791988f, 0.00000112356197979857f,
    0.00000073527551487574f, 0.00000046450130357806f, 0.00000033096079921048f,
    0.00000022512612292678f, 0.00000014879129266490f, 0.00000016828623364192f};
/* CIE XYZ color matching functions. */
static const float CMF_XYZ[][3] = {{0.00136800000f, 0.00003900000f, 0.00645000100f},
                                   {0.01431000000f, 0.00039600000f, 0.06785001000f},
                                   {0.13438000000f, 0.00400000000f, 0.64560000000f},
                                   {0.34828000000f, 0.02300000000f, 1.74706000000f},
                                   {0.29080000000f, 0.06000000000f, 1.66920000000f},
                                   {0.09564000000f, 0.13902000000f, 0.81295010000f},
                                   {0.00490000000f, 0.32300000000f, 0.27200000000f},
                                   {0.06327000000f, 0.71000000000f, 0.07824999000f},
                                   {0.29040000000f, 0.95400000000f, 0.02030000000f},
                                   {0.59450000000f, 0.99500000000f, 0.00390000000f},
                                   {0.91630000000f, 0.87000000000f, 0.00165000100f},
                                   {1.06220000000f, 0.63100000000f, 0.00080000000f},
                                   {0.85444990000f, 0.38100000000f, 0.00019000000f},
                                   {0.44790000000f, 0.17500000000f, 0.00002000000f},
                                   {0.16490000000f, 0.06100000000f, 0.00000000000f},
                                   {0.04677000000f, 0.01700000000f, 0.00000000000f},
                                   {0.01135916000f, 0.00410200000f, 0.00000000000f},
                                   {0.00289932700f, 0.00104700000f, 0.00000000000f},
                                   {0.00069007860f, 0.00024920000f, 0.00000000000f},
                                   {0.00016615050f, 0.00006000000f, 0.00000000000f},
                                   {0.00004150994f, 0.00001499000f, 0.00000000000f}};

/* Parameters for optical depth quadrature.
 * See the comment in ray_optical_depth for more detail.
 * Computed using sympy and following Python code:
 * # from sympy.integrals.quadrature import gauss_laguerre
 * # from sympy import exp
 * # x, w = gauss_laguerre(8, 50)
 * # xend = 25
 * # print([(xi / xend).evalf(10) for xi in x])
 * # print([(wi * exp(xi) / xend).evalf(10) for xi, wi in zip(x, w)])
 */
static const int QUADRATURE_STEPS = 8;
static const float QUADRATURE_NODES[] = {0.006811185292f,
                                         0.03614807107f,
                                         0.09004346519f,
                                         0.1706680068f,
                                         0.2818362161f,
                                         0.4303406404f,
                                         0.6296271457f,
                                         0.9145252695f};
static const float QUADRATURE_WEIGHTS[] = {0.01750893642f,
                                           0.04135477391f,
                                           0.06678839063f,
                                           0.09507698807f,
                                           0.1283416365f,
                                           0.1707430204f,
                                           0.2327233347f,
                                           0.3562490486f};

static float3 geographical_to_direction(float lat, float lon)
{
  return make_float3(cosf(lat) * cosf(lon), cosf(lat) * sinf(lon), sinf(lat));
}

static float3 spec_to_xyz(const float *spectrum)
{
  float3 xyz = make_float3(0.0f, 0.0f, 0.0f);
  for (int i = 0; i < NUM_WAVELENGTHS; i++) {
    xyz.x += CMF_XYZ[i][0] * spectrum[i];
    xyz.y += CMF_XYZ[i][1] * spectrum[i];
    xyz.z += CMF_XYZ[i][2] * spectrum[i];
  }
  return xyz * STEP_LAMBDA;
}

/* Atmosphere volume models */
static float density_rayleigh(float height)
{
  return expf(-height / RAYLEIGH_SCALE);
}

static float density_mie(float height)
{
  return expf(-height / MIE_SCALE);
}

static float density_ozone(float height)
{
  return fmax(0.0, 1.0 - (fabs(height - 25000.0) / 15000.0));
}

static float phase_rayleigh(float mu)
{
  return (0.1875f * M_1_PI_F) * (1.0f + sqr(mu));
}

static float phase_mie(float mu)
{
  return (3.0f * (1.0f - SQR_G) * (1.0f + sqr(mu))) /
         (8.0f * M_PI_F * (2.0f + SQR_G) * powf((1.0f + SQR_G - 2.0f * MIE_G * mu), 1.5));
}

/* Intersection helpers. */
static bool surface_intersection(float3 pos, float3 dir)
{
  if (dir.z >= 0) {
    return false;
  }
  float b = -2.0f * dot(dir, -pos);
  float c = len_squared(pos) - sqr(EARTH_RADIUS);
  float t = b * b - 4.0f * c;
  if (t >= 0.0f) {
    return true;
  }
  return false;
}

static float3 atmosphere_intersection(float3 pos, float3 dir)
{
  float b = -2.0f * dot(dir, -pos);
  float c = len_squared(pos) - sqr(ATMOSPHERE_RADIUS);
  float t = (-b + sqrtf(b * b - 4.0f * c)) / 2.0f;
  return make_float3(pos.x + dir.x * t, pos.y + dir.y * t, pos.z + dir.z * t);
}

static float3 ray_optical_depth(float3 ray_origin, float3 ray_dir)
{
  /* This function computes the optical depth along a ray.
   * Instead of using classic ray marching, the code is based on Gauss-Laguerre quadrature,
   * which is designed to compute the integral of f(x)*exp(-x) from 0 to infinity.
   * This works well here, since the optical depth along the ray tends to decrease exponentially.
   * By setting f(x) = g(x) exp(x), the exponentials cancel out and we get the integral of g(x).
   * The nodes and weights used here are the standard n=6 Gauss-Laguerre values, except that
   * the exp(x) scaling factor is already included in the weights.
   * The parametrization along the ray is scaled so that the last quadrature node is still within
   * the atmosphere. */
  float3 ray_end = atmosphere_intersection(ray_origin, ray_dir);
  float ray_length = distance(ray_origin, ray_end);

  float3 segment = ray_length * ray_dir;

  /* Instead of tracking the transmission spectrum across all wavelengths directly,
   * we use the fact that the density always has the same spectrum for each type of
   * scattering, so we split the density into a constant spectrum and a factor and
   * only track the factors. */
  float3 optical_depth = make_float3(0.0f, 0.0f, 0.0f);

  for (int i = 0; i < QUADRATURE_STEPS; i++) {
    float3 P = ray_origin + QUADRATURE_NODES[i] * segment;

    /* Height above sea level. */
    float height = len(P) - EARTH_RADIUS;

    float3 density = make_float3(
        density_rayleigh(height), density_mie(height), density_ozone(height));
    optical_depth += density * QUADRATURE_WEIGHTS[i];
  }

  return optical_depth * ray_length;
}

static void single_scattering(float3 ray_dir,
                              float3 sun_dir,
                              float3 ray_origin,
                              float air_density,
                              float aerosol_density,
                              float ozone_density,
                              float *r_spectrum)
{
  /* This code computes single-inscattering along a ray through the atmosphere. */
  float3 ray_end = atmosphere_intersection(ray_origin, ray_dir);
  float ray_length = distance(ray_origin, ray_end);

  /* To compute the inscattering, we step along the ray in segments and accumulate
   * the inscattering as well as the optical depth along each segment. */
  float segment_length = ray_length / STEPS;
  float3 segment = segment_length * ray_dir;

  /* Instead of tracking the transmission spectrum across all wavelengths directly,
   * we use the fact that the density always has the same spectrum for each type of
   * scattering, so we split the density into a constant spectrum and a factor and
   * only track the factors. */
  float3 optical_depth = make_float3(0.0f, 0.0f, 0.0f);

  /* Zero out light accumulation. */
  for (int wl = 0; wl < NUM_WAVELENGTHS; wl++) {
    r_spectrum[wl] = 0.0f;
  }

  /* Phase function for scattering and the density scale factor. */
  float mu = dot(ray_dir, sun_dir);
  float3 phase_function = make_float3(phase_rayleigh(mu), phase_mie(mu), 0.0f);
  float3 density_scale = make_float3(air_density, aerosol_density, ozone_density);

  /* The density and in-scattering of each segment is evaluated at its middle. */
  float3 P = ray_origin + 0.5f * segment;

  for (int i = 0; i < STEPS; i++) {
    /* Height above sea level. */
    float height = len(P) - EARTH_RADIUS;

    /* Evaluate and accumulate optical depth along the ray. */
    float3 density = density_scale * make_float3(density_rayleigh(height),
                                                 density_mie(height),
                                                 density_ozone(height));
    optical_depth += segment_length * density;

    /* If the Earth isn't in the way, evaluate inscattering from the Sun. */
    if (!surface_intersection(P, sun_dir)) {
      float3 light_optical_depth = density_scale * ray_optical_depth(P, sun_dir);
      float3 total_optical_depth = optical_depth + light_optical_depth;

      /* Attenuation of light. */
      for (int wl = 0; wl < NUM_WAVELENGTHS; wl++) {
        float3 extinction_density = total_optical_depth * make_float3(RAYLEIGH_COEFF[wl],
                                                                      1.11f * MIE_COEFF,
                                                                      OZONE_COEFF[wl]);
        float attenuation = expf(-reduce_add(extinction_density));

        float3 scattering_density = density * make_float3(RAYLEIGH_COEFF[wl], MIE_COEFF, 0.0f);

        /* The total inscattered radiance from one segment is:
         * Tr(A<->B) * Tr(B<->C) * sigma_s * phase * L * segment_length
         *
         * These terms are:
         * Tr(A<->B): Transmission from start to scattering position (tracked in optical_depth)
         * Tr(B<->C): Transmission from scattering position to light (computed in
         * ray_optical_depth) sigma_s: Scattering density phase: Phase function of the scattering
         * type (Rayleigh or Mie) L: Radiance coming from the light source segment_length: The
         * length of the segment
         *
         * The code here is just that, with a bit of additional optimization to not store full
         * spectra for the optical depth
         */
        r_spectrum[wl] += attenuation * reduce_add(phase_function * scattering_density) *
                          IRRADIANCE[wl] * segment_length;
      }
    }

    /* Advance along ray. */
    P += segment;
  }
}

void SKY_single_scattering_precompute_texture(float *pixels,
                                              int stride,
                                              int width,
                                              int height,
                                              float sun_elevation,
                                              float altitude,
                                              float air_density,
                                              float aerosol_density,
                                              float ozone_density)
{
  /* Clamp altitude to avoid numerical issues. */
  altitude = clamp(altitude, 1.0f, 59999.0f);
  /* Calculate texture pixels. */
  const int half_width = width / 2;
  const int half_height = height / 2;
  const float3 cam_pos = make_float3(0, 0, EARTH_RADIUS + altitude);
  const float3 sun_dir = geographical_to_direction(sun_elevation, 0.0f);
  const float longitude_step = M_2PI_F / width;
  const int rows_per_task = std::max(1024 / width, 1);

  /* Compute Sky in the upper hemisphere. */
  SKY_parallel_for(half_height, height, rows_per_task, [=](const size_t begin, const size_t end) {
    for (int y = begin; y < end; y++) {
      /* Sample more pixels toward the horizon. */
      float latitude = M_PI_2_F * sqr(float(y) / half_height - 1.0f);
      float *pixel_row = pixels + (y * width * stride);

      for (int x = 0; x < half_width; x++) {
        float longitude = longitude_step * x - M_PI_F;
        float3 dir = geographical_to_direction(latitude, longitude);
        float spectrum[NUM_WAVELENGTHS];
        single_scattering(
            dir, sun_dir, cam_pos, air_density, aerosol_density, ozone_density, spectrum);
        const float3 xyz = spec_to_xyz(spectrum);

        /* Store pixels. */
        int pos_x = x * stride;
        pixel_row[pos_x] = xyz.x;
        pixel_row[pos_x + 1] = xyz.y;
        pixel_row[pos_x + 2] = xyz.z;
        /* Mirror sky. */
        int mirror_x = (width - x - 1) * stride;
        pixel_row[mirror_x] = xyz.x;
        pixel_row[mirror_x + 1] = xyz.y;
        pixel_row[mirror_x + 2] = xyz.z;
      }
    }
  });

  /* Fill in the lower hemisphere by fading out the horizon. */
  for (int y = 0; y < half_height; y++) {
    /* Sample more pixels toward the horizon. */
    float latitude = M_PI_2_F * sqr(float(y) / half_height - 1.0f);
    float3 dir = geographical_to_direction(latitude, 0.0f);
    float fade = 0.0f;
    if (dir.z < 0.4f) {
      fade = 1.0f - dir.z * 2.5f;
      fade = sqr(fade) * fade;
    }
    float *pixel_row = pixels + (y * width * stride);
    float *horizon_row = pixels + (half_height * width * stride);

    for (int x = 0, offset = 0; x < width; x++, offset += stride) {
      pixel_row[offset + 0] = horizon_row[offset + 0] * fade;
      pixel_row[offset + 1] = horizon_row[offset + 1] * fade;
      pixel_row[offset + 2] = horizon_row[offset + 2] * fade;
    }
  }
}

/*********** Sun ***********/
static void sun_radiation(float3 cam_dir,
                          float altitude,
                          float air_density,
                          float aerosol_density,
                          float solid_angle,
                          float *r_spectrum)
{
  float3 cam_pos = make_float3(0, 0, EARTH_RADIUS + altitude);
  float3 optical_depth = ray_optical_depth(cam_pos, cam_dir);

  /* Compute final spectrum. */
  for (int i = 0; i < NUM_WAVELENGTHS; i++) {
    /* Combine spectra and the optical depth into transmittance. */
    float transmittance = RAYLEIGH_COEFF[i] * optical_depth.x * air_density +
                          1.11f * MIE_COEFF * optical_depth.y * aerosol_density;
    r_spectrum[i] = IRRADIANCE[i] * expf(-transmittance) / solid_angle;
  }
}

void SKY_single_scattering_precompute_sun(float sun_elevation,
                                          float angular_diameter,
                                          float altitude,
                                          float air_density,
                                          float aerosol_density,
                                          float r_pixel_bottom[3],
                                          float r_pixel_top[3])
{
  /* Clamp altitude to avoid numerical issues. */
  altitude = clamp(altitude, 1.0f, 59999.0f);
  float half_angular = angular_diameter / 2.0f;
  float solid_angle = M_2PI_F * (1.0f - cosf(half_angular));
  float spectrum[NUM_WAVELENGTHS];
  float bottom = sun_elevation - half_angular;
  float top = sun_elevation + half_angular;
  float elevation_bottom, elevation_top;
  float3 pix_bottom, pix_top, sun_dir;

  /* Compute 2 pixels for Sun disc: one is the lowest point of the disc, one is the highest.
   * Return black pixels if Sun is below horizon. */
  elevation_bottom = (bottom > 0.0f) ? bottom : 0.0f;
  elevation_top = (top > 0.0f) ? top : 0.0f;
  if (elevation_top > 0.0f) {
    sun_dir = geographical_to_direction(elevation_bottom, 0.0f);
    sun_radiation(sun_dir, altitude, air_density, aerosol_density, solid_angle, spectrum);
    pix_bottom = spec_to_xyz(spectrum);
    sun_dir = geographical_to_direction(elevation_top, 0.0f);
    sun_radiation(sun_dir, altitude, air_density, aerosol_density, solid_angle, spectrum);
    pix_top = spec_to_xyz(spectrum);
  }
  else {
    pix_bottom = make_float3(0.0f, 0.0f, 0.0f);
    pix_top = make_float3(0.0f, 0.0f, 0.0f);
  }

  /* Store pixels. */
  r_pixel_bottom[0] = pix_bottom.x;
  r_pixel_bottom[1] = pix_bottom.y;
  r_pixel_bottom[2] = pix_bottom.z;
  r_pixel_top[0] = pix_top.x;
  r_pixel_top[1] = pix_top.y;
  r_pixel_top[2] = pix_top.z;
}
