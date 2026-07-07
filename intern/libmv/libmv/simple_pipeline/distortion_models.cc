// Copyright (c) 2014 libmv authors.
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

#include "libmv/simple_pipeline/distortion_models.h"
#include "libmv/numeric/levenberg_marquardt.h"

namespace libmv {

namespace {

struct InvertPolynomialIntrinsicsCostFunction {
 public:
  typedef Vec2 FMatrixType;
  typedef Vec2 XMatrixType;

  InvertPolynomialIntrinsicsCostFunction(const double focal_length_x,
                                         const double focal_length_y,
                                         const double principal_point_x,
                                         const double principal_point_y,
                                         const double k1,
                                         const double k2,
                                         const double k3,
                                         const double p1,
                                         const double p2,
                                         const double image_x,
                                         const double image_y)
      : focal_length_x_(focal_length_x),
        focal_length_y_(focal_length_y),
        principal_point_x_(principal_point_x),
        principal_point_y_(principal_point_y),
        k1_(k1),
        k2_(k2),
        k3_(k3),
        p1_(p1),
        p2_(p2),
        x_(image_x),
        y_(image_y) {}

  Vec2 operator()(const Vec2& u) const {
    double xx, yy;

    ApplyPolynomialDistortionModel(focal_length_x_,
                                   focal_length_y_,
                                   principal_point_x_,
                                   principal_point_y_,
                                   k1_,
                                   k2_,
                                   k3_,
                                   p1_,
                                   p2_,
                                   u(0),
                                   u(1),
                                   &xx,
                                   &yy);

    Vec2 fx;
    fx << (xx - x_), (yy - y_);
    return fx;
  }
  double focal_length_x_;
  double focal_length_y_;
  double principal_point_x_;
  double principal_point_y_;
  double k1_, k2_, k3_;
  double p1_, p2_;
  double x_, y_;
};

struct InvertDivisionIntrinsicsCostFunction {
 public:
  typedef Vec2 FMatrixType;
  typedef Vec2 XMatrixType;

  InvertDivisionIntrinsicsCostFunction(const double focal_length_x,
                                       const double focal_length_y,
                                       const double principal_point_x,
                                       const double principal_point_y,
                                       const double k1,
                                       const double k2,
                                       const double image_x,
                                       const double image_y)
      : focal_length_x_(focal_length_x),
        focal_length_y_(focal_length_y),
        principal_point_x_(principal_point_x),
        principal_point_y_(principal_point_y),
        k1_(k1),
        k2_(k2),
        x_(image_x),
        y_(image_y) {}

  Vec2 operator()(const Vec2& u) const {
    double xx, yy;

    ApplyDivisionDistortionModel(focal_length_x_,
                                 focal_length_y_,
                                 principal_point_x_,
                                 principal_point_y_,
                                 k1_,
                                 k2_,
                                 u(0),
                                 u(1),
                                 &xx,
                                 &yy);

    Vec2 fx;
    fx << (xx - x_), (yy - y_);
    return fx;
  }
  double focal_length_x_;
  double focal_length_y_;
  double principal_point_x_;
  double principal_point_y_;
  double k1_, k2_;
  double x_, y_;
};

struct InvertBrownIntrinsicsCostFunction {
 public:
  typedef Vec2 FMatrixType;
  typedef Vec2 XMatrixType;

  InvertBrownIntrinsicsCostFunction(const double focal_length_x,
                                    const double focal_length_y,
                                    const double principal_point_x,
                                    const double principal_point_y,
                                    const double k1,
                                    const double k2,
                                    const double k3,
                                    const double k4,
                                    const double p1,
                                    const double p2,
                                    const double image_x,
                                    const double image_y)
      : focal_length_x_(focal_length_x),
        focal_length_y_(focal_length_y),
        principal_point_x_(principal_point_x),
        principal_point_y_(principal_point_y),
        k1_(k1),
        k2_(k2),
        k3_(k3),
        k4_(k4),
        p1_(p1),
        p2_(p2),
        x_(image_x),
        y_(image_y) {}

  Vec2 operator()(const Vec2& u) const {
    double xx, yy;

    ApplyBrownDistortionModel(focal_length_x_,
                              focal_length_y_,
                              principal_point_x_,
                              principal_point_y_,
                              k1_,
                              k2_,
                              k3_,
                              k4_,
                              p1_,
                              p2_,
                              u(0),
                              u(1),
                              &xx,
                              &yy);

    Vec2 fx;
    fx << (xx - x_), (yy - y_);
    return fx;
  }
  double focal_length_x_;
  double focal_length_y_;
  double principal_point_x_;
  double principal_point_y_;
  double k1_, k2_, k3_, k4_;
  double p1_, p2_;
  double x_, y_;
};

}  // namespace

void InvertPolynomialDistortionModel(const double focal_length_x,
                                     const double focal_length_y,
                                     const double principal_point_x,
                                     const double principal_point_y,
                                     const double k1,
                                     const double k2,
                                     const double k3,
                                     const double p1,
                                     const double p2,
                                     const double image_x,
                                     const double image_y,
                                     double* normalized_x,
                                     double* normalized_y) {
  // Compute the initial guess. For a camera with no distortion, this will also
  // be the final answer; the LM iteration will terminate immediately.
  Vec2 normalized;
  normalized(0) = (image_x - principal_point_x) / focal_length_x;
  normalized(1) = (image_y - principal_point_y) / focal_length_y;

  typedef LevenbergMarquardt<InvertPolynomialIntrinsicsCostFunction> Solver;

  InvertPolynomialIntrinsicsCostFunction intrinsics_cost(focal_length_x,
                                                         focal_length_y,
                                                         principal_point_x,
                                                         principal_point_y,
                                                         k1,
                                                         k2,
                                                         k3,
                                                         p1,
                                                         p2,
                                                         image_x,
                                                         image_y);
  Solver::SolverParameters params;
  Solver solver(intrinsics_cost);

  /*Solver::Results results =*/solver.minimize(params, &normalized);

  // TODO(keir): Better error handling.

  *normalized_x = normalized(0);
  *normalized_y = normalized(1);
}

void InvertDivisionDistortionModel(const double focal_length_x,
                                   const double focal_length_y,
                                   const double principal_point_x,
                                   const double principal_point_y,
                                   const double k1,
                                   const double k2,
                                   const double image_x,
                                   const double image_y,
                                   double* normalized_x,
                                   double* normalized_y) {
  // Compute the initial guess. For a camera with no distortion, this will also
  // be the final answer; the LM iteration will terminate immediately.
  Vec2 normalized;
  normalized(0) = (image_x - principal_point_x) / focal_length_x;
  normalized(1) = (image_y - principal_point_y) / focal_length_y;

  // TODO(sergey): Use Ceres minimizer instead.
  typedef LevenbergMarquardt<InvertDivisionIntrinsicsCostFunction> Solver;

  InvertDivisionIntrinsicsCostFunction intrinsics_cost(focal_length_x,
                                                       focal_length_y,
                                                       principal_point_x,
                                                       principal_point_y,
                                                       k1,
                                                       k2,
                                                       image_x,
                                                       image_y);
  Solver::SolverParameters params;
  Solver solver(intrinsics_cost);

  /*Solver::Results results =*/solver.minimize(params, &normalized);

  // TODO(keir): Better error handling.

  *normalized_x = normalized(0);
  *normalized_y = normalized(1);
}

void InvertBrownDistortionModel(const double focal_length_x,
                                const double focal_length_y,
                                const double principal_point_x,
                                const double principal_point_y,
                                const double k1,
                                const double k2,
                                const double k3,
                                const double k4,
                                const double p1,
                                const double p2,
                                const double image_x,
                                const double image_y,
                                double* normalized_x,
                                double* normalized_y) {
  // Compute the initial guess. For a camera with no distortion, this will also
  // be the final answer; the LM iteration will terminate immediately.
  Vec2 normalized;
  normalized(0) = (image_x - principal_point_x) / focal_length_x;
  normalized(1) = (image_y - principal_point_y) / focal_length_y;

  typedef LevenbergMarquardt<InvertBrownIntrinsicsCostFunction> Solver;

  InvertBrownIntrinsicsCostFunction intrinsics_cost(focal_length_x,
                                                    focal_length_y,
                                                    principal_point_x,
                                                    principal_point_y,
                                                    k1,
                                                    k2,
                                                    k3,
                                                    k4,
                                                    p1,
                                                    p2,
                                                    image_x,
                                                    image_y);
  Solver::SolverParameters params;
  Solver solver(intrinsics_cost);

  /*Solver::Results results =*/solver.minimize(params, &normalized);

  // TODO(keir): Better error handling.

  *normalized_x = normalized(0);
  *normalized_y = normalized(1);
}

struct ApplyNukeIntrinsicsCostFunction {
 public:
  typedef Vec2 FMatrixType;
  typedef Vec2 XMatrixType;

  ApplyNukeIntrinsicsCostFunction(const double focal_length_x,
                                  const double focal_length_y,
                                  const double principal_point_x,
                                  const double principal_point_y,
                                  const int image_width,
                                  const int image_height,
                                  const double k1,
                                  const double k2,
                                  const double p1,
                                  const double p2,
                                  const double expected_normalized_x,
                                  const double expected_normalized_y)
      : focal_length_x_(focal_length_x),
        focal_length_y_(focal_length_y),
        principal_point_x_(principal_point_x),
        principal_point_y_(principal_point_y),
        image_width_(image_width),
        image_height_(image_height),
        k1_(k1),
        k2_(k2),
        p1_(p1),
        p2_(p2),
        expected_normalized_x_(expected_normalized_x),
        expected_normalized_y_(expected_normalized_y) {}

  Vec2 operator()(const Vec2& image_coordinate) const {
    double actual_normalized_x, actual_normalized_y;

    InvertNukeDistortionModel(focal_length_x_,
                              focal_length_y_,
                              principal_point_x_,
                              principal_point_y_,
                              image_width_,
                              image_height_,
                              k1_,
                              k2_,
                              p1_,
                              p2_,
                              image_coordinate(0),
                              image_coordinate(1),
                              &actual_normalized_x,
                              &actual_normalized_y);

    Vec2 fx;
    fx << (actual_normalized_x - expected_normalized_x_),
        (actual_normalized_y - expected_normalized_y_);
    return fx;
  }
  double focal_length_x_;
  double focal_length_y_;
  double principal_point_x_;
  double principal_point_y_;
  int image_width_;
  int image_height_;
  double k1_, k2_, p1_, p2_;
  double expected_normalized_x_, expected_normalized_y_;
};

void ApplyNukeDistortionModel(const double focal_length_x,
                              const double focal_length_y,
                              const double principal_point_x,
                              const double principal_point_y,
                              const int image_width,
                              const int image_height,
                              const double k1,
                              const double k2,
                              const double p1,
                              const double p2,
                              const double normalized_x,
                              const double normalized_y,
                              double* image_x,
                              double* image_y) {
  // Compute the initial guess. For a camera with no distortion, this will also
  // be the final answer; the LM iteration will terminate immediately.
  Vec2 image;
  image(0) = normalized_x * focal_length_x + principal_point_x;
  image(1) = normalized_y * focal_length_y + principal_point_y;

  // TODO(sergey): Use Ceres minimizer instead.
  typedef LevenbergMarquardt<ApplyNukeIntrinsicsCostFunction> Solver;

  ApplyNukeIntrinsicsCostFunction intrinsics_cost(focal_length_x,
                                                  focal_length_y,
                                                  principal_point_x,
                                                  principal_point_y,
                                                  image_width,
                                                  image_height,
                                                  k1,
                                                  k2,
                                                  p1,
                                                  p2,
                                                  normalized_x,
                                                  normalized_y);
  Solver::SolverParameters params;
  Solver solver(intrinsics_cost);

  /*Solver::Results results =*/solver.minimize(params, &image);

  // TODO(keir): Better error handling.

  *image_x = image(0);
  *image_y = image(1);
}

}  // namespace libmv
