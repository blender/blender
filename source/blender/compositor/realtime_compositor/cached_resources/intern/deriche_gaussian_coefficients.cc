/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* -------------------------------------------------------------------------------------------------
 * Deriche Gaussian Coefficients.
 *
 * Computes the coefficients of the fourth order IIR filter approximating a Gaussian filter
 * computed using Deriche's design method. This is based on the following paper:
 *
 *   Deriche, Rachid. Recursively implementating the Gaussian and its derivatives. Diss. INRIA,
 *   1993.
 *
 * But with corrections in the normalization scale from the following paper, as will be seen in the
 * implementation:
 *
 *   Farneback, Gunnar, and Carl-Fredrik Westin. Improving Deriche-style recursive Gaussian
 *   filters. Journal of Mathematical Imaging and Vision 26.3 (2006): 293-299.
 *
 * The Deriche filter is computed as the sum of a causal and a non causal sequence of second order
 * difference equations as can be seen in Equation (30) in Deriche's paper, and the target of this
 * class is to compute the feedback, causal feedforward, and non causal feedforward coefficients of
 * the filter. */

#include <cstdint>
#include <memory>

#include "BLI_hash.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"

#include "COM_context.hh"
#include "COM_deriche_gaussian_coefficients.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Deriche Gaussian Coefficients Key.
 */

DericheGaussianCoefficientsKey::DericheGaussianCoefficientsKey(float sigma) : sigma(sigma) {}

uint64_t DericheGaussianCoefficientsKey::hash() const
{
  return get_default_hash(sigma);
}

bool operator==(const DericheGaussianCoefficientsKey &a, const DericheGaussianCoefficientsKey &b)
{
  return a.sigma == b.sigma;
}

/* -------------------------------------------------------------------------------------------------
 * Deriche Gaussian Coefficients.
 */

/* The base constant coefficients computed using Deriche's method with 10 digits of precision.
 * Those are available in Deriche's paper by comparing Equations (19) and (38). */
inline constexpr static double a0 = 1.6797292232361107;
inline constexpr static double a1 = 3.7348298269103580;
inline constexpr static double b0 = 1.7831906544515104;
inline constexpr static double b1 = 1.7228297663338028;
inline constexpr static double c0 = -0.6802783501806897;
inline constexpr static double c1 = -0.2598300478959625;
inline constexpr static double w0 = 0.6318113174569493;
inline constexpr static double w1 = 1.9969276832487770;

/* Computes n00 in Equation (21) in Deriche's paper. */
static double compute_numerator_0()
{
  return a0 + c0;
}

/* Computes n11 in Equation (21) in Deriche's paper. */
static double compute_numerator_1(float sigma)
{
  const double multiplier1 = math::exp(-b1 / sigma);
  const double term1 = c1 * math::sin(w1 / sigma) - (c0 + 2.0 * a0) * math::cos(w1 / sigma);

  const double multiplier2 = math::exp(-b0 / sigma);
  const double term2 = a1 * math::sin(w0 / sigma) - (2.0 * c0 + a0) * math::cos(w0 / sigma);

  return multiplier1 * term1 + multiplier2 * term2;
}

/* Computes n22 in Equation (21) in Deriche's paper. */
static double compute_numerator_2(float sigma)
{
  const double multiplier1 = 2.0 * math::exp(-(b0 / sigma) - (b1 / sigma));
  const double term11 = (a0 + c0) * math::cos(w1 / sigma) * math::cos(w0 / sigma);
  const double term12 = math::cos(w1 / sigma) * a1 * math::sin(w0 / sigma);
  const double term13 = math::cos(w0 / sigma) * c1 * math::sin(w1 / sigma);
  const double term1 = term11 - term12 - term13;

  const double term2 = c0 * math::exp(-2.0 * (b0 / sigma));
  const double term3 = a0 * math::exp(-2.0 * (b1 / sigma));

  return multiplier1 * term1 + term2 + term3;
}

/* Computes n33 in Equation (21) in Deriche's paper. */
static double compute_numerator_3(float sigma)
{
  const double multiplier1 = math::exp(-(b1 / sigma) - 2.0 * (b0 / sigma));
  const double term1 = c1 * math::sin(w1 / sigma) - math::cos(w1 / sigma) * c0;

  const double multiplier2 = math::exp(-(b0 / sigma) - 2.0 * (b1 / sigma));
  const double term2 = a1 * math::sin(w0 / sigma) - math::cos(w0 / sigma) * a0;

  return multiplier1 * term1 + multiplier2 * term2;
}

/* Computes and packs the numerators in Equation (21) in Deriche's paper. */
static double4 compute_numerator(float sigma)
{
  const double n0 = compute_numerator_0();
  const double n1 = compute_numerator_1(sigma);
  const double n2 = compute_numerator_2(sigma);
  const double n3 = compute_numerator_3(sigma);

  return double4(n0, n1, n2, n3);
}

/* Computes d11 in Equation (22) in Deriche's paper. */
static double compute_denominator_1(float sigma)
{
  const double term1 = -2.0 * math::exp(-(b0 / sigma)) * math::cos(w0 / sigma);
  const double term2 = 2.0 * math::exp(-(b1 / sigma)) * math::cos(w1 / sigma);

  return term1 - term2;
}

/* Computes d22 in Equation (22) in Deriche's paper. */
static double compute_denominator_2(float sigma)
{
  const double term1 = 4.0 * math::cos(w1 / sigma) * math::cos(w0 / sigma);
  const double multiplier1 = math::exp(-(b0 / sigma) - (b1 / sigma));

  const double term2 = math::exp(-2.0 * (b1 / sigma));
  const double term3 = math::exp(-2.0 * (b0 / sigma));

  return term1 * multiplier1 + term2 + term3;
}

/* Computes d33 in Equation (22) in Deriche's paper. */
static double compute_denominator_3(float sigma)
{
  const double term1 = -2.0 * math::cos(w0 / sigma);
  const double multiplier1 = math::exp(-(b0 / sigma) - 2.0 * (b1 / sigma));

  const double term2 = 2.0 * math::cos(w1 / sigma);
  const double multiplier2 = math::exp(-(b1 / sigma) - 2.0 * (b0 / sigma));

  return term1 * multiplier1 - term2 * multiplier2;
}

/* Computes d44 in Equation (22) in Deriche's paper. */
static double compute_denominator_4(float sigma)
{
  return math::exp(-2.0 * (b0 / sigma) - 2.0 * (b1 / sigma));
}

/* Computes and packs the denominators in Equation (22) in Deriche's paper. */
static double4 compute_denominator(float sigma)
{
  const double d1 = compute_denominator_1(sigma);
  const double d2 = compute_denominator_2(sigma);
  const double d3 = compute_denominator_3(sigma);
  const double d4 = compute_denominator_4(sigma);

  return double4(d1, d2, d3, d4);
}

/* Computes the normalization scale that the feedforward coefficients should be divided by to
 * match the unit integral of the Gaussian. The scaling factor proposed by Deriche's paper in
 * Equation (50) is wrong due to missing terms. A correct scaling factor is presented in
 * Farneback's paper in Equation (25), which is implemented in this method. */
static float compute_normalization_scale(const double4 &causal_feedforward_coefficients,
                                         const double4 &feedback_coefficients)
{
  const double causal_feedforwad_sum = math::reduce_add(causal_feedforward_coefficients);
  const double feedback_sum = 1.0 + math::reduce_add(feedback_coefficients);
  return 2.0 * (causal_feedforwad_sum / feedback_sum) - causal_feedforward_coefficients[0];
}

/* Computes the non causal feedforward coefficients from the feedback and causal feedforward
 * coefficients based on Equation (31) in Deriche's paper. Notice that the equation is linear, so
 * the coefficients can be computed after the normalization of the causal feedforward
 * coefficients. */
static double4 compute_non_causal_feedforward_coefficients(
    const double4 &causal_feedforward_coefficients, const double4 &feedback_coefficients)
{
  const double n1 = causal_feedforward_coefficients[1] -
                    feedback_coefficients[0] * causal_feedforward_coefficients[0];
  const double n2 = causal_feedforward_coefficients[2] -
                    feedback_coefficients[1] * causal_feedforward_coefficients[0];
  const double n3 = causal_feedforward_coefficients[3] -
                    feedback_coefficients[2] * causal_feedforward_coefficients[0];
  const double n4 = -feedback_coefficients[3] * causal_feedforward_coefficients[0];

  return double4(n1, n2, n3, n4);
}

/* The IIR filter difference equation relies on previous outputs to compute new outputs, those
 * previous outputs are not really defined at the start of the filter. To do Neumann boundary
 * condition, we initialize the previous output with a special value that is a function of the
 * boundary value. This special value is computed by multiply the boundary value with a coefficient
 * to simulate an infinite stream of the boundary value.
 *
 * The function for the coefficient can be derived by substituting the boundary value for previous
 * inputs, equating all current and previous outputs to the same value, and finally rearranging to
 * compute that same output value.
 *
 * Start by the difference equation where b_i are the feedforward coefficients and a_i are the
 * feedback coefficients:
 *
 *   y[n] = \sum_{i = 0}^3 b_i x[n - i] - \sum_{i = 0}^3 a_i y[n - i]
 *
 * Assume all outputs are y and all inputs are x, which is the boundary value:
 *
 *   y = \sum_{i = 0}^3 b_i x - \sum_{i = 0}^3 a_i y
 *
 * Now rearrange to compute y:
 *
 *   y = x \sum_{i = 0}^3 b_i - y \sum_{i = 0}^3 a_i
 *   y + y \sum_{i = 0}^3 a_i = x \sum_{i = 0}^3 b_i
 *   y (1 + \sum_{i = 0}^3 a_i) = x \sum_{i = 0}^3 b_i
 *   y = x \cdot \frac{\sum_{i = 0}^3 b_i}{1 + \sum_{i = 0}^3 a_i}
 *
 * So our coefficient is the value that is multiplied by the boundary value x. Had x been zero,
 * that is, we are doing Dirichlet boundary condition, the equations still hold. */
static double compute_boundary_coefficient(const double4 &feedforward_coefficients,
                                           const double4 &feedback_coefficients)
{
  return math::reduce_add(feedforward_coefficients) /
         (1.0 + math::reduce_add(feedback_coefficients));
}

/* Computes the feedback, causal feedforward, and non causal feedforward coefficients given a
 * target Gaussian sigma value as used in Equations (28) and (29) in Deriche's paper. */
DericheGaussianCoefficients::DericheGaussianCoefficients(Context & /*context*/, float sigma)
{
  /* The numerator coefficients are the causal feedforward coefficients and the denominator
   * coefficients are the feedback coefficients as can be seen in Equation (28). */
  causal_feedforward_coefficients_ = compute_numerator(sigma);
  feedback_coefficients_ = compute_denominator(sigma);

  /* Normalize the feedforward coefficients as discussed in Section "5.4 Normalization" in
   * Deriche's paper. Feedback coefficients do not need normalization. */
  causal_feedforward_coefficients_ /= compute_normalization_scale(causal_feedforward_coefficients_,
                                                                  feedback_coefficients_);

  /* Compute the non causal feedforward coefficients from the feedback and normalized causal
   * feedforward coefficients based on Equation (31) from Deriche's paper. Since the causal
   * coefficients are already normalized, this doesn't need normalization. */
  non_causal_feedforward_coefficients_ = compute_non_causal_feedforward_coefficients(
      causal_feedforward_coefficients_, feedback_coefficients_);

  /* Compute the boundary coefficient for both the causal and non causal filters. */
  causal_boundary_coefficient_ = compute_boundary_coefficient(causal_feedforward_coefficients_,
                                                              feedback_coefficients_);
  non_causal_boundary_coefficient_ = compute_boundary_coefficient(
      non_causal_feedforward_coefficients_, feedback_coefficients_);
}

const double4 &DericheGaussianCoefficients::feedback_coefficients() const
{
  return feedback_coefficients_;
}

const double4 &DericheGaussianCoefficients::causal_feedforward_coefficients() const
{
  return causal_feedforward_coefficients_;
}

const double4 &DericheGaussianCoefficients::non_causal_feedforward_coefficients() const
{
  return non_causal_feedforward_coefficients_;
}

double DericheGaussianCoefficients::causal_boundary_coefficient() const
{
  return causal_boundary_coefficient_;
}

double DericheGaussianCoefficients::non_causal_boundary_coefficient() const
{
  return non_causal_boundary_coefficient_;
}

/* --------------------------------------------------------------------
 * Deriche Gaussian Coefficients Container.
 */

void DericheGaussianCoefficientsContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

DericheGaussianCoefficients &DericheGaussianCoefficientsContainer::get(Context &context,
                                                                       float sigma)
{
  const DericheGaussianCoefficientsKey key(sigma);

  auto &deriche_gaussian_coefficients = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<DericheGaussianCoefficients>(context, sigma); });

  deriche_gaussian_coefficients.needed = true;
  return deriche_gaussian_coefficients;
}

}  // namespace blender::realtime_compositor
