/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_ocean.h"
#include "BLI_math.h"
#include "ocean_intern.h"

#ifdef WITH_OCEANSIM

/* -------------------------------------------------------------------- */
/** \name Ocean Spectrum from EncinoWaves
 * \{ */

/*
 * Original code from EncinoWaves project Copyright (c) 2015 Christopher Jon Horvath
 * Modifications made to work within blender.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

/**
 * alpha_beta_spectrum is a common algorithm for the Pierson-Moskowitz, JONSWAP and TMA models.
 * This is a modified implementation from the EncinoWaves project.
 */
static float alpha_beta_spectrum(const float alpha,
                                 const float beta,
                                 const float gamma,
                                 const float omega,
                                 const float peakomega)
{
  return (alpha * sqrt(gamma) / pow(omega, 5.0)) * exp(-beta * pow(peakomega / omega, 4.0));
}

static float peak_sharpen(const float omega, const float m_peakomega, const float m_gamma)
{
  const float peak_sharpening_sigma = (omega < m_peakomega) ? 0.07 : 0.09;
  const float peak_sharpening = pow(
      m_gamma, exp(-sqrt((omega - m_peakomega) / (peak_sharpening_sigma * m_peakomega)) / 2.0));

  return peak_sharpening;
}

/**
 * Spectrum-type independent modifications.
 */
static float ocean_spectrum_wind_and_damp(const Ocean *oc,
                                          const float kx,
                                          const float kz,
                                          const float val)
{
  const float k2 = kx * kx + kz * kz;
  const float k_mag_inv = 1.0f / k2;
  const float k_dot_w = (kx * k_mag_inv * oc->_wx) + (kz * k_mag_inv * oc->_wz);

  /* Bias towards wind dir. */
  float newval = val * pow(fabs(k_dot_w), oc->_wind_alignment);

  /* Eliminate wavelengths smaller than cutoff. */
  /* val *= exp(-k2 * m_cutoff); */

  /* Reduce reflected waves. */
  if (k_dot_w < 0.0f) {
    if (oc->_wind_alignment > 0.0) {
      newval *= oc->_damp_reflections;
    }
  }

  return newval;
}

static float jonswap(const Ocean *oc, const float k2)
{
  /* Get our basic JONSWAP value from #alpha_beta_spectrum. */
  const float k_mag = sqrt(k2);

  const float m_omega = GRAVITY * k_mag * tanh(k_mag * oc->_depth);
  const float omega = sqrt(m_omega);

  const float m_fetch = oc->_fetch_jonswap;

  /* Strictly, this should be a random value from a Gaussian (mean 3.3, variance 0.67),
   * clamped 1.0 to 6.0. */
  float m_gamma = oc->_sharpen_peak_jonswap;
  if (m_gamma < 1.0) {
    m_gamma = 1.00;
  }
  if (m_gamma > 6.0) {
    m_gamma = 6.0;
  }

  const float m_windspeed = oc->_V;

  const float m_dimensionlessFetch = fabs(GRAVITY * m_fetch / sqrt(m_windspeed));
  const float m_alpha = 0.076 * pow(m_dimensionlessFetch, -0.22);

  const float m_tau = M_PI * 2;
  const float m_peakomega = m_tau * 3.5 * fabs(GRAVITY / oc->_V) *
                            pow(m_dimensionlessFetch, -0.33);

  const float beta = 1.25f;

  float val = alpha_beta_spectrum(m_alpha, beta, GRAVITY, omega, m_peakomega);

  /* Peak sharpening  */
  val *= peak_sharpen(m_omega, m_peakomega, m_gamma);

  return val;
}

/**
 * Pierson-Moskowitz model, 1964, assumes waves reach equilibrium with wind.
 * Model is intended for large area 'fully developed' sea, where winds have been steadily blowing
 * for days over an area that includes hundreds of wavelengths on a side.
 */
float BLI_ocean_spectrum_piersonmoskowitz(const Ocean *oc, const float kx, const float kz)
{
  const float k2 = kx * kx + kz * kz;

  if (k2 == 0.0f) {
    /* No DC component. */
    return 0.0f;
  }

  /* Get Pierson-Moskowitz value from #alpha_beta_spectrum. */
  const float peak_omega_PM = 0.87f * GRAVITY / oc->_V;

  const float k_mag = sqrt(k2);
  const float m_omega = GRAVITY * k_mag * tanh(k_mag * oc->_depth);

  const float omega = sqrt(m_omega);
  const float alpha = 0.0081f;
  const float beta = 1.291f;

  float val = alpha_beta_spectrum(alpha, beta, GRAVITY, omega, peak_omega_PM);

  val = ocean_spectrum_wind_and_damp(oc, kx, kz, val);

  return val;
}

/**
 * TMA extends the JONSWAP spectrum.
 * This spectral model is best suited to shallow water.
 */
float BLI_ocean_spectrum_texelmarsenarsloe(const Ocean *oc, const float kx, const float kz)
{
  const float k2 = kx * kx + kz * kz;

  if (k2 == 0.0f) {
    /* No DC component. */
    return 0.0f;
  }

  float val = jonswap(oc, k2);

  val = ocean_spectrum_wind_and_damp(oc, kx, kz, val);

  /* TMA modifications to JONSWAP. */
  const float m_depth = oc->_depth;
  const float gain = sqrt(m_depth / GRAVITY);

  const float k_mag = sqrt(k2);

  const float m_omega = GRAVITY * k_mag * tanh(k_mag * oc->_depth);
  const float omega = sqrt(m_omega);

  const float kitaigorodskiiDepth_wh = omega * gain;
  const float kitaigorodskiiDepth = 0.5 + (0.5 * tanh(1.8 * (kitaigorodskiiDepth_wh - 1.125)));

  val *= kitaigorodskiiDepth;

  val = ocean_spectrum_wind_and_damp(oc, kx, kz, val);

  return val;
}

/**
 * Hasselmann et al, 1973. This model extends the Pierson-Moskowitz model with a peak sharpening
 * function This enhancement is an artificial construct to address the problem that the wave
 * spectrum is never fully developed.
 *
 * The fetch parameter represents the distance from a lee shore,
 * called the fetch, or the distance over which the wind blows with constant velocity.
 */
float BLI_ocean_spectrum_jonswap(const Ocean *oc, const float kx, const float kz)
{
  const float k2 = kx * kx + kz * kz;

  if (k2 == 0.0f) {
    /* No DC component. */
    return 0.0f;
  }

  float val = jonswap(oc, k2);

  val = ocean_spectrum_wind_and_damp(oc, kx, kz, val);

  return val;
}

/** \} */

#endif /* WITH_OCEANSIM */
