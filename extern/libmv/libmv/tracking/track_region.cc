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

// Because C++03 doesn't support partial template specializations for
// functions, but at the same time member function specializations are not
// supported, the sample function must be inside a template-specialized class
// with a non-templated static member.

// The "AutoDiffImage::Sample()" function allows sampling an image at an x, y
// position such that if x and y are jets, then the derivative information is
// correctly propagated.

// Empty default template.
template<typename T>
struct AutoDiffImage {
  static T Sample(const FloatImage &image_and_gradient,
                  const T &x, const T &y) {
    return 0.0;
  }
};

// Sample only the image when the coordinates are scalars.
template<>
struct AutoDiffImage<double> {
  static double Sample(const FloatImage &image_and_gradient,
                       const double& x, const double& y) {
    return SampleLinear(image_and_gradient, y, x, 0);
  }
};

// Sample the image and gradient when the coordinates are jets, applying the
// jacobian appropriately to propagate the derivatives from the coordinates.
template<>
template<typename T, int N>
struct AutoDiffImage<ceres::Jet<T, N> > {
  static ceres::Jet<T, N> Sample(const FloatImage &image_and_gradient,
                                 const ceres::Jet<T, N> &x,
                                 const ceres::Jet<T, N> &y) {
    // Sample the image and its derivatives in x and y. One way to think of
    // this is that the image is a scalar function with a single vector
    // argument, xy, of dimension 2. Call this s(xy).
    const T s    = SampleLinear(image_and_gradient, y.a, x.a, 0);
    const T dsdx = SampleLinear(image_and_gradient, y.a, x.a, 1);
    const T dsdy = SampleLinear(image_and_gradient, y.a, x.a, 2);

    // However, xy is itself a function of another variable ("z"); xy(z) =
    // [x(z), y(z)]^T. What this function needs to return is "s", but with the
    // derivative with respect to z attached to the jet. So combine the
    // derivative part of x and y's jets to form a Jacobian matrix between x, y
    // and z (i.e. dxy/dz).
    Eigen::Matrix<T, 2, N> dxydz;
    dxydz.row(0) = x.v.transpose();
    dxydz.row(1) = y.v.transpose();

    // Now apply the chain rule to obtain ds/dz. Combine the derivative with
    // the scalar part to obtain s with full derivative information.
    ceres::Jet<T, N> jet_s;
    jet_s.a = s;
    jet_s.v = Matrix<T, 1, 2>(dsdx, dsdy) * dxydz;
    return jet_s;
  }
};

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
class WarpCostFunctor {
 public:
  WarpCostFunctor(const FloatImage &image_and_gradient1,
                  const FloatImage &image_and_gradient2,
                  const Mat3 &canonical_to_image1,
                  const Warp &warp,
                  int num_samples_x,
                  int num_samples_y)
      : image_and_gradient1_(image_and_gradient1),       
        image_and_gradient2_(image_and_gradient2),       
        canonical_to_image1_(canonical_to_image1),
        warp_(warp),
        num_samples_x_(num_samples_x),  
        num_samples_y_(num_samples_y) {}

 template<typename T>
 bool operator()(const T *warp_parameters, T *residuals) const {
   for (int i = 0; i < Warp::NUM_PARAMETERS; ++i) {
     VLOG(2) << "warp_parameters[" << i << "]: " << warp_parameters[i];
   }

   int cursor = 0;
   for (int r = 0; r < num_samples_y_; ++r) {
     for (int c = 0; c < num_samples_x_; ++c) {
        // Compute the location of the source pixel (via homography).
        Vec3 image1_position = canonical_to_image1_ * Vec3(c, r, 1);
        image1_position /= image1_position(2);
        
        // Compute the location of the destination pixel.
        T image2_position[2];
        warp_.Forward(warp_parameters,
                      T(image1_position[0]),
                      T(image1_position[1]),
                      &image2_position[0],
                      &image2_position[1]);

        // Sample the source and destination.
        double src_sample = AutoDiffImage<double>::Sample(image_and_gradient1_,
                                                          image1_position[0],
                                                          image1_position[1]);
        T dst_sample = AutoDiffImage<T>::Sample(image_and_gradient2_,
                                                image2_position[0],
                                                image2_position[1]);

        // The difference is the error.
        residuals[cursor++] = T(src_sample) - dst_sample;
      }
    }
    return true;
  }

 private:
  const FloatImage &image_and_gradient1_;
  const FloatImage &image_and_gradient2_;
  const Mat3 &canonical_to_image1_;
  const Warp &warp_;
  int num_samples_x_;
  int num_samples_y_;
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
  Quad(const double *x, const double *y)
      : x_(x), y_(y) {

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
    Vec2 t = Quad(x1, y1).Centroid() - Quad(x2, y2).Centroid();
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
    Vec2 t = q1.Centroid() - q2.Centroid();
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
    Vec2 t = q1.Centroid() - q2.Centroid();
    parameters[0] = t[0];
    parameters[1] = t[1];

    // Obtain the rotation via orthorgonal procrustes.
    Mat2 correlation_matrix;
    for (int i = 0; i < 4; ++i) {
      correlation_matrix += q1.CornerRelativeToCentroid(i) * 
                            q2.CornerRelativeToCentroid(i).transpose();
    }
    Mat2 R = OrthogonalProcrustes(correlation_matrix);
    parameters[2] = acos(R(0, 0));
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
    Vec2 t = q1.Centroid() - q2.Centroid();
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
    parameters[3] = acos(R(0, 0));
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

// TODO(keir): Finish affine warp.

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

template<typename Warp>
void TemplatedTrackRegion(const FloatImage &image1,
                          const FloatImage &image2,
                          const double *x1, const double *y1,
                          const TrackRegionOptions &options,
                          double *x2, double *y2,
                          TrackRegionResult *result) {
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

  // Prepare the image and gradient.
  Array3Df image_and_gradient1;
  Array3Df image_and_gradient2;
  BlurredImageAndDerivativesChannels(image1, options.sigma,
                                     &image_and_gradient1);
  BlurredImageAndDerivativesChannels(image2, options.sigma,
                                     &image_and_gradient2);

  // Prepare the initial warp parameters from the four correspondences.
  Warp warp(x1, y1, x2, y2);

  ceres::Solver::Options solver_options;
  solver_options.linear_solver_type = ceres::DENSE_QR;
  solver_options.max_num_iterations = options.max_iterations;
  solver_options.update_state_every_iteration = true;
  solver_options.parameter_tolerance = 1e-16;
  solver_options.function_tolerance = 1e-16;

  // TODO(keir): Consider removing these options before committing.
  solver_options.numeric_derivative_relative_step_size = 1e-3;
  solver_options.gradient_check_relative_precision = 1e-10;
  solver_options.minimizer_progress_to_stdout = false;

  // Prevent the corners from going outside the destination image.
  BoundaryCheckingCallback<Warp> callback(image2, warp, x1, y1);
  solver_options.callbacks.push_back(&callback);

  // Compute the warp from rectangular coordinates.
  Mat3 canonical_homography = ComputeCanonicalHomography(x1, y1,
                                                         options.num_samples_x,
                                                         options.num_samples_y);

  // Construct the warp cost function. AutoDiffCostFunction takes ownership.
  WarpCostFunctor<Warp> *cost_function =
      new WarpCostFunctor<Warp>(image_and_gradient1,
                                image_and_gradient2,
                                canonical_homography,
                                warp,
                                options.num_samples_x,
                                options.num_samples_y);

  // Construct the problem with a single residual.
  ceres::Problem::Options problem_options;
  problem_options.cost_function_ownership = ceres::DO_NOT_TAKE_OWNERSHIP;
  ceres::Problem problem(problem_options);
  problem.AddResidualBlock(
      new ceres::AutoDiffCostFunction<
          WarpCostFunctor<Warp>,
          ceres::DYNAMIC,
          Warp::NUM_PARAMETERS>(cost_function,
                                options.num_samples_x * options.num_samples_y),
      NULL,
      warp.parameters);

  ceres::Solver::Summary summary;
  ceres::Solve(solver_options, &problem, &summary);

  LG << "Summary:\n" << summary.FullReport();

  // Update the four points with the found solution; if the solver failed, then
  // the warp parameters are the identity (so ignore failure).
  for (int i = 0; i < 4; ++i) {
    warp.Forward(warp.parameters, x1[i], y1[i], x2 + i, y2 + i);
  }

  // TODO(keir): Update the result statistics.

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
  HANDLE_TERMINATION(PARAMETER_TOLERANCE);
  HANDLE_TERMINATION(FUNCTION_TOLERANCE);
  HANDLE_TERMINATION(GRADIENT_TOLERANCE);
  HANDLE_TERMINATION(NO_CONVERGENCE);
  HANDLE_TERMINATION(DID_NOT_RUN);
  HANDLE_TERMINATION(NUMERICAL_FAILURE);
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
  //HANDLE_MODE(AFFINE,                     AffineWarp);
  HANDLE_MODE(HOMOGRAPHY,                 HomographyWarp);
#undef HANDLE_MODE
}

}  // namespace libmv
