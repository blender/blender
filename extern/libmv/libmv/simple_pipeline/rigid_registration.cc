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

#include "libmv/simple_pipeline/rigid_registration.h"
#include "libmv/numeric/levenberg_marquardt.h"

namespace libmv {

template<class RigidTransformation>
struct RigidRegistrationCostFunction {
 public:
  typedef Vec FMatrixType;
  typedef RigidTransformation XMatrixType;

  RigidRegistrationCostFunction(const vector<Vec3> &reference_points,
                                const vector<Vec3> &points):
    reference_points_(reference_points),
    points_(points) {}

  Vec CalculateResiduals(const Mat3 &R,
                         const Vec3 &S,
                         const Vec3 &t) const {
    Vec residuals(points_.size());
    residuals.setZero();

    // Convert scale vector to matrix
    Mat3 SMat = Mat3::Identity();
    SMat(0, 0) *= S(0);
    SMat(1, 1) *= S(1);
    SMat(2, 2) *= S(2);

    for (int i = 0; i < points_.size(); i++) {
      Vec3 transformed_point = R * SMat * points_[i] + t;

      double norm = (transformed_point - reference_points_[i]).norm();

      residuals(i) = norm * norm;
    }

    return residuals;
  }

  Vec operator()(const Vec9 &RSt) const {
    Mat3 R = RotationFromEulerVector(RSt.head<3>());
    Vec3 S = RSt.segment<3>(3);
    Vec3 t = RSt.tail<3>();

    return CalculateResiduals(R, S, t);
  }

  Vec operator()(const Vec3 &euler) const {
    Mat3 R = RotationFromEulerVector(euler);

    return CalculateResiduals(R, Vec3(1.0, 1.0, 1.0), Vec3::Zero());
  }

  Vec operator()(const Vec6 &Rt) const {
    Mat3 R = RotationFromEulerVector(Rt.head<3>());
    Vec3 t = Rt.tail<3>();

    return CalculateResiduals(R, Vec3(1.0, 1.0, 1.0), t);
  }

 private:
  vector<Vec3> reference_points_;
  vector<Vec3> points_;
};

static double RigidRegistrationError(const vector<Vec3> &reference_points,
                                     const vector<Vec3> &points,
                                     const Mat3 &R,
                                     const Vec3 &S,
                                     const Vec3 &t) {
  double error = 0.0;

  Mat3 SMat = Mat3::Identity();
  SMat(0, 0) *= S(0);
  SMat(1, 1) *= S(1);
  SMat(2, 2) *= S(2);

  for (int i = 0; i < points.size(); i++) {
    Vec3 new_point = R * SMat * points[i] + t;

    double norm = (new_point - reference_points[i]).norm();
    error += norm * norm;
  }
  error /= points.size();

  return error;
}

double RigidRegistration(const vector<Vec3> &reference_points,
                         const vector<Vec3> &points,
                         Mat3 &R,
                         Vec3 &S,
                         Vec3 &t) {
  typedef LevenbergMarquardt<RigidRegistrationCostFunction <Vec9> > Solver;

  RigidRegistrationCostFunction<Vec9> rigidregistration_cost(reference_points, points);
  Solver solver(rigidregistration_cost);

  Vec9 RSt = Vec9::Zero();

  RSt(3) = RSt(4) = RSt(5) = 1.0;

  Solver::SolverParameters params;
  /*Solver::Results results = */ solver.minimize(params, &RSt);
  /* TODO(sergey): better error handling here */

  LG << "Rigid registration completed, rotation is:" << RSt.head<3>().transpose()
    << ", scale is " << RSt.segment<3>(3).transpose()
    << ", translation is " << RSt.tail<3>().transpose();

  // Decompose individual rotation and translation
  R = RotationFromEulerVector(RSt.head<3>());
  S = RSt.segment<3>(3);
  t = RSt.tail<3>();

  return RigidRegistrationError(reference_points, points, R, S, t);
}

double RigidRegistration(const vector<Vec3> &reference_points,
                         const vector<Vec3> &points,
                         Mat3 &R,
                         Vec3 &t) {
  typedef LevenbergMarquardt<RigidRegistrationCostFunction <Vec6> > Solver;

  RigidRegistrationCostFunction<Vec6> rigidregistration_cost(reference_points, points);
  Solver solver(rigidregistration_cost);

  Vec6 Rt = Vec6::Zero();
  Solver::SolverParameters params;
  /*Solver::Results results = */solver.minimize(params, &Rt);
  /* TODO(sergey): better error handling here */

  LG << "Rigid registration completed, rotation is:" << Rt.head<3>().transpose()
    << ", translation is " << Rt.tail<3>().transpose();

  R = RotationFromEulerVector(Rt.head<3>());
  t = Rt.tail<3>();

  return RigidRegistrationError(reference_points, points, R, Vec3(1.0, 1.0, 1.0), t);
}

double RigidRegistration(const vector<Vec3> &reference_points,
                         const vector<Vec3> &points,
                         Mat3 &R) {
  typedef LevenbergMarquardt<RigidRegistrationCostFunction <Vec3> > Solver;

  RigidRegistrationCostFunction<Vec3> rigidregistration_cost(reference_points, points);
  Solver solver(rigidregistration_cost);

  Vec3 euler = Vec3::Zero();
  Solver::SolverParameters params;
  /*Solver::Results results = */solver.minimize(params, &euler);
  /* TODO(sergey): better error handling here */

  LG << "Rigid registration completed, rotation is:" << euler.transpose();

  R = RotationFromEulerVector(euler);

  return RigidRegistrationError(reference_points, points, R, Vec3(1.0, 1.0, 1.0), Vec3::Zero());
}

}  // namespace libmv
