/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

class Context;

/* ------------------------------------------------------------------------------------------------
 * Deriche Gaussian Coefficients Key.
 */
class DericheGaussianCoefficientsKey {
 public:
  float sigma;

  DericheGaussianCoefficientsKey(float sigma);

  uint64_t hash() const;
};

bool operator==(const DericheGaussianCoefficientsKey &a, const DericheGaussianCoefficientsKey &b);

/* -------------------------------------------------------------------------------------------------
 * Deriche Gaussian Coefficients.
 *
 * A caches resource that computes and caches the coefficients of the fourth order IIR filter
 * approximating a Gaussian filter computed using Deriche's design method. This is based on the
 * following paper:
 *
 *   Deriche, Rachid. Recursively implementating the Gaussian and its derivatives. Diss. INRIA,
 *   1993.
 */
class DericheGaussianCoefficients : public CachedResource {
 private:
  /* The d_ii coefficients in Equation (28) and (29). Those are the same for the causal and non
   * causal filters as can be seen in Equation (31). */
  double4 feedback_coefficients_;
  /* The n_ii^+ coefficients in Equation (28). */
  double4 causal_feedforward_coefficients_;
  /* The n_ii^- coefficients in Equation (29). */
  double4 non_causal_feedforward_coefficients_;
  /* The difference equation in Equation (28) rely on previous outputs to compute the new output,
   * and those previous outputs need to be properly initialized somehow. To do Neumann boundary
   * condition, we multiply the boundary value with this coefficient to simulate an infinite stream
   * of the boundary value. See the implementation for more information. */
  double causal_boundary_coefficient_;
  /* Same as causal_boundary_coefficient_ but for the non causal filter. */
  double non_causal_boundary_coefficient_;

 public:
  DericheGaussianCoefficients(Context &context, float sigma);

  const double4 &feedback_coefficients() const;
  const double4 &causal_feedforward_coefficients() const;
  const double4 &non_causal_feedforward_coefficients() const;
  double causal_boundary_coefficient() const;
  double non_causal_boundary_coefficient() const;
};

/* ------------------------------------------------------------------------------------------------
 * Deriche Gaussian Coefficients Container.
 */
class DericheGaussianCoefficientsContainer : CachedResourceContainer {
 private:
  Map<DericheGaussianCoefficientsKey, std::unique_ptr<DericheGaussianCoefficients>> map_;

 public:
  void reset() override;

  /* Check if there is an available DericheGaussianCoefficients cached resource with the given
   * parameters in the container, if one exists, return it, otherwise, return a newly created one
   * and add it to the container. In both cases, tag the cached resource as needed to keep it
   * cached for the next evaluation. */
  DericheGaussianCoefficients &get(Context &context, float sigma);
};

}  // namespace blender::realtime_compositor
