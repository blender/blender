/* SPDX-FileCopyrightText: 2022 Fernando García Liñán
 * SPDX-FileCopyrightText: 2011-2025 Blender Authors
 *
 * SPDX-License-Identifier: MIT */

/** \file
 * \ingroup intern_sky_modal
 */

/*
 * This code is a converted version of the ShaderToy written by Fernando García Liñán.
 *
 * This shader is the final result of my Master's Thesis.
 * The main contributions are:
 *
 * 1. A spectral rendering technique that only requires 4 wavelength samples to
 *    get accurate results.
 * 2. A multiple scattering approximation.
 *
 * Both of these approximations rely on an analytical fit, so they only work for
 * Earth's atmosphere. We make up for it by using a very flexible atmosphere
 * model that is able to represent a wide variety of atmospheric conditions.
 *
 * A brief description of this spectral rendering technique can be found in the
 * following article:
 * https://fgarlin.com/posts/2024-12-06-spectral_sky/
 *
 * The path tracer that has been used as a ground truth can be found at:
 * https://github.com/fgarlin/skytracer
 */

#include <algorithm>

#include "sky_math.h"
#include "sky_nishita.h"

using std::min;

/* Earth's atmosphere parameters. */
/* Ground reflectance. */
static const float4 GROUND_ALBEDO = make_float4(0.3f, 0.3f, 0.3f, 0.3f);
static const float PHASE_ISOTROPIC = M_1_4PI_F;
static const float RAYLEIGH_PHASE_SCALE = (3.0f / 16.0f) * M_1_PI_F;
/* Aerosols anisotropy. */
static const float G = 0.8f;
static const float SQR_G = G * G;
/* Earth radius (km). */
static const float EARTH_RADIUS = 6371.0f;
/* Atmosphere thickness (km). */
static const float ATMOSPHERE_THICKNESS = 100.0f;
static const float ATMOSPHERE_RADIUS = EARTH_RADIUS + ATMOSPHERE_THICKNESS;
/* Ray marching steps. Higher steps means increased accuracy but worse performance. */
static const int TRANSMITTANCE_STEPS = 64;
static const int IN_SCATTERING_STEPS = 64;

/* LUTs. */
static const int TRANSMITTANCE_RES_X = 256;
static const int TRANSMITTANCE_RES_Y = 64;

/* Spectral data sampled at 630, 560, 490, 430 nm for urban area. */
static const float4 SUN_SPECTRAL_IRRADIANCE = make_float4(1.679f, 1.828f, 1.986f, 1.307f);
static const float4 MOLECULAR_SCATTERING_COEFFICIENT_BASE = make_float4(
    6.605e-3f, 1.067e-2f, 1.842e-2f, 3.156e-2f);
static const float4 OZONE_ABSORPTION_CROSS_SECTION = make_float4(
    3.472e-25f, 3.914e-25f, 1.349e-25f, 11.03e-27f);
/* Average ozone dobson of monthly mean values. */
static const float OZONE_MEAN_DOBSON = 334.5f;
static const float4 AEROSOL_ABSORPTION_CROSS_SECTION = make_float4(
    2.8722e-24f, 4.6168e-24f, 7.9706e-24f, 1.3578e-23f);
static const float4 AEROSOL_SCATTERING_CROSS_SECTION = make_float4(
    1.5908e-22f, 1.7711e-22f, 2.0942e-22f, 2.4033e-22f);
static const float AEROSOL_BASE_DENSITY = 1.3681e20f;
static const float AEROSOL_BACKGROUND_DENSITY = 2e6f;
static const float AEROSOL_HEIGHT_SCALE = 0.73f;
/* Spectral to XYZ space conversion matrix. */
static const float3 SPECTRAL_XYZ[4] = {
    make_float3(53.386917738564668023f, 22.981337506691024754f, 0.0f),
    make_float3(43.904844466369358263f, 71.347795700053393866f, 0.102506867965741307f),
    make_float3(1.6137278251608962005f, 18.422960591455485011f, 31.742921188390805758f),
    make_float3(20.762668673810577145f, 2.3614213523314368527f, 110.48009643252140334f),
};

inline float molecular_phase_function(const float cos_theta)
{
  return RAYLEIGH_PHASE_SCALE * (1.0f + sqr(cos_theta));
}

inline float aerosol_phase_function(const float cos_theta)
{
  float den = 1.0f + SQR_G + 2.0f * G * cos_theta;
  return M_1_4PI_F * (1.0f - SQR_G) / (den * sqrtf(den));
}

inline float4 get_molecular_scattering_coefficient(const float h)
{
  return MOLECULAR_SCATTERING_COEFFICIENT_BASE * expf(-0.07771971f * powf(h, 1.16364243f));
}

inline float4 get_molecular_absorption_coefficient(const float h)
{
  const float log_h = logf(fmaxf(h, 1e-4f));
  float density = 3.78547397e20f * expf(-sqr(log_h - 3.22261f) * 5.55555555f - log_h);
  return OZONE_ABSORPTION_CROSS_SECTION * OZONE_MEAN_DOBSON * density;
}

inline float get_aerosol_density(const float h)
{
  float division = AEROSOL_BACKGROUND_DENSITY / AEROSOL_BASE_DENSITY;
  return AEROSOL_BASE_DENSITY * (expf(-h / AEROSOL_HEIGHT_SCALE) + division);
}

inline float3 spectral_to_xyz(const float4 L)
{
  float3 xyz = make_float3(0.0f, 0.0f, 0.0f);
  for (int i = 0; i < 4; i++) {
    xyz += SPECTRAL_XYZ[i] * L[i];
  }
  return xyz;
}

/* Precomputed data. */
class SkyMultipleScattering {
 public:
  SkyMultipleScattering(const float air_density,
                        const float aerosol_density,
                        const float ozone_density)
      : air_density(air_density), aerosol_density(aerosol_density), ozone_density(ozone_density)
  {
  }

  /* Compute atmosphere's transmittance from the given altitude to the sun. */
  inline float4 get_transmittance(const float cos_theta, const float normalized_altitude) const
  {
    const float3 sun_dir = sun_direction(cos_theta);
    const float distance_to_earth_center = mix(
        EARTH_RADIUS, ATMOSPHERE_RADIUS, normalized_altitude);
    const float3 ray_origin = make_float3(0.0f, 0.0f, distance_to_earth_center);
    const float t_d = ray_sphere_intersection(ray_origin, sun_dir, ATMOSPHERE_RADIUS);
    const float t_step = t_d / TRANSMITTANCE_STEPS;

    float4 result = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
    for (int step = 0; step < TRANSMITTANCE_STEPS; step++) {
      const float t = (step + 0.5f) * t_step;
      const float3 x_t = ray_origin + sun_dir * t;
      const float altitude = fmaxf(x_t.length() - EARTH_RADIUS, 0.0f);
      float4 aerosol_absorption, aerosol_scattering, molecular_absorption, molecular_scattering;
      get_atmosphere_collision_coefficients(altitude,
                                            aerosol_absorption,
                                            aerosol_scattering,
                                            molecular_absorption,
                                            molecular_scattering);
      const float4 extinction = aerosol_absorption + aerosol_scattering + molecular_absorption +
                                molecular_scattering;
      result += extinction * t_step;
    }

    return exp(-result);
  }

  /* Compute in-scattered radiance for the given ray. */
  float4 get_inscattering(const float3 sun_dir,
                          const float3 ray_origin,
                          const float3 ray_dir,
                          const float t_d) const
  {
    const float cos_theta = dot(-ray_dir, sun_dir);
    const float molecular_phase = molecular_phase_function(cos_theta);
    const float aerosol_phase = aerosol_phase_function(cos_theta);
    const float dt = t_d / IN_SCATTERING_STEPS;
    float4 L_inscattering = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 transmittance = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
    for (int i = 0; i < IN_SCATTERING_STEPS; i++) {
      const float t = (i + 0.5f) * dt;
      const float3 x_t = ray_origin + ray_dir * t;
      const float distance_to_earth_center = x_t.length();
      const float3 zenith_dir = x_t / distance_to_earth_center;
      const float altitude = fmaxf(distance_to_earth_center - EARTH_RADIUS, 0.0f);
      const float normalized_altitude = altitude / ATMOSPHERE_THICKNESS;
      const float sample_cos_theta = dot(zenith_dir, sun_dir);
      float4 aerosol_absorption, aerosol_scattering, molecular_absorption, molecular_scattering;
      get_atmosphere_collision_coefficients(altitude,
                                            aerosol_absorption,
                                            aerosol_scattering,
                                            molecular_absorption,
                                            molecular_scattering);
      const float4 extinction = aerosol_absorption + aerosol_scattering + molecular_absorption +
                                molecular_scattering;
      const float4 transmittance_to_sun = lookup_transmittance(sample_cos_theta,
                                                               normalized_altitude);
      const float4 ms = lookup_multiscattering(
          sample_cos_theta, normalized_altitude, distance_to_earth_center);
      const float4 S = SUN_SPECTRAL_IRRADIANCE *
                       (molecular_scattering * (molecular_phase * transmittance_to_sun + ms) +
                        aerosol_scattering * (aerosol_phase * transmittance_to_sun + ms));
      const float4 step_transmittance = exp(-dt * extinction);
      /* Energy-conserving analytical integration "Physically Based Sky, Atmosphere and Cloud
       * Rendering in Frostbite" by Sébastien Hillaire. */
      const float4 cut_ext = max(extinction, 1e-7f);
      const float4 S_int = (S - S * step_transmittance) / cut_ext;
      L_inscattering = L_inscattering + transmittance * S_int;
      transmittance *= step_transmittance;
    }

    return L_inscattering;
  }

  /* Precompute the transmittance LUT. Must be called before get_inscattering(). */
  void precompute_lut()
  {
    SKY_parallel_for(0, TRANSMITTANCE_RES_Y, 4, [&](const size_t begin, const size_t end) {
      for (int y = begin; y < end; y++) {
        for (int x = 0; x < TRANSMITTANCE_RES_X; x++) {
          const float2 uv = make_float2(x / float(TRANSMITTANCE_RES_X - 1),
                                        y / float(TRANSMITTANCE_RES_Y - 1));
          transmittance_lut[y][x] = get_transmittance(uv.x * 2.0f - 1.0f, uv.y);
        }
      }
    });
  }

 protected:
  float4 transmittance_lut[TRANSMITTANCE_RES_Y][TRANSMITTANCE_RES_X];
  float air_density;
  float aerosol_density;
  float ozone_density;

  /* Compute absorption/scattering coeffients at the given altitude. */
  inline void get_atmosphere_collision_coefficients(const float altitude,
                                                    float4 &aerosol_absorption,
                                                    float4 &aerosol_scattering,
                                                    float4 &molecular_absorption,
                                                    float4 &molecular_scattering) const
  {
    const float local_aerosol_density = get_aerosol_density(altitude) * aerosol_density;
    aerosol_absorption = AEROSOL_ABSORPTION_CROSS_SECTION * local_aerosol_density;
    aerosol_scattering = AEROSOL_SCATTERING_CROSS_SECTION * local_aerosol_density;
    molecular_absorption = get_molecular_absorption_coefficient(altitude) * ozone_density;
    molecular_scattering = get_molecular_scattering_coefficient(altitude) * air_density;
  }

  inline float4 lookup_multiscattering(float cos_theta, float normalized_height, float d) const
  {
    /* Solid angle subtended by the planet from a point at d distance from the planet center. */
    const float omega = M_2PI_F * (1.0f - safe_sqrtf(1.0f - sqr(EARTH_RADIUS / d)));
    const float4 T_to_ground = lookup_transmittance_at_ground(cos_theta);
    /* We can split the path into Ground <-> Sample <-> Sun.
     * The LUT gives us both T(Sample,Sun) and T(Ground,Sun) = T(Ground,Sample)*T(Sample,Sun),
     * so we can easily compute T(Ground,Sample) from those two. */
    const float4 T_ground_to_sample = lookup_transmittance_to_sun(0.0f) /
                                      lookup_transmittance_to_sun(normalized_height);
    /* 2nd order scattering from the ground. */
    const float4 L_ground = PHASE_ISOTROPIC * omega * (GROUND_ALBEDO * M_1_PI_F) * T_to_ground *
                            T_ground_to_sample * cos_theta;
    /* Fit of Earth's multiple scattering coming from other points in the atmosphere. */
    const float4 L_ms = 0.02f * make_float4(0.217f, 0.347f, 0.594f, 1.0f) *
                        (1.0f / (1.0f + 5.0f * expf(-17.92f * cos_theta)));
    return L_ms + L_ground;
  }

  /* Look up a transmittance from the precomputed LUT. */
  inline float4 lookup_transmittance(const float cos_theta, const float normalized_altitude) const
  {
    const float u = saturate(cos_theta * 0.5f + 0.5f);
    const float v = saturate(normalized_altitude);
    const float x = float(TRANSMITTANCE_RES_X - 1) * u;
    const float y = float(TRANSMITTANCE_RES_Y - 1) * v;
    const int x1 = int(x);
    const int y1 = int(y);
    const int x2 = min(x1 + 1, TRANSMITTANCE_RES_X - 1);
    const int y2 = min(y1 + 1, TRANSMITTANCE_RES_Y - 1);
    const float fx = x - x1;
    const float fy = y - y1;
    const float4 bottom = mix(transmittance_lut[y1][x1], transmittance_lut[y1][x2], fx);
    const float4 top = mix(transmittance_lut[y2][x1], transmittance_lut[y2][x2], fx);
    return mix(bottom, top, fy);
  }

  /* Specialized versions of lookup_transmittance that skip one interpolation. */
  inline float4 lookup_transmittance_at_ground(const float cos_theta) const
  {
    const float u = saturate(cos_theta * 0.5f + 0.5f);
    const float x = float(TRANSMITTANCE_RES_X - 1) * u;
    const int x1 = int(x);
    const int x2 = min(x1 + 1, TRANSMITTANCE_RES_X - 1);
    const int y = 0;
    const float fx = x - x1;
    return mix(transmittance_lut[y][x1], transmittance_lut[y][x2], fx);
  }

  inline float4 lookup_transmittance_to_sun(const float normalized_altitude) const
  {
    const float v = saturate(normalized_altitude);
    const float y = float(TRANSMITTANCE_RES_Y - 1) * v;
    const int x = TRANSMITTANCE_RES_X - 1;
    const int y1 = int(y);
    const int y2 = min(y1 + 1, TRANSMITTANCE_RES_Y - 1);
    const float fy = y - y1;
    return mix(transmittance_lut[y1][x], transmittance_lut[y2][x], fy);
  }
};

void SKY_multiple_scattering_precompute_texture(float *pixels,
                                                int stride,
                                                int width,
                                                int height,
                                                float sun_elevation,
                                                float altitude,
                                                float air_density,
                                                float aerosol_density,
                                                float ozone_density)
{
  SkyMultipleScattering sms(air_density, aerosol_density, ozone_density);
  sms.precompute_lut();

  /* Clamp altitude to avoid numerical issues. */
  altitude = clamp(altitude, 1.0f, 99999.0f) / 1000.0f;
  const int half_width = width / 2;
  const float sun_zenith_cos_angle = cosf(M_PI_2_F - sun_elevation);
  const float3 sun_dir = sun_direction(sun_zenith_cos_angle);
  const int rows_per_task = std::max(1024 / width, 1);

  SKY_parallel_for(0, height, rows_per_task, [&](const size_t begin, const size_t end) {
    for (int y = begin; y < end; y++) {
      float *pixel_row = pixels + (y * width * stride);
      for (int x = 0; x < half_width; x++) {
        float2 uv = make_float2((x + 0.5f) / width, (y + 0.5f) / height);

        const float azimuth = M_2PI_F * uv.x;
        /* Apply a non-linear transformation to the elevation to dedicate more texels to the
         * horizon, where having more detail matters. */
        const float l = uv.y * 2.0f - 1.0f;
        /* [-pi/2, pi/2]. */
        const float elev = copysignf(sqr(l), l) * M_PI_2_F;
        const float3 ray_dir = make_float3(
            cosf(elev) * cosf(azimuth), cosf(elev) * sinf(azimuth), sinf(elev));
        const float3 ray_origin = make_float3(0.0f, 0.0f, EARTH_RADIUS + altitude);
        const float atmos_dist = ray_sphere_intersection(ray_origin, ray_dir, ATMOSPHERE_RADIUS);
        const float ground_dist = ray_sphere_intersection(ray_origin, ray_dir, EARTH_RADIUS);
        /* If no ground collision then use the distance to the outer atmosphere, else we have a
         * collision with the ground so we use the distance to it. */
        const float t_d = (ground_dist < 0.0f) ? atmos_dist : ground_dist;
        const float4 L = sms.get_inscattering(sun_dir, ray_origin, ray_dir, t_d);
        const float3 sky = spectral_to_xyz(L);

        /* Store pixels. */
        const int pos_x = x * stride;
        pixel_row[pos_x] = sky.x;
        pixel_row[pos_x + 1] = sky.y;
        pixel_row[pos_x + 2] = sky.z;
        /* Mirror pixels. */
        const int mirror_x = (width - x - 1) * stride;
        pixel_row[mirror_x] = sky.x;
        pixel_row[mirror_x + 1] = sky.y;
        pixel_row[mirror_x + 2] = sky.z;
      }
    }
  });
}

void SKY_multiple_scattering_precompute_sun(float sun_elevation,
                                            float angular_diameter,
                                            float altitude,
                                            float air_density,
                                            float aerosol_density,
                                            float ozone_density,
                                            float r_pixel_bottom[3],
                                            float r_pixel_top[3])
{
  const SkyMultipleScattering sms(air_density, aerosol_density, ozone_density);

  /* Clamp altitude to avoid numerical issues. */
  altitude = clamp(altitude, 1.0f, 99999.0f) / 1000.0f;
  const float half_angular = angular_diameter / 2.0f;
  const float solid_angle = M_2PI_F * (1.0f - cosf(half_angular));
  const float normalized_altitude = altitude / ATMOSPHERE_THICKNESS;

  /* Compute 2 pixels for Sun disc: one is the lowest point of the disc, one is the highest. */
  auto get_sun_xyz = [&](const float elevation) {
    const float sun_zenith_cos_angle = cosf(M_PI_2_F - elevation);
    const float4 transmittance_to_sun = sms.get_transmittance(sun_zenith_cos_angle,
                                                              normalized_altitude);
    const float4 spectrum = SUN_SPECTRAL_IRRADIANCE * transmittance_to_sun / solid_angle;
    return spectral_to_xyz(spectrum);
  };
  const float3 bottom = get_sun_xyz(sun_elevation - half_angular);
  const float3 top = get_sun_xyz(sun_elevation + half_angular);

  /* Store pixels */
  r_pixel_bottom[0] = bottom.x;
  r_pixel_bottom[1] = bottom.y;
  r_pixel_bottom[2] = bottom.z;
  r_pixel_top[0] = top.x;
  r_pixel_top[1] = top.y;
  r_pixel_top[2] = top.z;
}

float SKY_earth_intersection_angle(float altitude)
{
  /* Calculate intersection angle between line passing through viewpoint and Earth surface. */
  return M_PI_2_F - asinf(EARTH_RADIUS / (EARTH_RADIUS + altitude / 1000.0f));
}
