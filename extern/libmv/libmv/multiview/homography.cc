// Copyright (c) 2008, 2009 libmv authors.
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

#include "libmv/multiview/homography.h"

#include "ceres/ceres.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/conditioning.h"
#include "libmv/multiview/homography_parameterization.h"

namespace libmv {
/** 2D Homography transformation estimation in the case that points are in 
 * euclidean coordinates.
 *
 * x = H y
 * x and y vector must have the same direction, we could write
 * crossproduct(|x|, * H * |y| ) = |0|
 *
 * | 0 -1  x2|   |a b c|   |y1|    |0|
 * | 1  0 -x1| * |d e f| * |y2| =  |0|
 * |-x2  x1 0|   |g h 1|   |1 |    |0|
 *
 * That gives :
 *
 * (-d+x2*g)*y1    + (-e+x2*h)*y2 + -f+x2          |0|
 * (a-x1*g)*y1     + (b-x1*h)*y2  + c-x1         = |0|
 * (-x2*a+x1*d)*y1 + (-x2*b+x1*e)*y2 + -x2*c+x1*f  |0|
 */
static bool Homography2DFromCorrespondencesLinearEuc(
    const Mat &x1,
    const Mat &x2,
    Mat3 *H,
    double expected_precision) {
  assert(2 == x1.rows());
  assert(4 <= x1.cols());
  assert(x1.rows() == x2.rows());
  assert(x1.cols() == x2.cols());

  int n = x1.cols();
  MatX8 L = Mat::Zero(n * 3, 8);
  Mat b = Mat::Zero(n * 3, 1);
  for (int i = 0; i < n; ++i) {
    int j = 3 * i;
    L(j, 0) =  x1(0, i);             // a
    L(j, 1) =  x1(1, i);             // b
    L(j, 2) =  1.0;                  // c
    L(j, 6) = -x2(0, i) * x1(0, i);  // g
    L(j, 7) = -x2(0, i) * x1(1, i);  // h
    b(j, 0) =  x2(0, i);             // i

    ++j;
    L(j, 3) =  x1(0, i);             // d
    L(j, 4) =  x1(1, i);             // e
    L(j, 5) =  1.0;                  // f
    L(j, 6) = -x2(1, i) * x1(0, i);  // g
    L(j, 7) = -x2(1, i) * x1(1, i);  // h
    b(j, 0) =  x2(1, i);             // i

    // This ensures better stability
    // TODO(julien) make a lite version without this 3rd set
    ++j;
    L(j, 0) =  x2(1, i) * x1(0, i);  // a
    L(j, 1) =  x2(1, i) * x1(1, i);  // b
    L(j, 2) =  x2(1, i);             // c
    L(j, 3) = -x2(0, i) * x1(0, i);  // d
    L(j, 4) = -x2(0, i) * x1(1, i);  // e
    L(j, 5) = -x2(0, i);             // f
  }
  // Solve Lx=B
  Vec h = L.fullPivLu().solve(b);
  Homography2DNormalizedParameterization<double>::To(h, H);
  if ((L * h).isApprox(b, expected_precision))  {
    return true;
  } else {
    return false;
  }
}

/** 2D Homography transformation estimation in the case that points are in 
 * homogeneous coordinates.
 *
 * | 0 -x3  x2|   |a b c|   |y1|   -x3*d+x2*g -x3*e+x2*h -x3*f+x2*1    |y1|   (-x3*d+x2*g)*y1 (-x3*e+x2*h)*y2 (-x3*f+x2*1)*y3   |0|
 * | x3  0 -x1| * |d e f| * |y2| =  x3*a-x1*g  x3*b-x1*h  x3*c-x1*1  * |y2| =  (x3*a-x1*g)*y1  (x3*b-x1*h)*y2  (x3*c-x1*1)*y3 = |0|
 * |-x2  x1  0|   |g h 1|   |y3|   -x2*a+x1*d -x2*b+x1*e -x2*c+x1*f    |y3|   (-x2*a+x1*d)*y1 (-x2*b+x1*e)*y2 (-x2*c+x1*f)*y3   |0|
 * X = |a b c d e f g h|^t
 */
bool Homography2DFromCorrespondencesLinear(const Mat &x1,
                                           const Mat &x2,
                                           Mat3 *H,
                                           double expected_precision) {
  if (x1.rows() == 2) {
    return Homography2DFromCorrespondencesLinearEuc(x1, x2, H,
                                                    expected_precision);
  }
  assert(3 == x1.rows());
  assert(4 <= x1.cols());
  assert(x1.rows() == x2.rows());
  assert(x1.cols() == x2.cols());

  const int x = 0;
  const int y = 1;
  const int w = 2;
  int n = x1.cols();
  MatX8 L = Mat::Zero(n * 3, 8);
  Mat b = Mat::Zero(n * 3, 1);
  for (int i = 0; i < n; ++i) {
    int j = 3 * i;
    L(j, 0) =  x2(w, i) * x1(x, i);  // a
    L(j, 1) =  x2(w, i) * x1(y, i);  // b
    L(j, 2) =  x2(w, i) * x1(w, i);  // c
    L(j, 6) = -x2(x, i) * x1(x, i);  // g
    L(j, 7) = -x2(x, i) * x1(y, i);  // h
    b(j, 0) =  x2(x, i) * x1(w, i);

    ++j;
    L(j, 3) =  x2(w, i) * x1(x, i);  // d
    L(j, 4) =  x2(w, i) * x1(y, i);  // e
    L(j, 5) =  x2(w, i) * x1(w, i);  // f
    L(j, 6) = -x2(y, i) * x1(x, i);  // g
    L(j, 7) = -x2(y, i) * x1(y, i);  // h
    b(j, 0) =  x2(y, i) * x1(w, i);

    // This ensures better stability
    ++j;
    L(j, 0) =  x2(y, i) * x1(x, i);  // a
    L(j, 1) =  x2(y, i) * x1(y, i);  // b
    L(j, 2) =  x2(y, i) * x1(w, i);  // c
    L(j, 3) = -x2(x, i) * x1(x, i);  // d
    L(j, 4) = -x2(x, i) * x1(y, i);  // e
    L(j, 5) = -x2(x, i) * x1(w, i);  // f
  }
  // Solve Lx=B
  Vec h = L.fullPivLu().solve(b);
  if ((L * h).isApprox(b, expected_precision))  {
    Homography2DNormalizedParameterization<double>::To(h, H);
    return true;
  } else {
    return false;
  }
}

// Default settings for homography estimation which should be suitable
// for a wide range of use cases.
EstimateHomographyOptions::EstimateHomographyOptions(void) :
    use_normalization(true),
    max_num_iterations(50),
    expected_average_symmetric_distance(1e-16) {
}

namespace {
void GetNormalizedPoints(const Mat &original_points,
                         Mat *normalized_points,
                         Mat3 *normalization_matrix) {
  IsotropicPreconditionerFromPoints(original_points, normalization_matrix);
  ApplyTransformationToPoints(original_points,
                              *normalization_matrix,
                              normalized_points);
}

// Cost functor which computes symmetric geometric distance
// used for homography matrix refinement.
class HomographySymmetricGeometricCostFunctor {
 public:
  HomographySymmetricGeometricCostFunctor(const Vec2 &x,
                                          const Vec2 &y)
      : x_(x), y_(y) { }

  template<typename T>
  bool operator()(const T *homography_parameters, T *residuals) const {
    typedef Eigen::Matrix<T, 3, 3> Mat3;
    typedef Eigen::Matrix<T, 3, 1> Vec3;

    Mat3 H(homography_parameters);

    Vec3 x(T(x_(0)), T(x_(1)), T(1.0));
    Vec3 y(T(y_(0)), T(y_(1)), T(1.0));

    Vec3 H_x = H * x;
    Vec3 Hinv_y = H.inverse() * y;

    H_x /= H_x(2);
    Hinv_y /= Hinv_y(2);

    // This is a forward error.
    residuals[0] = H_x(0) - T(y_(0));
    residuals[1] = H_x(1) - T(y_(1));

    // This is a backward error.
    residuals[2] = Hinv_y(0) - T(x_(0));
    residuals[3] = Hinv_y(1) - T(x_(1));

    return true;
  }

  const Vec2 x_;
  const Vec2 y_;
};

// Termination checking callback used for homography estimation.
// It finished the minimization as soon as actual average of
// symmetric geometric distance is less or equal to the expected
// average value.
class TerminationCheckingCallback : public ceres::IterationCallback {
 public:
  TerminationCheckingCallback(const Mat &x1, const Mat &x2,
                              const EstimateHomographyOptions &options,
                              Mat3 *H)
      : options_(options), x1_(x1), x2_(x2), H_(H) {}

  virtual ceres::CallbackReturnType operator()(
      const ceres::IterationSummary& summary) {
    // If the step wasn't successful, there's nothing to do.
    if (!summary.step_is_successful) {
      return ceres::SOLVER_CONTINUE;
    }

    // Calculate average of symmetric geometric distance.
    double average_distance = 0.0;
    for (int i = 0; i < x1_.cols(); i++) {
      average_distance = SymmetricGeometricDistance(*H_,
                                                    x1_.col(i),
                                                    x2_.col(i));
    }
    average_distance /= x1_.cols();

    if (average_distance <= options_.expected_average_symmetric_distance) {
      return ceres::SOLVER_TERMINATE_SUCCESSFULLY;
    }

    return ceres::SOLVER_CONTINUE;
  }

 private:
  const EstimateHomographyOptions &options_;
  const Mat &x1_;
  const Mat &x2_;
  Mat3 *H_;
};
}  // namespace

/** 2D Homography transformation estimation in the case that points are in
 * euclidean coordinates.
 */
bool EstimateHomography2DFromCorrespondences(
    const Mat &x1,
    const Mat &x2,
    const EstimateHomographyOptions &options,
    Mat3 *H) {
  // TODO(sergey): Support homogenous coordinates, not just euclidean.

  assert(2 == x1.rows());
  assert(4 <= x1.cols());
  assert(x1.rows() == x2.rows());
  assert(x1.cols() == x2.cols());

  Mat3 T1 = Mat3::Identity(),
       T2 = Mat3::Identity();

  // Step 1: Algebraic homography estimation.
  Mat x1_normalized, x2_normalized;

  if (options.use_normalization) {
    LG << "Estimating homography using normalization.";
    GetNormalizedPoints(x1, &x1_normalized, &T1);
    GetNormalizedPoints(x2, &x2_normalized, &T2);
  } else {
    x1_normalized = x1;
    x2_normalized = x2;
  }

  // Assume algebraic estiation always suceeds,
  Homography2DFromCorrespondencesLinear(x1_normalized, x2_normalized, H);

  // Denormalize the homography matrix.
  if (options.use_normalization) {
    *H = T2.inverse() * (*H) * T1;
  }

  LG << "Estimated matrix after algebraic estimation:\n" << *H;

  // Step 2: Refine matrix using Ceres minimizer.
  ceres::Problem problem;
  for (int i = 0; i < x1.cols(); i++) {
    HomographySymmetricGeometricCostFunctor
        *homography_symmetric_geometric_cost_function =
            new HomographySymmetricGeometricCostFunctor(x1.col(i),
                                                        x2.col(i));

    problem.AddResidualBlock(
        new ceres::AutoDiffCostFunction<
            HomographySymmetricGeometricCostFunctor,
            4,  // num_residuals
            9>(homography_symmetric_geometric_cost_function),
        NULL,
        H->data());
  }

  // Configure the solve.
  ceres::Solver::Options solver_options;
  solver_options.linear_solver_type = ceres::DENSE_QR;
  solver_options.max_num_iterations = options.max_num_iterations;
  solver_options.update_state_every_iteration = true;

  // Terminate if the average symmetric distance is good enough.
  TerminationCheckingCallback callback(x1, x2, options, H);
  solver_options.callbacks.push_back(&callback);

  // Run the solve.
  ceres::Solver::Summary summary;
  ceres::Solve(solver_options, &problem, &summary);

  VLOG(1) << "Summary:\n" << summary.FullReport();

  LG << "Final refined matrix:\n" << *H;

  return summary.IsSolutionUsable();
}

/**
 * x2 ~ A * x1
 * x2^t * Hi * A *x1 = 0
 * H1 =              H2 =               H3 = 
 * | 0 0 0 1|     |-x2w|  |0 0 0 0|      |  0 |  | 0 0 1 0|     |-x2z|
 * | 0 0 0 0|  -> |  0 |  |0 0 1 0|   -> |-x2z|  | 0 0 0 0|  -> |  0 |
 * | 0 0 0 0|     |  0 |  |0-1 0 0|      | x2y|  |-1 0 0 0|     | x2x|
 * |-1 0 0 0|     | x2x|  |0 0 0 0|      |  0 |  | 0 0 0 0|     |  0 |
 * H4 =              H5 =               H6 = 
 *  |0 0 0 0|     |  0 |  | 0 1 0 0|     |-x2y|   |0 0 0 0|     |  0 |
 *  |0 0 0 1|  -> |-x2w|  |-1 0 0 0|  -> | x2x|   |0 0 0 0|  -> |  0 |
 *  |0 0 0 0|     |  0 |  | 0 0 0 0|     |  0 |   |0 0 0 1|     |-x2w|
 *  |0-1 0 0|     | x2y|  | 0 0 0 0|     |  0 |   |0 0-1 0|     | x2z|
 *     |a b c d|
 * A = |e f g h|
 *     |i j k l|
 *     |m n o 1|
 *
 * x2^t * H1 * A *x1 = (-x2w*a +x2x*m )*x1x + (-x2w*b +x2x*n )*x1y + (-x2w*c +x2x*o )*x1z + (-x2w*d +x2x*1 )*x1w = 0
 * x2^t * H2 * A *x1 = (-x2z*e +x2y*i )*x1x + (-x2z*f +x2y*j )*x1y + (-x2z*g +x2y*k )*x1z + (-x2z*h +x2y*l )*x1w = 0
 * x2^t * H3 * A *x1 = (-x2z*a +x2x*i )*x1x + (-x2z*b +x2x*j )*x1y + (-x2z*c +x2x*k )*x1z + (-x2z*d +x2x*l )*x1w = 0
 * x2^t * H4 * A *x1 = (-x2w*e +x2y*m )*x1x + (-x2w*f +x2y*n )*x1y + (-x2w*g +x2y*o )*x1z + (-x2w*h +x2y*1 )*x1w = 0
 * x2^t * H5 * A *x1 = (-x2y*a +x2x*e )*x1x + (-x2y*b +x2x*f )*x1y + (-x2y*c +x2x*g )*x1z + (-x2y*d +x2x*h )*x1w = 0
 * x2^t * H6 * A *x1 = (-x2w*i +x2z*m )*x1x + (-x2w*j +x2z*n )*x1y + (-x2w*k +x2z*o )*x1z + (-x2w*l +x2z*1 )*x1w = 0
 *
 * X = |a b c d e f g h i j k l m n o|^t
*/
bool Homography3DFromCorrespondencesLinear(const Mat &x1,
                                           const Mat &x2,
                                           Mat4 *H,
                                           double expected_precision) {
  assert(4 == x1.rows());
  assert(5 <= x1.cols());
  assert(x1.rows() == x2.rows());
  assert(x1.cols() == x2.cols());
  const int x = 0;
  const int y = 1;
  const int z = 2;
  const int w = 3;
  int n = x1.cols();
  MatX15 L = Mat::Zero(n * 6, 15);
  Mat b = Mat::Zero(n * 6, 1);
  for (int i = 0; i < n; ++i) {
    int j = 6 * i;
    L(j,  0) = -x2(w, i) * x1(x, i);  // a
    L(j,  1) = -x2(w, i) * x1(y, i);  // b
    L(j,  2) = -x2(w, i) * x1(z, i);  // c
    L(j,  3) = -x2(w, i) * x1(w, i);  // d
    L(j, 12) =  x2(x, i) * x1(x, i);  // m
    L(j, 13) =  x2(x, i) * x1(y, i);  // n
    L(j, 14) =  x2(x, i) * x1(z, i);  // o
    b(j,  0) = -x2(x, i) * x1(w, i);

    ++j;
    L(j,  4) = -x2(z, i) * x1(x, i);  // e
    L(j,  5) = -x2(z, i) * x1(y, i);  // f
    L(j,  6) = -x2(z, i) * x1(z, i);  // g
    L(j,  7) = -x2(z, i) * x1(w, i);  // h
    L(j,  8) =  x2(y, i) * x1(x, i);  // i
    L(j,  9) =  x2(y, i) * x1(y, i);  // j
    L(j, 10) =  x2(y, i) * x1(z, i);  // k
    L(j, 11) =  x2(y, i) * x1(w, i);  // l

    ++j;
    L(j,  0) = -x2(z, i) * x1(x, i);  // a
    L(j,  1) = -x2(z, i) * x1(y, i);  // b
    L(j,  2) = -x2(z, i) * x1(z, i);  // c
    L(j,  3) = -x2(z, i) * x1(w, i);  // d
    L(j,  8) =  x2(x, i) * x1(x, i);  // i
    L(j,  9) =  x2(x, i) * x1(y, i);  // j
    L(j, 10) =  x2(x, i) * x1(z, i);  // k
    L(j, 11) =  x2(x, i) * x1(w, i);  // l

    ++j;
    L(j,  4) = -x2(w, i) * x1(x, i);  // e
    L(j,  5) = -x2(w, i) * x1(y, i);  // f
    L(j,  6) = -x2(w, i) * x1(z, i);  // g
    L(j,  7) = -x2(w, i) * x1(w, i);  // h
    L(j, 12) =  x2(y, i) * x1(x, i);  // m
    L(j, 13) =  x2(y, i) * x1(y, i);  // n
    L(j, 14) =  x2(y, i) * x1(z, i);  // o
    b(j,  0) = -x2(y, i) * x1(w, i);

    ++j;
    L(j, 0) = -x2(y, i) * x1(x, i);  // a
    L(j, 1) = -x2(y, i) * x1(y, i);  // b
    L(j, 2) = -x2(y, i) * x1(z, i);  // c
    L(j, 3) = -x2(y, i) * x1(w, i);  // d
    L(j, 4) =  x2(x, i) * x1(x, i);  // e
    L(j, 5) =  x2(x, i) * x1(y, i);  // f
    L(j, 6) =  x2(x, i) * x1(z, i);  // g
    L(j, 7) =  x2(x, i) * x1(w, i);  // h

    ++j;
    L(j,  8) = -x2(w, i) * x1(x, i);  // i
    L(j,  9) = -x2(w, i) * x1(y, i);  // j
    L(j, 10) = -x2(w, i) * x1(z, i);  // k
    L(j, 11) = -x2(w, i) * x1(w, i);  // l
    L(j, 12) =  x2(z, i) * x1(x, i);  // m
    L(j, 13) =  x2(z, i) * x1(y, i);  // n
    L(j, 14) =  x2(z, i) * x1(z, i);  // o
    b(j,  0) = -x2(z, i) * x1(w, i);
  }
  // Solve Lx=B
  Vec h = L.fullPivLu().solve(b);
  if ((L * h).isApprox(b, expected_precision))  {
    Homography3DNormalizedParameterization<double>::To(h, H);
    return true;
  } else {
    return false;
  }
}

double SymmetricGeometricDistance(const Mat3 &H,
                                  const Vec2 &x1,
                                  const Vec2 &x2) {
  Vec3 x(x1(0), x1(1), 1.0);
  Vec3 y(x2(0), x2(1), 1.0);

  Vec3 H_x = H * x;
  Vec3 Hinv_y = H.inverse() * y;

  H_x /= H_x(2);
  Hinv_y /= Hinv_y(2);

  return (H_x.head<2>() - y.head<2>()).squaredNorm() +
         (Hinv_y.head<2>() - x.head<2>()).squaredNorm();
}

}  // namespace libmv
