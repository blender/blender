/* SPDX-FileCopyrightText: 2012-2013 Lukas Hosek and Alexander Wilkie. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause */

/* ============================================================================

This file is part of a sample implementation of the analytical skylight and
solar radiance models presented in the SIGGRAPH 2012 paper


           "An Analytic Model for Full Spectral Sky-Dome Radiance"

and the 2013 IEEE CG&A paper

       "Adding a Solar Radiance Function to the Hosek Skylight Model"

                                   both by

                       Lukas Hosek and Alexander Wilkie
                Charles University in Prague, Czech Republic


                        Version: 1.4a, February 22nd, 2013

Version history:

1.4a  February 22nd, 2013
      Removed unnecessary and counter-intuitive solar radius parameters
      from the interface of the color-space sky dome initialization functions.

1.4   February 11th, 2013
      Fixed a bug which caused the relative brightness of the solar disc
      and the sky dome to be off by a factor of about 6. The sun was too
      bright: this affected both normal and alien sun scenarios. The
      coefficients of the solar radiance function were changed to fix this.

1.3   January 21st, 2013 (not released to the public)
      Added support for solar discs that are not exactly the same size as
      the terrestrial sun. Also added support for suns with a different
      emission spectrum ("Alien World" functionality).

1.2a  December 18th, 2012
      Fixed a mistake and some inaccuracies in the solar radiance function
      explanations found in ArHosekSkyModel.h. The actual source code is
      unchanged compared to version 1.2.

1.2   December 17th, 2012
      Native RGB data and a solar radiance function that matches the turbidity
      conditions were added.

1.1   September 2012
      The coefficients of the spectral model are now scaled so that the output
      is given in physical units: W / (m^-2 * sr * nm). Also, the output of the
      XYZ model is now no longer scaled to the range [0...1]. Instead, it is
      the result of a simple conversion from spectral data via the CIE 2 degree
      standard observer matching functions. Therefore, after multiplication
      with 683 lm / W, the Y channel now corresponds to luminance in lm.

1.0   May 11th, 2012
      Initial release.


Please visit http://cgg.mff.cuni.cz/projects/SkylightModelling/ to check if
an updated version of this code has been published!

============================================================================ */

/*

All instructions on how to use this code are in the accompanying header file.

*/

/** \file
 * \ingroup intern_sky_modal
 */

#include "sky_hosek.h"
#include "sky_hosek_data.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

//   Some macro definitions that occur elsewhere in ART, and that have to be
//   replicated to make this a stand-alone module.

#ifndef MATH_PI
#  define MATH_PI 3.141592653589793
#endif

#ifndef MATH_DEG_TO_RAD
#  define MATH_DEG_TO_RAD (MATH_PI / 180.0)
#endif

#ifndef DEGREES
#  define DEGREES *MATH_DEG_TO_RAD
#endif

#ifndef TERRESTRIAL_SOLAR_RADIUS
#  define TERRESTRIAL_SOLAR_RADIUS ((0.51 DEGREES) / 2.0)
#endif

#ifndef ALLOC
#  define ALLOC(_struct) ((_struct *)malloc(sizeof(_struct)))
#endif

/* Not defined on all platforms (macOS & WIN32). */
using uint = unsigned int;

// internal definitions

using ArHosekSkyModel_Dataset = const double *;
using ArHosekSkyModel_Radiance_Dataset = const double *;

// internal functions

static void ArHosekSkyModel_CookConfiguration(ArHosekSkyModel_Dataset dataset,
                                              SKY_ArHosekSkyModelConfiguration config,
                                              double turbidity,
                                              double albedo,
                                              double solar_elevation)
{
  const double *elev_matrix;

  int int_turbidity = int(turbidity);
  double turbidity_rem = turbidity - double(int_turbidity);

  solar_elevation = pow(solar_elevation / (MATH_PI / 2.0), (1.0 / 3.0));

  // alb 0 low turb

  elev_matrix = dataset + (9 * 6 * (int_turbidity - 1));

  for (uint i = 0; i < 9; ++i) {
    //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
    config[i] =
        (1.0 - albedo) * (1.0 - turbidity_rem) *
        (pow(1.0 - solar_elevation, 5.0) * elev_matrix[i] +
         5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[i + 9] +
         10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[i + 18] +
         10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[i + 27] +
         5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[i + 36] +
         pow(solar_elevation, 5.0) * elev_matrix[i + 45]);
  }

  // alb 1 low turb
  elev_matrix = dataset + (9 * 6 * 10 + 9 * 6 * (int_turbidity - 1));
  for (uint i = 0; i < 9; ++i) {
    //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
    config[i] +=
        (albedo) * (1.0 - turbidity_rem) *
        (pow(1.0 - solar_elevation, 5.0) * elev_matrix[i] +
         5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[i + 9] +
         10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[i + 18] +
         10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[i + 27] +
         5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[i + 36] +
         pow(solar_elevation, 5.0) * elev_matrix[i + 45]);
  }

  if (int_turbidity == 10) {
    return;
  }

  // alb 0 high turb
  elev_matrix = dataset + (9 * 6 * (int_turbidity));
  for (uint i = 0; i < 9; ++i) {
    //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
    config[i] +=
        (1.0 - albedo) * (turbidity_rem) *
        (pow(1.0 - solar_elevation, 5.0) * elev_matrix[i] +
         5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[i + 9] +
         10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[i + 18] +
         10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[i + 27] +
         5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[i + 36] +
         pow(solar_elevation, 5.0) * elev_matrix[i + 45]);
  }

  // alb 1 high turb
  elev_matrix = dataset + (9 * 6 * 10 + 9 * 6 * (int_turbidity));
  for (uint i = 0; i < 9; ++i) {
    //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
    config[i] +=
        (albedo) * (turbidity_rem) *
        (pow(1.0 - solar_elevation, 5.0) * elev_matrix[i] +
         5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[i + 9] +
         10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[i + 18] +
         10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[i + 27] +
         5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[i + 36] +
         pow(solar_elevation, 5.0) * elev_matrix[i + 45]);
  }
}

static double ArHosekSkyModel_CookRadianceConfiguration(ArHosekSkyModel_Radiance_Dataset dataset,
                                                        double turbidity,
                                                        double albedo,
                                                        double solar_elevation)
{
  const double *elev_matrix;

  int int_turbidity = int(turbidity);
  double turbidity_rem = turbidity - double(int_turbidity);
  double res;
  solar_elevation = pow(solar_elevation / (MATH_PI / 2.0), (1.0 / 3.0));

  // alb 0 low turb
  elev_matrix = dataset + (6 * (int_turbidity - 1));
  //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
  res = (1.0 - albedo) * (1.0 - turbidity_rem) *
        (pow(1.0 - solar_elevation, 5.0) * elev_matrix[0] +
         5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[1] +
         10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[2] +
         10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[3] +
         5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[4] +
         pow(solar_elevation, 5.0) * elev_matrix[5]);

  // alb 1 low turb
  elev_matrix = dataset + (6 * 10 + 6 * (int_turbidity - 1));
  //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
  res += (albedo) * (1.0 - turbidity_rem) *
         (pow(1.0 - solar_elevation, 5.0) * elev_matrix[0] +
          5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[1] +
          10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[2] +
          10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[3] +
          5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[4] +
          pow(solar_elevation, 5.0) * elev_matrix[5]);
  if (int_turbidity == 10) {
    return res;
  }

  // alb 0 high turb
  elev_matrix = dataset + (6 * (int_turbidity));
  //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
  res += (1.0 - albedo) * (turbidity_rem) *
         (pow(1.0 - solar_elevation, 5.0) * elev_matrix[0] +
          5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[1] +
          10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[2] +
          10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[3] +
          5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[4] +
          pow(solar_elevation, 5.0) * elev_matrix[5]);

  // alb 1 high turb
  elev_matrix = dataset + (6 * 10 + 6 * (int_turbidity));
  //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
  res += (albedo) * (turbidity_rem) *
         (pow(1.0 - solar_elevation, 5.0) * elev_matrix[0] +
          5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[1] +
          10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[2] +
          10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[3] +
          5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[4] +
          pow(solar_elevation, 5.0) * elev_matrix[5]);
  return res;
}

static double ArHosekSkyModel_GetRadianceInternal(
    const SKY_ArHosekSkyModelConfiguration configuration, const double theta, const double gamma)
{
  const double expM = exp(configuration[4] * gamma);
  const double rayM = cos(gamma) * cos(gamma);
  const double mieM =
      (1.0 + cos(gamma) * cos(gamma)) /
      pow((1.0 + configuration[8] * configuration[8] - 2.0 * configuration[8] * cos(gamma)), 1.5);
  const double zenith = sqrt(cos(theta));

  return (1.0 + configuration[0] * exp(configuration[1] / (cos(theta) + 0.01))) *
         (configuration[2] + configuration[3] * expM + configuration[5] * rayM +
          configuration[6] * mieM + configuration[7] * zenith);
}

void SKY_arhosekskymodelstate_free(SKY_ArHosekSkyModelState *state)
{
  free(state);
}

double SKY_arhosekskymodel_radiance(SKY_ArHosekSkyModelState *state,
                                    double theta,
                                    double gamma,
                                    double wavelength)
{
  int low_wl = int((wavelength - 320.0) / 40.0);

  if (low_wl < 0 || low_wl >= 11) {
    return 0.0;
  }

  double interp = fmod((wavelength - 320.0) / 40.0, 1.0);

  double val_low = ArHosekSkyModel_GetRadianceInternal(state->configs[low_wl], theta, gamma) *
                   state->radiances[low_wl] * state->emission_correction_factor_sky[low_wl];

  if (interp < 1e-6) {
    return val_low;
  }

  double result = (1.0 - interp) * val_low;

  if (low_wl + 1 < 11) {
    result += interp *
              ArHosekSkyModel_GetRadianceInternal(state->configs[low_wl + 1], theta, gamma) *
              state->radiances[low_wl + 1] * state->emission_correction_factor_sky[low_wl + 1];
  }

  return result;
}

// xyz and rgb versions

SKY_ArHosekSkyModelState *SKY_arhosek_xyz_skymodelstate_alloc_init(const double turbidity,
                                                                   const double albedo,
                                                                   const double elevation)
{
  SKY_ArHosekSkyModelState *state = ALLOC(SKY_ArHosekSkyModelState);

  state->solar_radius = TERRESTRIAL_SOLAR_RADIUS;
  state->turbidity = turbidity;
  state->albedo = albedo;
  state->elevation = elevation;

  for (uint channel = 0; channel < 3; ++channel) {
    ArHosekSkyModel_CookConfiguration(
        datasetsXYZ[channel], state->configs[channel], turbidity, albedo, elevation);

    state->radiances[channel] = ArHosekSkyModel_CookRadianceConfiguration(
        datasetsXYZRad[channel], turbidity, albedo, elevation);
  }

  return state;
}
