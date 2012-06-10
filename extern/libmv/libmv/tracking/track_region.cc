// Copyright (c) 2012 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Author: mierle@google.com (Keir Mierle)
//
// TODO(keir): While this tracking code works rather well, it has some
// outragous inefficiencies. There is probably a 5-10x speedup to be had if a
// smart coder went through the TODO's and made the suggested performance
// enhancements.

// Necessary for M_E when building with MSVC.
#define _USE_MATH_DEFINES

#include "libmv/tracking/track_region.h"

#include <Eigen/SVD>
#include <Eigen/QR>
#include <iostream>
#include "ceres/ceres.h"
#include "libmv/logging/logging.h"
#include "libmv/image/image.h"
#include "libmv/image/sample.h"
#include "libmv/image/convolve.h"
#include "libmv/multiview/homography.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

using ceres::Jet;
using ceres::JetOps;
using ceres::Chain;

TrackRegionOptions::TrackRegionOptions()
    : mode(TRANSLATION),
      minimum_correlation(0),
      max_iterations(20),
      use_esm(true),
      use_brute_initialization(true),
      use_normalized_intensities(false),
      sigma(0.9),
      num_extra_points(0),
      regularization_coefficient(0.0),
      image1_mask(NULL) {
}

namespace {

// TODO(keir): Consider adding padding.
template<typename T>
bool InBounds(const FloatImage &image,
              const T &x,
              const T &y) {
  return 0.0 <= x && x < image.Width() &&
         0.0 <= y && y < image.Height();
}

bool AllInBounds(const FloatImage &image,
                 const double *x,
                 const double *y) {
  for (int i = 0; i < 4; ++i) {
    if (!InBounds(image, x[i], y[i])) {
      return false;
    }
  }
  return true;
}

// Sample the image at position (x, y) but use the gradient, if present, to
// propagate derivatives from x and y. This is needed to integrate the numeric
// image gradients with Ceres's autodiff framework.
template<typename T>
static T SampleWithDerivative(const FloatImage &image_and_gradient,
                              const T &x,
                              const T &y) {
  float scalar_x = JetOps<T>::GetScalar(x);
  float scalar_y = JetOps<T>::GetScalar(y);

  // Note that sample[1] and sample[2] will be uninitialized in the scalar
  // case, but that is not an issue because the Chain::Rule below will not read
  // the uninitialized values.
  float sample[3];
  if (JetOps<T>::IsScalar()) {
    // For the scalar case, only sample the image.
    sample[0] = SampleLinear(image_and_gradient, scalar_y, scalar_x, 0);
  } else {
    // For the derivative case, sample the gradient as well.
    SampleLinear(image_and_gradient, scalar_y, scalar_x, sample);
  }
  T xy[2] = { x, y };
  return Chain<float, 2, T>::Rule(sample[0], sample + 1, xy);
}

template<typename Warp>
class BoundaryCheckingCallback : public ceres::IterationCallback {
 public:
  BoundaryCheckingCallback(const FloatImage& image2,
                           const Warp &warp,
                           const double *x1, const double *y1)
      : image2_(image2), warp_(warp), x1_(x1), y1_(y1) {}

  virtual ceres::CallbackReturnType operator()(
      const ceres::IterationSummary& summary) {
    // Warp the original 4 points with the current warp into image2.
    double x2[4];
    double y2[4];
    for (int i = 0; i < 4; ++i) {
      warp_.Forward(warp_.parameters, x1_[i], y1_[i], x2 + i, y2 + i);
    }
    // Enusre they are all in bounds.
    if (!AllInBounds(image2_, x2, y2)) {
      return ceres::SOLVER_ABORT;
    }
    return ceres::SOLVER_CONTINUE;
  }

 private:
  const FloatImage &image2_;
  const Warp &warp_;
  const double *x1_;
  const double *y1_;
};

template<typename Warp>
class PixelDifferenceCostFunctor {
 public:
  PixelDifferenceCostFunctor(const TrackRegionOptions &options,
                  const FloatImage &image_and_gradient1,
                  const FloatImage &image_and_gradient2,
                  const Mat3 &canonical_to_image1,
                  int num_samples_x,
                  int num_samples_y,
                  const Warp &warp)
      : options_(options),
        image_and_gradient1_(image_and_gradient1),       
        image_and_gradient2_(image_and_gradient2),       
        canonical_to_image1_(canonical_to_image1),
        num_samples_x_(num_samples_x),
        num_samples_y_(num_samples_y),
        warp_(warp),
        pattern_and_gradient_(num_samples_y_, num_samples_x_, 3),
        pattern_positions_(num_samples_y_, num_samples_x_, 2),
        pattern_mask_(num_samples_y_, num_samples_x_, 1) {
    ComputeCanonicalPatchAndNormalizer();
  }

  void ComputeCanonicalPatchAndNormalizer() {
    src_mean_ = 0.0;
    double num_samples = 0.0;
    for (int r = 0; r < num_samples_y_; ++r) {
      for (int c = 0; c < num_samples_x_; ++c) {
        // Compute the position; cache it.
        Vec3 image_position = canonical_to_image1_ * Vec3(c, r, 1);
        image_position /= image_position(2);
        pattern_positions_(r, c, 0) = image_position(0);
        pattern_positions_(r, c, 1) = image_position(1);

        // Sample the pattern and gradients.
        SampleLinear(image_and_gradient1_,
                     image_position(1),  // SampleLinear is r, c.
                     image_position(0),
                     &pattern_and_gradient_(r, c, 0));

        // Sample sample the mask.
        double mask_value = 1.0;
        if (options_.image1_mask != NULL) {
          SampleLinear(*options_.image1_mask,
                       image_position(1),  // SampleLinear is r, c.
                       image_position(0),
                       &pattern_mask_(r, c, 0));
          mask_value = pattern_mask_(r, c);
        }
        src_mean_ += pattern_and_gradient_(r, c, 0) * mask_value;
        num_samples += mask_value;
      }
    }
    src_mean_ /= num_samples;
  }

  template<typename T>
  bool operator()(const T *warp_parameters, T *residuals) const {
    if (options_.image1_mask != NULL) {
      VLOG(2) << "Using a mask.";
    }
    for (int i = 0; i < Warp::NUM_PARAMETERS; ++i) {
      VLOG(2) << "warp_parameters[" << i << "]: " << warp_parameters[i];
    }

    T dst_mean = T(1.0);
    if (options_.use_normalized_intensities) {
      ComputeNormalizingCoefficient(warp_parameters,
                                    &dst_mean);
    }

    int cursor = 0;
    for (int r = 0; r < num_samples_y_; ++r) {
      for (int c = 0; c < num_samples_x_; ++c) {
        // Use the pre-computed image1 position.
        Vec2 image1_position(pattern_positions_(r, c, 0),
                             pattern_positions_(r, c, 1));

        // Sample the mask early; if it's zero, this pixel has no effect. This
        // allows early bailout from the expensive sampling that happens below.
        //
        // Note that partial masks are not short circuited. To see why short
        // circuiting produces bitwise-exact same results, consider that the
        // residual for each pixel is 
        //
        //    residual = mask * (src - dst)  ,
        //
        // and for jets, multiplying by a scalar multiplies the derivative
        // components by the scalar as well. Therefore, if the mask is exactly
        // zero, then so too will the final residual and derivatives.
        double mask_value = 1.0;
        if (options_.image1_mask != NULL) {
          mask_value = pattern_mask_(r, c);
          if (mask_value == 0.0) {
            residuals[cursor++] = T(0.0);
            continue;
          }
        }

        // Compute the location of the destination pixel.
        T image2_position[2];
        warp_.Forward(warp_parameters,
                      T(image1_position[0]),
                      T(image1_position[1]),
                      &image2_position[0],
                      &image2_position[1]);

        // Sample the destination, propagating derivatives.
        T dst_sample = SampleWithDerivative(image_and_gradient2_,
                                            image2_position[0],
                                            image2_position[1]);

        // Sample the source. This is made complicated by ESM mode.
        T src_sample;
        if (options_.use_esm && !JetOps<T>::IsScalar()) {
          // In ESM mode, the derivative of the source is also taken into
          // account. This changes the linearization in a way that causes
          // better convergence. Copy the derivative of the warp parameters
          // onto the jets for the image1 position. This is the ESM hack.
          T image1_position_jet[2] = {
            image2_position[0],  // Order is x, y. This matches the
            image2_position[1]   // derivative order in the patch.
          };
          JetOps<T>::SetScalar(image1_position[0], image1_position_jet + 0);
          JetOps<T>::SetScalar(image1_position[1], image1_position_jet + 1);

          // Now that the image1 positions have the jets applied from the
          // image2 position (the ESM hack), chain the image gradients to
          // obtain a sample with the derivative with respect to the warp
          // parameters attached.
          src_sample = Chain<float, 2, T>::Rule(pattern_and_gradient_(r, c),
                                                &pattern_and_gradient_(r, c, 1),
                                                image1_position_jet);

          // The jacobians for these should be averaged. Due to the subtraction
          // below, flip the sign of the src derivative so that the effect
          // after subtraction of the jets is that they are averaged.
          JetOps<T>::ScaleDerivative(-0.5, &src_sample);
          JetOps<T>::ScaleDerivative(0.5, &dst_sample);
        } else {
          // This is the traditional, forward-mode KLT solution.
          src_sample = T(pattern_and_gradient_(r, c));
        }

        // Normalize the samples by the mean values of each signal. The typical
        // light model assumes multiplicative intensity changes with changing
        // light, so this is a reasonable choice. Note that dst_mean has
        // derivative information attached thanks to autodiff.
        if (options_.use_normalized_intensities) {
          src_sample /= T(src_mean_);
          dst_sample /= dst_mean;
        }

        // The difference is the error.
        T error = src_sample - dst_sample;

        // Weight the error by the mask, if one is present.
        if (options_.image1_mask != NULL) {
          error *= T(mask_value);
        }
        residuals[cursor++] = error;
      }
    }
    return true;
  }

  // For normalized matching, the average and 
  template<typename T>
  void ComputeNormalizingCoefficient(const T *warp_parameters,
                                     T *dst_mean) const {

    *dst_mean = T(0.0);
    double num_samples = 0.0;
    for (int r = 0; r < num_samples_y_; ++r) {
      for (int c = 0; c < num_samples_x_; ++c) {
        // Use the pre-computed image1 position.
        Vec2 image1_position(pattern_positions_(r, c, 0),
                             pattern_positions_(r, c, 1));
        
        // Sample the mask early; if it's zero, this pixel has no effect. This
        // allows early bailout from the expensive sampling that happens below.
        double mask_value = 1.0;
        if (options_.image1_mask != NULL) {
          mask_value = pattern_mask_(r, c);
          if (mask_value == 0.0) {
            continue;
          }
        }

        // Compute the location of the destination pixel.
        T image2_position[2];
        warp_.Forward(warp_parameters,
                      T(image1_position[0]),
                      T(image1_position[1]),
                      &image2_position[0],
                      &image2_position[1]);


        // Sample the destination, propagating derivatives.
        // TODO(keir): This accumulation can, surprisingly, be done as a
        // pre-pass by using integral images. This is complicated by the need
        // to store the jets in the integral image, but it is possible.
        T dst_sample = SampleWithDerivative(image_and_gradient2_,
                                            image2_position[0],
                                            image2_position[1]);

        // Weight the sample by the mask, if one is present.
        if (options_.image1_mask != NULL) {
          dst_sample *= T(mask_value);
        }

        *dst_mean += dst_sample;
        num_samples += mask_value;
      }
    }
    *dst_mean /= T(num_samples);
    LG << "Normalization for dst:" << *dst_mean;
  }

 // TODO(keir): Consider also computing the cost here.
 double PearsonProductMomentCorrelationCoefficient(
     const double *warp_parameters) const {
   for (int i = 0; i < Warp::NUM_PARAMETERS; ++i) {
     VLOG(2) << "Correlation warp_parameters[" << i << "]: "
             << warp_parameters[i];
   }

   // The single-pass PMCC computation is somewhat numerically unstable, but
   // it's sufficient for the tracker.
   double sX = 0, sY = 0, sXX = 0, sYY = 0, sXY = 0;

   // Due to masking, it's important to account for fractional samples.
   // For example, samples with a 50% mask are counted as a half sample.
   double num_samples = 0;

   for (int r = 0; r < num_samples_y_; ++r) {
     for (int c = 0; c < num_samples_x_; ++c) {
        // Use the pre-computed image1 position.
        Vec2 image1_position(pattern_positions_(r, c, 0),
                             pattern_positions_(r, c, 1));
        
        double mask_value = 1.0;
        if (options_.image1_mask != NULL) {
          mask_value = pattern_mask_(r, c);
          if (mask_value == 0.0) {
            continue;
          }
        }

        // Compute the location of the destination pixel.
        double image2_position[2];
        warp_.Forward(warp_parameters,
                      image1_position[0],
                      image1_position[1],
                      &image2_position[0],
                      &image2_position[1]);

        double x = pattern_and_gradient_(r, c);
        double y = SampleLinear(image_and_gradient2_,
                                image2_position[1],  // SampleLinear is r, c.
                                image2_position[0]);

        // Weight the signals by the mask, if one is present.
        if (options_.image1_mask != NULL) {
          x *= mask_value;
          y *= mask_value;
          num_samples += mask_value;
        } else {
          num_samples++;
        }
        sX += x;
        sY += y;
        sXX += x*x;
        sYY += y*y;
        sXY += x*y;
      }
    }
    // Normalize.
    sX /= num_samples;
    sY /= num_samples;
    sXX /= num_samples;
    sYY /= num_samples;
    sXY /= num_samples;

    double var_x = sXX - sX*sX;
    double var_y = sYY - sY*sY;
    double covariance_xy = sXY - sX*sY;

    double correlation = covariance_xy / sqrt(var_x * var_y);
    LG << "Covariance xy: " << covariance_xy
       << ", var 1: " << var_x << ", var 2: " << var_y
       << ", correlation: " << correlation;
    return correlation;
  }

 private:
  const TrackRegionOptions &options_;
  const FloatImage &image_and_gradient1_;
  const FloatImage &image_and_gradient2_;
  const Mat3 &canonical_to_image1_;
  int num_samples_x_;
  int num_samples_y_;
  const Warp &warp_;
  double src_mean_;
  FloatImage pattern_and_gradient_;

  // This contains the position from where the cached pattern samples were
  // taken from. This is also used to warp from src to dest without going from
  // canonical pixels to src first.
  FloatImage pattern_positions_;

  FloatImage pattern_mask_;
};

template<typename Warp>
class WarpRegularizingCostFunctor {
 public:
  WarpRegularizingCostFunctor(const TrackRegionOptions &options,
                              const double *x1,
                              const double *y1,
                              const double *x2_original,
                              const double *y2_original,
                              const Warp &warp)
      : options_(options),
        x1_(x1),
        y1_(y1),
        x2_original_(x2_original),
        y2_original_(y2_original),
        warp_(warp) {
    // Compute the centroid of the first guess quad.
    // TODO(keir): Use Quad class here.
    original_centroid_[0] = 0.0;
    original_centroid_[1] = 0.0;
    for (int i = 0; i < 4; ++i) {
      original_centroid_[0] += x2_original[i];
      original_centroid_[1] += y2_original[i];
    }
    original_centroid_[0] /= 4;
    original_centroid_[1] /= 4;
  }

  template<typename T>
  bool operator()(const T *warp_parameters, T *residuals) const {
    T dst_centroid[2] = { T(0.0), T(0.0) };
    for (int i = 0; i < 4; ++i) {
      T image1_position[2] = { T(x1_[i]), T(y1_[i]) };
      T image2_position[2];
      warp_.Forward(warp_parameters,
                    T(x1_[i]),
                    T(y1_[i]),
                    &image2_position[0],
                    &image2_position[1]);

      // Subtract the positions. Note that this ignores the centroids.
      residuals[2 * i + 0] = image2_position[0] - image1_position[0];
      residuals[2 * i + 1] = image2_position[1] - image1_position[1];

      // Accumulate the dst centroid.
      dst_centroid[0] += image2_position[0];
      dst_centroid[1] += image2_position[1];
    }
    dst_centroid[0] /= T(4.0);
    dst_centroid[1] /= T(4.0);

    // Adjust for the centroids.
    for (int i = 0; i < 4; ++i) {
      residuals[2 * i + 0] += T(original_centroid_[0]) - dst_centroid[0];
      residuals[2 * i + 1] += T(original_centroid_[1]) - dst_centroid[1];
    }

    // Reweight the residuals.
    for (int i = 0; i < 8; ++i) {
      residuals[i] *= T(options_.regularization_coefficient);
    }

    return true;
  }

  const TrackRegionOptions &options_;
  const double *x1_;
  const double *y1_;
  const double *x2_original_;
  const double *y2_original_;
  double original_centroid_[2];
  const Warp &warp_;
};

// Compute the warp from rectangular coordinates, where one corner is the
// origin, and the opposite corner is at (num_samples_x, num_samples_y).
Mat3 ComputeCanonicalHomography(const double *x1,
                                const double *y1,
                                int num_samples_x,
                                int num_samples_y) {
  Mat canonical(2, 4);
  canonical << 0, num_samples_x, num_samples_x, 0,
               0, 0,             num_samples_y, num_samples_y;

  Mat xy1(2, 4);
  xy1 << x1[0], x1[1], x1[2], x1[3],
         y1[0], y1[1], y1[2], y1[3];

  Mat3 H;
  if (!Homography2DFromCorrespondencesLinear(canonical, xy1, &H, 1e-12)) {
    LG << "Couldn't construct homography.";
  }
  return H;
}

class Quad {
 public:
  Quad(const double *x, const double *y) : x_(x), y_(y) {
    // Compute the centroid and store it.
    centroid_ = Vec2(0.0, 0.0);
    for (int i = 0; i < 4; ++i) {
      centroid_ += Vec2(x_[i], y_[i]);
    }
    centroid_ /= 4.0;
  }

  // The centroid of the four points representing the quad.
  const Vec2& Centroid() const {
    return centroid_;
  }

  // The average magnitude of the four points relative to the centroid.
  double Scale() const {
    double scale = 0.0;
    for (int i = 0; i < 4; ++i) {
      scale += (Vec2(x_[i], y_[i]) - Centroid()).norm();
    }
    return scale / 4.0;
  }

  Vec2 CornerRelativeToCentroid(int i) const {
    return Vec2(x_[i], y_[i]) - centroid_;
  }

 private:
  const double *x_;
  const double *y_;
  Vec2 centroid_;
};

struct TranslationWarp {
  TranslationWarp(const double *x1, const double *y1,
                  const double *x2, const double *y2) {
    Vec2 t = Quad(x2, y2).Centroid() - Quad(x1, y1).Centroid();
    parameters[0] = t[0];
    parameters[1] = t[1];
  }

  template<typename T>
  void Forward(const T *warp_parameters,
               const T &x1, const T& y1, T *x2, T* y2) const {
    *x2 = x1 + warp_parameters[0];
    *y2 = y1 + warp_parameters[1];
  }

  // Translation x, translation y.
  enum { NUM_PARAMETERS = 2 };
  double parameters[NUM_PARAMETERS];
};

struct TranslationScaleWarp {
  TranslationScaleWarp(const double *x1, const double *y1,
                       const double *x2, const double *y2)
      : q1(x1, y1) {
    Quad q2(x2, y2);

    // The difference in centroids is the best guess for translation.
    Vec2 t = q2.Centroid() - q1.Centroid();
    parameters[0] = t[0];
    parameters[1] = t[1];

    // The difference in scales is the estimate for the scale.
    parameters[2] = 1.0 - q2.Scale() / q1.Scale();
  }

  // The strange way of parameterizing the translation and scaling is to make
  // the knobs that the optimizer sees easy to adjust. This is less important
  // for the scaling case than the rotation case.
  template<typename T>
  void Forward(const T *warp_parameters,
               const T &x1, const T& y1, T *x2, T* y2) const {
    // Make the centroid of Q1 the origin.
    const T x1_origin = x1 - q1.Centroid()(0);
    const T y1_origin = y1 - q1.Centroid()(1);

    // Scale uniformly about the origin.
    const T scale = 1.0 + warp_parameters[2];
    const T x1_origin_scaled = scale * x1_origin;
    const T y1_origin_scaled = scale * y1_origin;

    // Translate back into the space of Q1 (but scaled).
    const T x1_scaled = x1_origin_scaled + q1.Centroid()(0);
    const T y1_scaled = y1_origin_scaled + q1.Centroid()(1);

    // Translate into the space of Q2.
    *x2 = x1_scaled + warp_parameters[0];
    *y2 = y1_scaled + warp_parameters[1];
  }

  // Translation x, translation y, scale.
  enum { NUM_PARAMETERS = 3 };
  double parameters[NUM_PARAMETERS];

  Quad q1;
};

// Assumes the given points are already zero-centroid and the same size.
Mat2 OrthogonalProcrustes(const Mat2 &correlation_matrix) {
  Eigen::JacobiSVD<Mat2> svd(correlation_matrix,
                             Eigen::ComputeFullU | Eigen::ComputeFullV);
  return svd.matrixV() * svd.matrixU().transpose();
}

struct TranslationRotationWarp {
  TranslationRotationWarp(const double *x1, const double *y1,
                          const double *x2, const double *y2)
      : q1(x1, y1) {
    Quad q2(x2, y2);

    // The difference in centroids is the best guess for translation.
    Vec2 t = q2.Centroid() - q1.Centroid();
    parameters[0] = t[0];
    parameters[1] = t[1];

    // Obtain the rotation via orthorgonal procrustes.
    Mat2 correlation_matrix;
    for (int i = 0; i < 4; ++i) {
      correlation_matrix += q1.CornerRelativeToCentroid(i) * 
                            q2.CornerRelativeToCentroid(i).transpose();
    }
    Mat2 R = OrthogonalProcrustes(correlation_matrix);
    parameters[2] = atan2(R(1, 0), R(0, 0));

    LG << "Correlation_matrix:\n" << correlation_matrix;
    LG << "R:\n" << R;
    LG << "Theta:" << parameters[2];
  }

  // The strange way of parameterizing the translation and rotation is to make
  // the knobs that the optimizer sees easy to adjust. The reason is that while
  // it is always the case that it is possible to express composed rotations
  // and translations as a single translation and rotation, the numerical
  // values needed for the composition are often large in magnitude. This is
  // enough to throw off any minimizer, since it must do the equivalent of
  // compose rotations and translations.
  //
  // Instead, use the parameterization below that offers a parameterization
  // that exposes the degrees of freedom in a way amenable to optimization.
  template<typename T>
  void Forward(const T *warp_parameters,
                      const T &x1, const T& y1, T *x2, T* y2) const {
    // Make the centroid of Q1 the origin.
    const T x1_origin = x1 - q1.Centroid()(0);
    const T y1_origin = y1 - q1.Centroid()(1);

    // Rotate about the origin (i.e. centroid of Q1).
    const T theta = warp_parameters[2];
    const T costheta = cos(theta);
    const T sintheta = sin(theta);
    const T x1_origin_rotated = costheta * x1_origin - sintheta * y1_origin;
    const T y1_origin_rotated = sintheta * x1_origin + costheta * y1_origin;

    // Translate back into the space of Q1 (but scaled).
    const T x1_rotated = x1_origin_rotated + q1.Centroid()(0);
    const T y1_rotated = y1_origin_rotated + q1.Centroid()(1);

    // Translate into the space of Q2.
    *x2 = x1_rotated + warp_parameters[0];
    *y2 = y1_rotated + warp_parameters[1];
  }

  // Translation x, translation y, rotation about the center of Q1 degrees.
  enum { NUM_PARAMETERS = 3 };
  double parameters[NUM_PARAMETERS];

  Quad q1;
};

struct TranslationRotationScaleWarp {
  TranslationRotationScaleWarp(const double *x1, const double *y1,
                               const double *x2, const double *y2)
      : q1(x1, y1) {
    Quad q2(x2, y2);

    // The difference in centroids is the best guess for translation.
    Vec2 t = q2.Centroid() - q1.Centroid();
    parameters[0] = t[0];
    parameters[1] = t[1];

    // The difference in scales is the estimate for the scale.
    parameters[2] = 1.0 - q2.Scale() / q1.Scale();

    // Obtain the rotation via orthorgonal procrustes.
    Mat2 correlation_matrix;
    for (int i = 0; i < 4; ++i) {
      correlation_matrix += q1.CornerRelativeToCentroid(i) * 
                            q2.CornerRelativeToCentroid(i).transpose();
    }
    Mat2 R = OrthogonalProcrustes(correlation_matrix);
    parameters[3] = atan2(R(1, 0), R(0, 0));

    LG << "Correlation_matrix:\n" << correlation_matrix;
    LG << "R:\n" << R;
    LG << "Theta:" << parameters[3];
  }

  // The strange way of parameterizing the translation and rotation is to make
  // the knobs that the optimizer sees easy to adjust. The reason is that while
  // it is always the case that it is possible to express composed rotations
  // and translations as a single translation and rotation, the numerical
  // values needed for the composition are often large in magnitude. This is
  // enough to throw off any minimizer, since it must do the equivalent of
  // compose rotations and translations.
  //
  // Instead, use the parameterization below that offers a parameterization
  // that exposes the degrees of freedom in a way amenable to optimization.
  template<typename T>
  void Forward(const T *warp_parameters,
                      const T &x1, const T& y1, T *x2, T* y2) const {
    // Make the centroid of Q1 the origin.
    const T x1_origin = x1 - q1.Centroid()(0);
    const T y1_origin = y1 - q1.Centroid()(1);

    // Rotate about the origin (i.e. centroid of Q1).
    const T theta = warp_parameters[3];
    const T costheta = cos(theta);
    const T sintheta = sin(theta);
    const T x1_origin_rotated = costheta * x1_origin - sintheta * y1_origin;
    const T y1_origin_rotated = sintheta * x1_origin + costheta * y1_origin;

    // Scale uniformly about the origin.
    const T scale = 1.0 + warp_parameters[2];
    const T x1_origin_rotated_scaled = scale * x1_origin_rotated;
    const T y1_origin_rotated_scaled = scale * y1_origin_rotated;

    // Translate back into the space of Q1 (but scaled and rotated).
    const T x1_rotated_scaled = x1_origin_rotated_scaled + q1.Centroid()(0);
    const T y1_rotated_scaled = y1_origin_rotated_scaled + q1.Centroid()(1);

    // Translate into the space of Q2.
    *x2 = x1_rotated_scaled + warp_parameters[0];
    *y2 = y1_rotated_scaled + warp_parameters[1];
  }

  // Translation x, translation y, rotation about the center of Q1 degrees,
  // scale.
  enum { NUM_PARAMETERS = 4 };
  double parameters[NUM_PARAMETERS];

  Quad q1;
};

struct AffineWarp {
  AffineWarp(const double *x1, const double *y1,
             const double *x2, const double *y2)
      : q1(x1, y1) {
    Quad q2(x2, y2);

    // The difference in centroids is the best guess for translation.
    Vec2 t = q2.Centroid() - q1.Centroid();
    parameters[0] = t[0];
    parameters[1] = t[1];

    // Estimate the four affine parameters with the usual least squares.
    Mat Q1(8, 4);
    Vec Q2(8);
    for (int i = 0; i < 4; ++i) {
      Vec2 v1 = q1.CornerRelativeToCentroid(i);
      Vec2 v2 = q2.CornerRelativeToCentroid(i);

      Q1.row(2 * i + 0) << v1[0], v1[1],   0,     0  ;
      Q1.row(2 * i + 1) <<   0,     0,   v1[0], v1[1];

      Q2(2 * i + 0) = v2[0];
      Q2(2 * i + 1) = v2[1];
    }

    // TODO(keir): Check solution quality.
    Vec4 a = Q1.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(Q2);
    parameters[2] = a[0];
    parameters[3] = a[1];
    parameters[4] = a[2];
    parameters[5] = a[3];

    LG << "a:" << a.transpose();
    LG << "t:" << t.transpose();
  }

  // See comments in other parameterizations about why the centroid is used.
  template<typename T>
  void Forward(const T *p, const T &x1, const T& y1, T *x2, T* y2) const {
    // Make the centroid of Q1 the origin.
    const T x1_origin = x1 - q1.Centroid()(0);
    const T y1_origin = y1 - q1.Centroid()(1);

    // Apply the affine transformation.
    const T x1_origin_affine = p[2] * x1_origin + p[3] * y1_origin;
    const T y1_origin_affine = p[4] * x1_origin + p[5] * y1_origin;

    // Translate back into the space of Q1 (but affine transformed).
    const T x1_affine = x1_origin_affine + q1.Centroid()(0);
    const T y1_affine = y1_origin_affine + q1.Centroid()(1);

    // Translate into the space of Q2.
    *x2 = x1_affine + p[0];
    *y2 = y1_affine + p[1];
  }

  // Translation x, translation y, rotation about the center of Q1 degrees,
  // scale.
  enum { NUM_PARAMETERS = 6 };
  double parameters[NUM_PARAMETERS];

  Quad q1;
};

struct HomographyWarp {
  HomographyWarp(const double *x1, const double *y1,
                 const double *x2, const double *y2) {
    Mat quad1(2, 4);
    quad1 << x1[0], x1[1], x1[2], x1[3],
             y1[0], y1[1], y1[2], y1[3];

    Mat quad2(2, 4);
    quad2 << x2[0], x2[1], x2[2], x2[3],
             y2[0], y2[1], y2[2], y2[3];

    Mat3 H;
    if (!Homography2DFromCorrespondencesLinear(quad1, quad2, &H, 1e-12)) {
      LG << "Couldn't construct homography.";
    }

    // Assume H(2, 2) != 0, and fix scale at H(2, 2) == 1.0.
    H /= H(2, 2);

    // Assume H is close to identity, so subtract out the diagonal.
    H(0, 0) -= 1.0;
    H(1, 1) -= 1.0;

    CHECK_NE(H(2, 2), 0.0) << H;
    for (int i = 0; i < 8; ++i) {
      parameters[i] = H(i / 3, i % 3);
      LG << "Parameters[" << i << "]: " << parameters[i];
    }
  }

  template<typename T>
  static void Forward(const T *p,
                      const T &x1, const T& y1, T *x2, T* y2) {
    // Homography warp with manual 3x3 matrix multiply.
    const T xx2 = (1.0 + p[0]) * x1 +     p[1]     * y1 + p[2];
    const T yy2 =     p[3]     * x1 + (1.0 + p[4]) * y1 + p[5];
    const T zz2 =     p[6]     * x1 +     p[7]     * y1 + 1.0;
    *x2 = xx2 / zz2;
    *y2 = yy2 / zz2;
  }

  enum { NUM_PARAMETERS = 8 };
  double parameters[NUM_PARAMETERS];
};

// Determine the number of samples to use for x and y. Quad winding goes:
//
//    0 1
//    3 2
//
// The idea is to take the maximum x or y distance. This may be oversampling.
// TODO(keir): Investigate the various choices; perhaps average is better?
void PickSampling(const double *x1, const double *y1,
                  const double *x2, const double *y2,
                  int *num_samples_x, int *num_samples_y) {
  Vec2 a0(x1[0], y1[0]);
  Vec2 a1(x1[1], y1[1]);
  Vec2 a2(x1[2], y1[2]);
  Vec2 a3(x1[3], y1[3]);

  Vec2 b0(x1[0], y1[0]);
  Vec2 b1(x1[1], y1[1]);
  Vec2 b2(x1[2], y1[2]);
  Vec2 b3(x1[3], y1[3]);

  double x_dimensions[4] = {
    (a1 - a0).norm(),
    (a3 - a2).norm(),
    (b1 - b0).norm(),
    (b3 - b2).norm()
  };

  double y_dimensions[4] = {
    (a3 - a0).norm(),
    (a1 - a2).norm(),
    (b3 - b0).norm(),
    (b1 - b2).norm()
  };
  const double kScaleFactor = 1.0;
  *num_samples_x = static_cast<int>(
      kScaleFactor * *std::max_element(x_dimensions, x_dimensions + 4));
  *num_samples_y = static_cast<int>(
      kScaleFactor * *std::max_element(y_dimensions, y_dimensions + 4));
  LG << "Automatic num_samples_x: " << *num_samples_x
     << ", num_samples_y: " << *num_samples_y;
}

bool SearchAreaTooBigForDescent(const FloatImage &image2,
                                const double *x2, const double *y2) {
  // TODO(keir): Check the bounds and enable only when it makes sense.
  return true;
}

bool PointOnRightHalfPlane(const Vec2 &a, const Vec2 &b, double x, double y) {
  Vec2 ba = b - a;
  return ((Vec2(x, y) - b).transpose() * Vec2(-ba.y(), ba.x())) > 0;
}

// Determine if a point is in a quad. The quad is arranged as:
//
//    +-------> x
//    |
//    |  a0------a1
//    |   |       |
//    |   |       |
//    |   |       |
//    |  a3------a2
//    v
//    y
//
// The implementation does up to four half-plane comparisons.
bool PointInQuad(const double *xs, const double *ys, double x, double y) {
  Vec2 a0(xs[0], ys[0]);
  Vec2 a1(xs[1], ys[1]);
  Vec2 a2(xs[2], ys[2]);
  Vec2 a3(xs[3], ys[3]);

  return PointOnRightHalfPlane(a0, a1, x, y) &&
         PointOnRightHalfPlane(a1, a2, x, y) &&
         PointOnRightHalfPlane(a2, a3, x, y) &&
         PointOnRightHalfPlane(a3, a0, x, y);
}

// This makes it possible to map between Eigen float arrays and FloatImage
// without using comparisons.
typedef Eigen::Array<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> FloatArray;

// This creates a pattern in the frame of image2, from the pixel is image1,
// based on the initial guess represented by the two quads x1, y1, and x2, y2.
template<typename Warp>
void CreateBrutePattern(const double *x1, const double *y1,
                        const double *x2, const double *y2,
                        const FloatImage &image1,
                        const FloatImage *image1_mask,
                        FloatArray *pattern,
                        FloatArray *mask,
                        int *origin_x,
                        int *origin_y) {
  // Get integer bounding box of quad2 in image2.
  int min_x = static_cast<int>(floor(*std::min_element(x2, x2 + 4)));
  int min_y = static_cast<int>(floor(*std::min_element(y2, y2 + 4)));
  int max_x = static_cast<int>(ceil (*std::max_element(x2, x2 + 4)));
  int max_y = static_cast<int>(ceil (*std::max_element(y2, y2 + 4)));

  int w = max_x - min_x;
  int h = max_y - min_y;

  pattern->resize(h, w);
  mask->resize(h, w);

  Warp inverse_warp(x2, y2, x1, y1);

  // r,c are in the coordinate frame of image2.
  for (int r = min_y; r < max_y; ++r) {
    for (int c = min_x; c < max_x; ++c) {
      // i and j are in the coordinate frame of the pattern in image2.
      int i = r - min_y;
      int j = c - min_x;

      double dst_x = c;
      double dst_y = r;
      double src_x;
      double src_y;
      inverse_warp.Forward(inverse_warp.parameters,
                           dst_x, dst_y,
                           &src_x, &src_y);
      
      if (PointInQuad(x1, y1, src_x, src_y)) {
        (*pattern)(i, j) = SampleLinear(image1, src_y, src_x);
        (*mask)(i, j) = 1.0;
        if (image1_mask) {
          (*mask)(i, j) = SampleLinear(*image1_mask, src_y, src_x);;
        }
      } else {
        (*pattern)(i, j) = 0.0;
        (*mask)(i, j) = 0.0;
      }
    }
  }
  *origin_x = min_x;
  *origin_y = min_y;
}

// Compute a translation-only estimate of the warp, using brute force search. A
// smarter implementation would use the FFT to compute the normalized cross
// correlation. Instead, this is a dumb implementation. Surprisingly, it is
// fast enough in practice.
//
// TODO(keir): The normalization is less effective for the brute force search
// than it is with the Ceres solver. It's unclear if this is a bug or due to
// the original frame being too different from the reprojected reference in the
// destination frame.
//
// The likely solution is to use the previous frame, instead of the original
// pattern, when doing brute initialization. Unfortunately that implies a
// totally different warping interface, since access to more than a the source
// and current destination frame is necessary.
template<typename Warp>
void BruteTranslationOnlyInitialize(const FloatImage &image1,
                                    const FloatImage *image1_mask,
                                    const FloatImage &image2,
                                    const int num_extra_points,
                                    const bool use_normalized_intensities,
                                    const double *x1, const double *y1,
                                    double *x2, double *y2) {
  // Create the pattern to match in the space of image2, assuming our inital
  // guess isn't too far from the template in image1. If there is no image1
  // mask, then the resulting mask is binary.
  FloatArray pattern;
  FloatArray mask;
  int origin_x = -1, origin_y = -1;
  CreateBrutePattern<Warp>(x1, y1, x2, y2,
                           image1, image1_mask,
                           &pattern, &mask,
                           &origin_x, &origin_y);

  // For normalization, premultiply the pattern by the inverse pattern mean.
  double mask_sum = 1.0;
  if (use_normalized_intensities) {
    mask_sum = mask.sum();
    double inverse_pattern_mean = mask_sum / ((mask * pattern).sum());
    pattern *= inverse_pattern_mean;
  }

  // Use Eigen on the images via maps for strong vectorization.
  Map<const FloatArray> search(image2.Data(), image2.Height(), image2.Width());

  // Try all possible locations inside the search area. Yes, everywhere.
  //
  // TODO(keir): There are a number of possible optimizations here. One choice
  // is to make a grid and only try one out of every N possible samples.
  // 
  // Another, slightly more clever idea, is to compute some sort of spatial
  // frequency distribution of the pattern patch. If the spatial resolution is
  // high (e.g. a grating pattern or fine lines) then checking every possible
  // translation is necessary, since a 1-pixel shift may induce a massive
  // change in the cost function. If the image is a blob or splotch with blurry
  // edges, then fewer samples are necessary since a few pixels offset won't
  // change the cost function much.
  double best_sad = std::numeric_limits<double>::max();
  int best_r = -1;
  int best_c = -1;
  int w = pattern.cols();
  int h = pattern.rows();
  for (int r = 0; r < (image2.Height() - h); ++r) {
    for (int c = 0; c < (image2.Width() - w); ++c) {
      // Compute the weighted sum of absolute differences, Eigen style. Note
      // that the block from the search image is never stored in a variable, to
      // avoid copying overhead and permit inlining.
      double sad;
      if (use_normalized_intensities) {
        // TODO(keir): It's really dumb to recompute the search mean for every
        // shift. A smarter implementation would use summed area tables
        // instead, reducing the mean calculation to an O(1) operation.
        double inverse_search_mean =
            mask_sum / ((mask * search.block(r, c, h, w)).sum());
        sad = (mask * (pattern - (search.block(r, c, h, w) *
                                  inverse_search_mean))).abs().sum();
      } else {
        sad = (mask * (pattern - search.block(r, c, h, w))).abs().sum();
      }
      if (sad < best_sad) {
        best_r = r;
        best_c = c;
        best_sad = sad;
      }
    }
  }
  CHECK_NE(best_r, -1);
  CHECK_NE(best_c, -1);

  LG << "Brute force translation found a shift. "
     << "best_c: " << best_c << ", best_r: " << best_r << ", "
     << "origin_x: " << origin_x << ", origin_y: " << origin_y << ", "
     << "dc: " << (best_c - origin_x) << ", "
     << "dr: " << (best_r - origin_y)
     << ", tried " << ((image2.Height() - h) * (image2.Width() - w))
     << " shifts.";

  // Apply the shift.
  for (int i = 0; i < 4 + num_extra_points; ++i) {
    x2[i] += best_c - origin_x;
    y2[i] += best_r - origin_y;
  }
}

}  // namespace

template<typename Warp>
void TemplatedTrackRegion(const FloatImage &image1,
                          const FloatImage &image2,
                          const double *x1, const double *y1,
                          const TrackRegionOptions &options,
                          double *x2, double *y2,
                          TrackRegionResult *result) {
  for (int i = 0; i < 4; ++i) {
    LG << "P" << i << ": (" << x1[i] << ", " << y1[i] << "); guess ("
       << x2[i] << ", " << y2[i] << "); (dx, dy): (" << (x2[i] - x1[i]) << ", "
       << (y2[i] - y1[i]) << ").";
  }
  if (options.use_normalized_intensities) {
    LG << "Using normalized intensities.";
  }

  // Bail early if the points are already outside.
  if (!AllInBounds(image1, x1, y1)) {
    result->termination = TrackRegionResult::SOURCE_OUT_OF_BOUNDS;
    return;
  }
  if (!AllInBounds(image2, x2, y2)) {
    result->termination = TrackRegionResult::DESTINATION_OUT_OF_BOUNDS;
    return;
  }
  // TODO(keir): Check quads to ensure there is some area.

  // Keep a copy of the "original" guess for regularization.
  double x2_original[4];
  double y2_original[4];
  for (int i = 0; i < 4; ++i) {
    x2_original[i] = x2[i];
    y2_original[i] = y2[i];
  }

  // Prepare the image and gradient.
  Array3Df image_and_gradient1;
  Array3Df image_and_gradient2;
  BlurredImageAndDerivativesChannels(image1, options.sigma,
                                     &image_and_gradient1);
  BlurredImageAndDerivativesChannels(image2, options.sigma,
                                     &image_and_gradient2);

  // Possibly do a brute-force translation-only initialization.
  if (SearchAreaTooBigForDescent(image2, x2, y2) &&
      options.use_brute_initialization) {
    LG << "Running brute initialization...";
    BruteTranslationOnlyInitialize<Warp>(image_and_gradient1,
                                         options.image1_mask,
                                         image2,
                                         options.num_extra_points,
                                         options.use_normalized_intensities,
                                         x1, y1, x2, y2);
    for (int i = 0; i < 4; ++i) {
      LG << "P" << i << ": (" << x1[i] << ", " << y1[i] << "); brute ("
         << x2[i] << ", " << y2[i] << "); (dx, dy): (" << (x2[i] - x1[i])
         << ", " << (y2[i] - y1[i]) << ").";
    }
  }

  // Prepare the initial warp parameters from the four correspondences.
  // Note: This must happen after the brute initialization runs, since the
  // brute initialization mutates x2 and y2 in place.
  Warp warp(x1, y1, x2, y2);

  // Decide how many samples to use in the x and y dimensions.
  int num_samples_x;
  int num_samples_y;
  PickSampling(x1, y1, x2, y2, &num_samples_x, &num_samples_y);


  // Compute the warp from rectangular coordinates.
  Mat3 canonical_homography = ComputeCanonicalHomography(x1, y1,
                                                         num_samples_x,
                                                         num_samples_y);

  ceres::Problem problem;

  // Construct the warp cost function. AutoDiffCostFunction takes ownership.
  PixelDifferenceCostFunctor<Warp> *pixel_difference_cost_function =
      new PixelDifferenceCostFunctor<Warp>(options,
                                           image_and_gradient1,
                                           image_and_gradient2,
                                           canonical_homography,
                                           num_samples_x,
                                           num_samples_y,
                                           warp);
   problem.AddResidualBlock(
       new ceres::AutoDiffCostFunction<
           PixelDifferenceCostFunctor<Warp>,
           ceres::DYNAMIC,
           Warp::NUM_PARAMETERS>(pixel_difference_cost_function,
                                 num_samples_x * num_samples_y),
       NULL,
       warp.parameters);

  // Construct the regularizing cost function
  if (options.regularization_coefficient != 0.0) {
    WarpRegularizingCostFunctor<Warp> *regularizing_warp_cost_function =
        new WarpRegularizingCostFunctor<Warp>(options,
                                              x1, y2,
                                              x2_original,
                                              y2_original,
                                              warp);

    problem.AddResidualBlock(
        new ceres::AutoDiffCostFunction<
            WarpRegularizingCostFunctor<Warp>,
            8 /* num_residuals */,
            Warp::NUM_PARAMETERS>(regularizing_warp_cost_function),
        NULL,
        warp.parameters);
  }

  // Configure the solve.
  ceres::Solver::Options solver_options;
  solver_options.linear_solver_type = ceres::DENSE_QR;
  solver_options.max_num_iterations = options.max_iterations;
  solver_options.update_state_every_iteration = true;
  solver_options.parameter_tolerance = 1e-16;
  solver_options.function_tolerance = 1e-16;

  // Prevent the corners from going outside the destination image.
  BoundaryCheckingCallback<Warp> callback(image2, warp, x1, y1);
  solver_options.callbacks.push_back(&callback);

  // Run the solve.
  ceres::Solver::Summary summary;
  ceres::Solve(solver_options, &problem, &summary);

  LG << "Summary:\n" << summary.FullReport();

  // Update the four points with the found solution; if the solver failed, then
  // the warp parameters are the identity (so ignore failure).
  //
  // Also warp any extra points on the end of the array.
  for (int i = 0; i < 4 + options.num_extra_points; ++i) {
    warp.Forward(warp.parameters, x1[i], y1[i], x2 + i, y2 + i);
    LG << "Warped point " << i << ": (" << x1[i] << ", " << y1[i] << ") -> ("
       << x2[i] << ", " << y2[i] << "); (dx, dy): (" << (x2[i] - x1[i]) << ", "
       << (y2[i] - y1[i]) << ").";
  }

  // TODO(keir): Update the result statistics.
  // TODO(keir): Add a normalize-cross-correlation variant.

  CHECK_NE(summary.termination_type, ceres::USER_ABORT) << "Libmv bug.";
  if (summary.termination_type == ceres::USER_ABORT) {
    result->termination = TrackRegionResult::FELL_OUT_OF_BOUNDS;
    return;
  }
#define HANDLE_TERMINATION(termination_enum) \
  if (summary.termination_type == ceres::termination_enum) { \
    result->termination = TrackRegionResult::termination_enum; \
    return; \
  }

  // Avoid computing correlation for tracking failures.
  HANDLE_TERMINATION(DID_NOT_RUN);
  HANDLE_TERMINATION(NUMERICAL_FAILURE);

  // Otherwise, run a final correlation check.
  if (options.minimum_correlation > 0.0) {
    result->correlation = pixel_difference_cost_function->
        PearsonProductMomentCorrelationCoefficient(warp.parameters);
    if (result->correlation < options.minimum_correlation) {
      LG << "Failing with insufficient correlation.";
      result->termination = TrackRegionResult::INSUFFICIENT_CORRELATION;
      return;
    }
  }

  HANDLE_TERMINATION(PARAMETER_TOLERANCE);
  HANDLE_TERMINATION(FUNCTION_TOLERANCE);
  HANDLE_TERMINATION(GRADIENT_TOLERANCE);
  HANDLE_TERMINATION(NO_CONVERGENCE);
#undef HANDLE_TERMINATION
};

void TrackRegion(const FloatImage &image1,
                 const FloatImage &image2,
                 const double *x1, const double *y1,
                 const TrackRegionOptions &options,
                 double *x2, double *y2,
                 TrackRegionResult *result) {
  // Enum is necessary due to templated nature of autodiff.
#define HANDLE_MODE(mode_enum, mode_type) \
  if (options.mode == TrackRegionOptions::mode_enum) { \
    TemplatedTrackRegion<mode_type>(image1, image2, \
                                    x1, y1, \
                                    options, \
                                    x2, y2, \
                                    result); \
    return; \
  }
  HANDLE_MODE(TRANSLATION,                TranslationWarp);
  HANDLE_MODE(TRANSLATION_SCALE,          TranslationScaleWarp);
  HANDLE_MODE(TRANSLATION_ROTATION,       TranslationRotationWarp);
  HANDLE_MODE(TRANSLATION_ROTATION_SCALE, TranslationRotationScaleWarp);
  HANDLE_MODE(AFFINE,                     AffineWarp);
  HANDLE_MODE(HOMOGRAPHY,                 HomographyWarp);
#undef HANDLE_MODE
}

bool SamplePlanarPatch(const FloatImage &image,
                       const double *xs, const double *ys,
                       int num_samples_x, int num_samples_y,
                       FloatImage *patch,
                       double *warped_position_x, double *warped_position_y) {
  // Bail early if the points are outside the image.
  if (!AllInBounds(image, xs, ys)) {
    LG << "Can't sample patch: out of bounds.";
    return false;
  }

  // Make the patch have the appropriate size, and match the depth of image.
  patch->Resize(num_samples_y, num_samples_x, image.Depth());

  // Compute the warp from rectangular coordinates.
  Mat3 canonical_homography = ComputeCanonicalHomography(xs, ys,
                                                         num_samples_x,
                                                         num_samples_y);

  // Walk over the coordinates in the canonical space, sampling from the image
  // in the original space and copying the result into the patch.
  for (int r = 0; r < num_samples_y; ++r) {
    for (int c = 0; c < num_samples_x; ++c) {
      Vec3 image_position = canonical_homography * Vec3(c, r, 1);
      image_position /= image_position(2);
      SampleLinear(image, image_position(1),
                   image_position(0),
                   &(*patch)(r, c, 0));
    }
  }

  Vec3 warped_position = canonical_homography.inverse() * Vec3(xs[4], ys[4], 1);
  warped_position /= warped_position(2);

  *warped_position_x = warped_position(0);
  *warped_position_y = warped_position(1);

  return true;
}

}  // namespace libmv
