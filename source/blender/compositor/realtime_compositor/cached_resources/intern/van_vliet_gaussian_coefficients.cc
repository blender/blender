/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* -------------------------------------------------------------------------------------------------
 * Van Vliet Gaussian Coefficients.
 *
 * Computes the coefficients of the fourth order IIR filter approximating a Gaussian filter
 * computed using Van Vliet's design method. This is based on the following paper:
 *
 *   Van Vliet, Lucas J., Ian T. Young, and Piet W. Verbeek. "Recursive Gaussian derivative
 *   filters." Proceedings. Fourteenth International Conference on Pattern Recognition (Cat. No.
 *   98EX170). Vol. 1. IEEE, 1998.
 *
 *
 * The filter is computed as the cascade of a causal and a non causal sequences of second order
 * difference equations as can be seen in Equation (11) in Van Vliet's paper. The coefficients are
 * the same for both the causal and non causal sequences.
 *
 * However, to improve its numerical stability, we decompose the 4th order filter into a parallel
 * bank of second order filers using the methods of partial fractions as demonstrated in the follow
 * book:
 *
 *   Oppenheim, Alan V. Discrete-time signal processing. Pearson Education India, 1999.
 *
 */

#include <array>
#include <complex>
#include <cstdint>
#include <memory>

#include "BLI_assert.h"
#include "BLI_hash.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"

#include "COM_context.hh"
#include "COM_van_vliet_gaussian_coefficients.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Van Vliet Gaussian Coefficients Key.
 */

VanVlietGaussianCoefficientsKey::VanVlietGaussianCoefficientsKey(float sigma) : sigma(sigma) {}

uint64_t VanVlietGaussianCoefficientsKey::hash() const
{
  return get_default_hash(sigma);
}

bool operator==(const VanVlietGaussianCoefficientsKey &a, const VanVlietGaussianCoefficientsKey &b)
{
  return a.sigma == b.sigma;
}

/* -------------------------------------------------------------------------------------------------
 * Van Vliet Gaussian Coefficients.
 */

/* Computes the variance of the Gaussian filter represented by the given poles scaled by the given
 * scale factor. This is based on Equation (20) in Van Vliet's paper. */
static double compute_scaled_poles_variance(const std::array<std::complex<double>, 4> &poles,
                                            double scale_factor)
{
  std::complex<double> variance = std::complex<double>(0.0, 0.0);
  for (const std::complex<double> &pole : poles) {
    const double magnitude = std::pow(std::abs(pole), 1.0 / scale_factor);
    const double phase = std::arg(pole) / scale_factor;
    const std::complex<double> multiplier1 = std::polar(magnitude, phase);
    const std::complex<double> multiplier2 = std::pow(magnitude - std::polar(1.0, phase), -2.0);
    variance += 2.0 * multiplier1 * multiplier2;
  }

  /* The variance is actually real valued as guaranteed by Equations (10) and (2) since the poles
   * are complex conjugate pairs. See Section 3.3 of the paper. */
  return variance.real();
}

/* Computes the partial derivative with respect to the scale factor at the given scale factor of
 * the variance of the Gaussian filter represented by the given poles scaled by the given scale
 * factor. This is based on the partial derivative with respect to the scale factor of Equation
 * (20) in Van Vliet's paper.
 *
 * The derivative is not listed in the paper, but was computed manually as the sum of the following
 * for each of the poles:
 *
 *   \frac{
 *     2a^\frac{1}{x}e^\frac{ib}{x} (e^\frac{ib}{x}+a^\frac{1}{x}) (\ln(a)-ib)
 *   }{
 *     x^2 (a^\frac{1}{x}-e^\frac{ib}{x})^3
 *   }
 *
 * Where "x" is the scale factor, "a" is the magnitude of the pole, and "b" is its phase. */
static double compute_scaled_poles_variance_derivative(
    const std::array<std::complex<double>, 4> &poles, double scale_factor)
{
  std::complex<double> variance_derivative = std::complex<double>(0.0, 0.0);
  for (const std::complex<double> &pole : poles) {
    const double magnitude = std::pow(std::abs(pole), 1.0 / scale_factor);
    const double phase = std::arg(pole) / scale_factor;

    const std::complex<double> multiplier1 = std::polar(magnitude, phase);
    const std::complex<double> multiplier2 = magnitude + std::polar(1.0, phase);
    const std::complex<double> multiplier3 = std::log(std::abs(pole)) -
                                             std::complex<double>(0.0, std::arg(pole));

    const std::complex<double> divisor1 = std::pow(magnitude - std::polar(1.0, phase), 3.0);
    const std::complex<double> divisor2 = math::square(scale_factor);

    variance_derivative += 2.0 * multiplier1 * multiplier2 * multiplier3 / (divisor1 * divisor2);
  }

  /* The variance derivative is actually real valued as guaranteed by Equations (10) and (2) since
   * the poles are complex conjugate pairs. See Section 3.3 of the paper. */
  return variance_derivative.real();
}

/* The poles were computed for a Gaussian filter with a sigma value of 2, in order to generalize
 * that for any sigma value, we need to scale the poles by a certain scaling factor as described in
 * Section 4.2 of Van Vliet's paper. To find the scaling factor, we start from an initial guess of
 * half sigma, then iteratively improve the guess using Newton's method by computing the variance
 * and its derivative based on Equation (20). */
static double find_scale_factor(const std::array<std::complex<double>, 4> &poles,
                                float reference_sigma)
{
  const double reference_variance = math::square(reference_sigma);

  /* Note that the poles were computed for a Gaussian filter with a sigma value of 2, so it it
   * as if we have a base scale of 2, and we start with half sigma as an initial guess. See
   * Section 4.2 for more information */
  double scale_factor = reference_sigma / 2.0;

  const int maximum_interations = 10;
  for (int i = 0; i < maximum_interations; i++) {
    const double variance = compute_scaled_poles_variance(poles, scale_factor);

    /* Close enough, we have found our scale factor. */
    if (math::abs(reference_variance - variance) < 1.0e-8) {
      return scale_factor;
    }

    /* Improve guess using Newton's method. Notice that Newton's method is a root finding method,
     * so we supply the difference to the reference variance as our function, since the zero point
     * will be when the variance is equal to the reference one. The derivative is not affected
     * since the reference variance is a constant. */
    const double derivative = compute_scaled_poles_variance_derivative(poles, scale_factor);
    scale_factor -= (variance - reference_variance) / derivative;
  }

  /* The paper mentions that only a few iterations are needed, so if we didn't converge after
   * maximum_interations, something is probably wrong. */
  BLI_assert_unreachable();
  return scale_factor;
}

/* The poles were computed for a Gaussian filter with a sigma value of 2, so scale them using
 * Equation (19) in Van Vliet's paper to have the given sigma value. This involves finding the
 * appropriate scale factor based on Equation (20), see Section 4.2 and the find_scale_factor
 * method for more information. */
static std::array<std::complex<double>, 4> computed_scaled_poles(
    const std::array<std::complex<double>, 4> &poles, float sigma)
{
  const double scale_factor = find_scale_factor(poles, sigma);

  std::array<std::complex<double>, 4> scaled_poles;
  for (int i = 0; i < poles.size(); i++) {
    const std::complex<double> &pole = poles[i];
    const double magnitude = std::pow(std::abs(pole), 1.0 / scale_factor);
    const double phase = std::arg(pole) / scale_factor;
    scaled_poles[i] = std::polar(magnitude, phase);
  }

  return scaled_poles;
}

/* Compute the causal poles from the non causal ones. Since the Gaussian is a real even function,
 * the causal poles are just the inverse of the non causal poles, as noted in Equation (2) in Van
 * Vliet's paper. */
static std::array<std::complex<double>, 4> compute_causal_poles(
    const std::array<std::complex<double>, 4> &non_causal_poles)
{
  std::array<std::complex<double>, 4> causal_poles;
  for (int i = 0; i < non_causal_poles.size(); i++) {
    causal_poles[i] = 1.0 / non_causal_poles[i];
  }

  return causal_poles;
}

/* Computes the feedback coefficients from the given poles based on the equations in Equation (13)
 * in Van Vliet's paper. See Section 3.2 for more information. */
static double4 compute_feedback_coefficients(const std::array<std::complex<double>, 4> &poles)
{
  /* Compute the gain of the poles, which is the "b" at the end of Equation (13). */
  std::complex<double> gain = std::complex<double>(1.0, 0.0);
  for (const std::complex<double> &pole : poles) {
    gain /= pole;
  }

  /* Compute the coefficients b4, b3, b2, and b1 based on the expressions b_N, b_N-1, b_N-2, and
   * b_N-3 respectively in Equation (13). b4 and b3 are trivial, while b2 and b1 can be computed by
   * drawing the following summation trees, where each path from the root to the leaf is multiplied
   * and added:
   *
   *                  b2
   *             ____/|\____
   *            /     |     \
   *   i -->   2      3      4
   *           |     / \    /|\
   *   j -->   1    1   2  1 2 3
   *
   *                 b1
   *             ___/ \___
   *            /         \
   *   i -->   3           4
   *           |          / \
   *   j -->   2         2   3
   *           |         |  / \
   *   k -->   1         1 1   2
   *
   * Notice that the values of i, j, and k are 1-index, so we need to subtract one when accessing
   * the poles. */
  const std::complex<double> b4 = gain;
  const std::complex<double> b3 = -gain * (poles[0] + poles[1] + poles[2] + poles[3]);
  const std::complex<double> b2 = gain * (poles[1] * poles[0] + poles[2] * poles[0] +
                                          poles[2] * poles[1] + poles[3] * poles[0] +
                                          poles[3] * poles[1] + poles[3] * poles[2]);
  const std::complex<double> b1 = -gain * (poles[2] * poles[1] * poles[0] +
                                           poles[3] * poles[1] * poles[0] +
                                           poles[3] * poles[2] * poles[0] +
                                           poles[3] * poles[2] * poles[1]);

  /* The coefficients are actually real valued as guaranteed by Equations (10) and (2) since
   * the poles are complex conjugate pairs. See Section 3.3 of the paper. */
  const double4 coefficients = double4(b1.real(), b2.real(), b3.real(), b4.real());

  return coefficients;
}

/* Computes the feedforward coefficient from the feedback coefficients based on Equation (12) of
 * Van Vliet's paper. See Section 3.2 for more information. */
static double compute_feedforward_coefficient(const double4 &feedback_coefficients)
{
  return 1.0 + math::reduce_add(feedback_coefficients);
}

/* Computes the residue of the partial fraction of the transfer function of the given causal poles
 * and gain for the given target pole. This essentially evaluates Equation (3.41) in Oppenheim's
 * book, where d_k is the target pole and assuming the transfer function is in the form given in
 * Equation (3.39), where d_k are the poles. See the following derivation for the gain value.
 *
 * For the particular case of the Van Vliet's system, there are no zeros, so the numerator in
 * Equation (3.39) is one. Further note that Van Vliet's formulation is different from the expected
 * form, so we need to rearrange Equation (3) in to match the form in Equation (3.39), which is
 * shown below.
 *
 * Start from the causal term of Equation (3):
 *
 *   H_+(z) = \prod_{i=1}^N \frac{d_i - 1}{d_i - z^{-1}}
 *
 * Divide by d_i:
 *
 *   H_+(z) = \prod_{i=1}^N \frac{1 - d_i^{-1}}{1 - d_i^{-1}z^{-1}}
 *
 * Move the numerator to its own product:
 *
 *   H_+(z) = \prod_{i=1}^N 1 - d_i^{-1} \prod_{i=1}^N \frac{1}{1 - d_i^{-1}z^{-1}}
 *
 * And we reach the same form as Equation (3.39). Where the first product term is b0 / a0 and is
 * also the given gain value, which is also the same as the feedforward coefficient denoted by
 * the alpha in Equation (12). Further d_i^{-1} in our derivation is the same as d_k in Equation
 * (3.39), the discrepancy in the inverse operator is the fact that Van Vliet's derivation assume
 * non causal poles, while Oppenheim's assume causal poles, which are inverse of each other as can
 * be seen in the compute_causal_poles function. */
static std::complex<double> compute_partial_fraction_residue(
    const std::array<std::complex<double>, 4> &poles,
    const std::complex<double> &target_pole,
    double gain)
{
  /* Evaluating Equation (3.41) actually corresponds to omitting the terms in Equation (3.39) that
   * corresponds to the target pole or its conjugate, because they get canceled by the first term
   * in Equation (3.41). That's we are essentially evaluating the limit as the expression tends to
   * the target pole. */
  std::complex<double> target_pole_inverse = 1.0 / target_pole;
  std::complex<double> residue = std::complex<double>(1.0, 0.0);
  for (const std::complex<double> &pole : poles) {
    if (pole != target_pole && pole != std::conj(target_pole)) {
      residue *= 1.0 - pole * target_pole_inverse;
    }
  }

  /* Remember that the gain is the b0 / a0 expression in Equation (3.39). */
  return gain / residue;
}

/* Evaluates the causal transfer function at the reciprocal of the given pole, which will be the
 * non causal pole if the given pole is a causal one, as discussed in the compute_causal_poles
 * function. The causal transfer function is given in Equation (3) in Van Vliet's paper, but we
 * compute it in the form derived in the description of the compute_partial_fraction_residue
 * function, also see the aforementioned function for the gain value. */
static std::complex<double> compute_causal_transfer_function_at_non_causal_pole(
    const std::array<std::complex<double>, 4> &poles,
    const std::complex<double> &target_pole,
    double gain)
{
  std::complex<double> result = std::complex<double>(1.0, 0.0);
  for (const std::complex<double> &pole : poles) {
    result *= 1.0 - pole * target_pole;
  }

  return gain / result;
}

/* Combine each pole and its conjugate counterpart into a second order section and assign its
 * coefficients to the given coefficients value. The residue of the pole and its transfer value in
 * the partial fraction of its transfer function are given.
 *
 * TODO: Properly document this function and prove its equations. */
static void compute_second_order_section(const std::complex<double> &pole,
                                         const std::complex<double> &residue,
                                         const std::complex<double> &transfer_value,
                                         double2 &r_feedback_coefficients,
                                         double2 &r_causal_feedforward_coefficients,
                                         double2 &r_non_causal_feedforward_coefficients)
{
  const std::complex<double> parallel_residue = residue * transfer_value;
  const std::complex<double> pole_inverse = 1.0 / pole;

  r_feedback_coefficients = double2(-2.0 * pole.real(), std::norm(pole));

  const double causal_feedforward_1 = parallel_residue.imag() / pole_inverse.imag();
  const double causal_feedforward_0 = parallel_residue.real() -
                                      causal_feedforward_1 * pole_inverse.real();
  r_causal_feedforward_coefficients = double2(causal_feedforward_0, causal_feedforward_1);

  const double non_causal_feedforward_1 = causal_feedforward_1 -
                                          causal_feedforward_0 * r_feedback_coefficients[0];
  const double non_causal_feedforward_2 = -causal_feedforward_0 * r_feedback_coefficients[1];
  r_non_causal_feedforward_coefficients = double2(non_causal_feedforward_1,
                                                  non_causal_feedforward_2);
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
static double compute_boundary_coefficient(const double2 &feedback_coefficients,
                                           const double2 &feedforward_coefficients)
{
  return math::reduce_add(feedforward_coefficients) /
         (1.0 + math::reduce_add(feedback_coefficients));
}

/* Computes the feedback and feedforward coefficients for the 4th order Van Vliet Gaussian filter
 * given a target Gaussian sigma value. We first scale the poles of the filter to match the sigma
 * value based on the method described in Section 4.2 of Van Vliet's paper, then we compute the
 * coefficients from the scaled poles based on Equations (12) and (13). */
VanVlietGaussianCoefficients::VanVlietGaussianCoefficients(Context & /*context*/, float sigma)
{

  /* The 4th order (N=4) poles for the Gaussian filter of a sigma of 2 computed by minimizing the
   * maximum error (L-infinity) to true Gaussian as provided in Van Vliet's paper Table (1) fourth
   * column. Notice that the second and fourth poles are the complex conjugates of the first and
   * third poles respectively as noted in the table description. */
  const std::array<std::complex<double>, 4> poles = {
      std::complex<double>(1.12075, 1.27788),
      std::complex<double>(1.12075, -1.27788),
      std::complex<double>(1.76952, 0.46611),
      std::complex<double>(1.76952, -0.46611),
  };

  const std::array<std::complex<double>, 4> scaled_poles = computed_scaled_poles(poles, sigma);

  /* The given poles are actually the non causal poles, since they are outside of the unit circle,
   * as demonstrated in Section 3.4 of Van Vliet's paper. And we compute the causal poles from
   * those. */
  const std::array<std::complex<double>, 4> non_causal_poles = scaled_poles;
  const std::array<std::complex<double>, 4> causal_poles = compute_causal_poles(non_causal_poles);

  /* Compute the feedforward and feedback coefficients, noting that those are functions of the non
   * causal poles. */
  const double4 feedback_coefficients = compute_feedback_coefficients(non_causal_poles);
  const double feedforward_coefficient = compute_feedforward_coefficient(feedback_coefficients);

  /* We only compute the residue for two of the causal poles, since the other two are complex
   * conjugates of those two, and their residues will also be the complex conjugate of their
   * respective counterpart. The gain is the feedforward coefficient as discussed in the function
   * description. */
  const std::complex<double> first_residue = compute_partial_fraction_residue(
      causal_poles, causal_poles[0], feedforward_coefficient);
  const std::complex<double> second_residue = compute_partial_fraction_residue(
      causal_poles, causal_poles[2], feedforward_coefficient);

  /* We only compute the transfer value of for two of the non causal poles, since the other two are
   * complex conjugates of those two, and their transfer values will also be the complex conjugate
   * of their respective counterpart. The gain is the feedforward coefficient as discussed in the
   * function description. */
  const std::complex<double> first_transfer_value =
      compute_causal_transfer_function_at_non_causal_pole(
          causal_poles, causal_poles[0], feedforward_coefficient);
  const std::complex<double> second_transfer_value =
      compute_causal_transfer_function_at_non_causal_pole(
          causal_poles, causal_poles[2], feedforward_coefficient);

  /* Combine each pole and its conjugate counterpart into a second order section and assign its
   * coefficients. */
  compute_second_order_section(causal_poles[0],
                               first_residue,
                               first_transfer_value,
                               first_feedback_coefficients_,
                               first_causal_feedforward_coefficients_,
                               first_non_causal_feedforward_coefficients_);
  compute_second_order_section(causal_poles[2],
                               second_residue,
                               second_transfer_value,
                               second_feedback_coefficients_,
                               second_causal_feedforward_coefficients_,
                               second_non_causal_feedforward_coefficients_);

  /* Compute the boundary coefficients for all four of second order sections. */
  first_causal_boundary_coefficient_ = compute_boundary_coefficient(
      first_feedback_coefficients_, first_causal_feedforward_coefficients_);
  first_non_causal_boundary_coefficient_ = compute_boundary_coefficient(
      first_feedback_coefficients_, first_non_causal_feedforward_coefficients_);
  second_causal_boundary_coefficient_ = compute_boundary_coefficient(
      second_feedback_coefficients_, second_causal_feedforward_coefficients_);
  second_non_causal_boundary_coefficient_ = compute_boundary_coefficient(
      second_feedback_coefficients_, second_non_causal_feedforward_coefficients_);
}

const double2 &VanVlietGaussianCoefficients::first_causal_feedforward_coefficients() const
{
  return first_causal_feedforward_coefficients_;
}

const double2 &VanVlietGaussianCoefficients::first_non_causal_feedforward_coefficients() const
{
  return first_non_causal_feedforward_coefficients_;
}

const double2 &VanVlietGaussianCoefficients::first_feedback_coefficients() const
{
  return first_feedback_coefficients_;
}

const double2 &VanVlietGaussianCoefficients::second_causal_feedforward_coefficients() const
{
  return second_causal_feedforward_coefficients_;
}

const double2 &VanVlietGaussianCoefficients::second_non_causal_feedforward_coefficients() const
{
  return second_non_causal_feedforward_coefficients_;
}

const double2 &VanVlietGaussianCoefficients::second_feedback_coefficients() const
{
  return second_feedback_coefficients_;
}

double VanVlietGaussianCoefficients::first_causal_boundary_coefficient() const
{
  return first_causal_boundary_coefficient_;
}

double VanVlietGaussianCoefficients::first_non_causal_boundary_coefficient() const
{
  return first_non_causal_boundary_coefficient_;
}

double VanVlietGaussianCoefficients::second_causal_boundary_coefficient() const
{
  return second_causal_boundary_coefficient_;
}

double VanVlietGaussianCoefficients::second_non_causal_boundary_coefficient() const
{
  return second_non_causal_boundary_coefficient_;
}

/* --------------------------------------------------------------------
 * Van Vliet Gaussian Coefficients Container.
 */

void VanVlietGaussianCoefficientsContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

VanVlietGaussianCoefficients &VanVlietGaussianCoefficientsContainer::get(Context &context,
                                                                         float sigma)
{
  const VanVlietGaussianCoefficientsKey key(sigma);

  auto &deriche_gaussian_coefficients = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<VanVlietGaussianCoefficients>(context, sigma); });

  deriche_gaussian_coefficients.needed = true;
  return deriche_gaussian_coefficients;
}

}  // namespace blender::realtime_compositor
