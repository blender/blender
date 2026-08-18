/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/tables.h"

#include "util/types_spectrum.h"

CCL_NAMESPACE_BEGIN

ccl_device float3 xyz_to_rgb(KernelGlobals kg, const float3 xyz)
{
  return make_float3(dot(make_float3(kernel_data.film.xyz_to_r), xyz),
                     dot(make_float3(kernel_data.film.xyz_to_g), xyz),
                     dot(make_float3(kernel_data.film.xyz_to_b), xyz));
}

ccl_device float3 xyz_to_rgb_clamped(KernelGlobals kg, const float3 xyz)
{
  return max(xyz_to_rgb(kg, xyz), zero_float3());
}

ccl_device float3 rec709_to_rgb(KernelGlobals kg, const float3 rec709)
{
  return (kernel_data.film.is_rec709) ?
             rec709 :
             make_float3(dot(make_float3(kernel_data.film.rec709_to_r), rec709),
                         dot(make_float3(kernel_data.film.rec709_to_g), rec709),
                         dot(make_float3(kernel_data.film.rec709_to_b), rec709));
}

template<class T> ccl_device auto linear_rgb_to_gray(KernelGlobals kg, const T c)
{
  return dot(c, make_float3(kernel_data.film.rgb_to_y));
}

ccl_device_inline Spectrum rgb_to_spectrum(const float3 rgb)
{
  return rgb;
}

ccl_device_inline float3 spectrum_to_rgb(Spectrum s)
{
  return s;
}

ccl_device float spectrum_to_gray(KernelGlobals kg, Spectrum c)
{
  return linear_rgb_to_gray(kg, spectrum_to_rgb(c));
}

#ifdef __SPECTRAL__
/* Given a wavelength, convert it to rgb in the working color space, assuming D65 illuminant.
 * [Metameric: Spectral Uplifting via Controllable Color Constraints]
 * (https://markvanderuit.nl/files/2023-07-23-paper-metameric/metameric-paper.pdf)
 * by Mark van de Ruit and Elmar Eisemann, Eq. (1). */
ccl_device_inline float3 wavelength_to_rgb_d65(KernelGlobals kg, const float lambda_um)
{
  int i;
  const float f = floorfrac((lambda_um - WAVELENGTH_CIE_MIN) / WAVELENGTH_DLAMBDA, &i);

  if (i < 0 || i >= WAVELENGTH_RESOLUTION) {
    return zero_float3();
  }

  ccl_constant float *c = cie_color_match[i];
  float3 xyz = mix(make_float3(c[0], c[1], c[2]), make_float3(c[3], c[4], c[5]), f);
  xyz *= mix(cie_d65_spd[i], cie_d65_spd[i + 1], f);

  return xyz_to_rgb(kg, xyz * CIE_D65_NORMALIZATION);
}

/* Sample wavelength in um.
 * See `intern/cycles/app/cie_d65_luminance_fit.py` for the fitting. */
ccl_device_inline float sample_wavelength(float rand, ccl_private float *prob = nullptr)
{
  constexpr float a = 21.71348444564851f;
  constexpr float x0 = 0.5554867905834258f;
  constexpr float y0 = 0.021659159132699574f;
  constexpr float N = 0.9707633294863183f;

  rand = N * rand + y0;

  if (prob) {
    /* The derivative of F(x) = 1/(1+e^-(a(x-x0))) is F'(x) = a * F(x) * (1 - F(x)). */
    *prob = a * rand * (1.0f - rand);
    /* Normalization */
    *prob *= (WAVELENGTH_CIE_MAX - WAVELENGTH_CIE_MIN) / N;
  }

  const float wavelength = -fast_logf(1.0f / rand - 1.0f) / a + x0;
  return clamp(wavelength, WAVELENGTH_CIE_MIN, WAVELENGTH_CIE_MAX);
}
#endif

CCL_NAMESPACE_END
