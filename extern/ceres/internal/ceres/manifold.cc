#include "ceres/manifold.h"

#include <algorithm>
#include <cmath>

#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "glog/logging.h"

namespace ceres {
namespace {

struct CeresQuaternionOrder {
  static constexpr int kW = 0;
  static constexpr int kX = 1;
  static constexpr int kY = 2;
  static constexpr int kZ = 3;
};

struct EigenQuaternionOrder {
  static constexpr int kW = 3;
  static constexpr int kX = 0;
  static constexpr int kY = 1;
  static constexpr int kZ = 2;
};

template <typename Order>
inline void QuaternionPlusImpl(const double* x,
                               const double* delta,
                               double* x_plus_delta) {
  // x_plus_delta = QuaternionProduct(q_delta, x), where q_delta is the
  // quaternion constructed from delta.
  const double norm_delta = std::sqrt(
      delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);

  if (norm_delta == 0.0) {
    for (int i = 0; i < 4; ++i) {
      x_plus_delta[i] = x[i];
    }
    return;
  }

  const double sin_delta_by_delta = (std::sin(norm_delta) / norm_delta);
  double q_delta[4];
  q_delta[Order::kW] = std::cos(norm_delta);
  q_delta[Order::kX] = sin_delta_by_delta * delta[0];
  q_delta[Order::kY] = sin_delta_by_delta * delta[1];
  q_delta[Order::kZ] = sin_delta_by_delta * delta[2];

  x_plus_delta[Order::kW] =
      q_delta[Order::kW] * x[Order::kW] - q_delta[Order::kX] * x[Order::kX] -
      q_delta[Order::kY] * x[Order::kY] - q_delta[Order::kZ] * x[Order::kZ];
  x_plus_delta[Order::kX] =
      q_delta[Order::kW] * x[Order::kX] + q_delta[Order::kX] * x[Order::kW] +
      q_delta[Order::kY] * x[Order::kZ] - q_delta[Order::kZ] * x[Order::kY];
  x_plus_delta[Order::kY] =
      q_delta[Order::kW] * x[Order::kY] - q_delta[Order::kX] * x[Order::kZ] +
      q_delta[Order::kY] * x[Order::kW] + q_delta[Order::kZ] * x[Order::kX];
  x_plus_delta[Order::kZ] =
      q_delta[Order::kW] * x[Order::kZ] + q_delta[Order::kX] * x[Order::kY] -
      q_delta[Order::kY] * x[Order::kX] + q_delta[Order::kZ] * x[Order::kW];
}

template <typename Order>
inline void QuaternionPlusJacobianImpl(const double* x, double* jacobian_ptr) {
  Eigen::Map<Eigen::Matrix<double, 4, 3, Eigen::RowMajor>> jacobian(
      jacobian_ptr);

  jacobian(Order::kW, 0) = -x[Order::kX];
  jacobian(Order::kW, 1) = -x[Order::kY];
  jacobian(Order::kW, 2) = -x[Order::kZ];
  jacobian(Order::kX, 0) = x[Order::kW];
  jacobian(Order::kX, 1) = x[Order::kZ];
  jacobian(Order::kX, 2) = -x[Order::kY];
  jacobian(Order::kY, 0) = -x[Order::kZ];
  jacobian(Order::kY, 1) = x[Order::kW];
  jacobian(Order::kY, 2) = x[Order::kX];
  jacobian(Order::kZ, 0) = x[Order::kY];
  jacobian(Order::kZ, 1) = -x[Order::kX];
  jacobian(Order::kZ, 2) = x[Order::kW];
}

template <typename Order>
inline void QuaternionMinusImpl(const double* y,
                                const double* x,
                                double* y_minus_x) {
  // ambient_y_minus_x = QuaternionProduct(y, -x) where -x is the conjugate of
  // x.
  double ambient_y_minus_x[4];
  ambient_y_minus_x[Order::kW] =
      y[Order::kW] * x[Order::kW] + y[Order::kX] * x[Order::kX] +
      y[Order::kY] * x[Order::kY] + y[Order::kZ] * x[Order::kZ];
  ambient_y_minus_x[Order::kX] =
      -y[Order::kW] * x[Order::kX] + y[Order::kX] * x[Order::kW] -
      y[Order::kY] * x[Order::kZ] + y[Order::kZ] * x[Order::kY];
  ambient_y_minus_x[Order::kY] =
      -y[Order::kW] * x[Order::kY] + y[Order::kX] * x[Order::kZ] +
      y[Order::kY] * x[Order::kW] - y[Order::kZ] * x[Order::kX];
  ambient_y_minus_x[Order::kZ] =
      -y[Order::kW] * x[Order::kZ] - y[Order::kX] * x[Order::kY] +
      y[Order::kY] * x[Order::kX] + y[Order::kZ] * x[Order::kW];

  const double u_norm =
      std::sqrt(ambient_y_minus_x[Order::kX] * ambient_y_minus_x[Order::kX] +
                ambient_y_minus_x[Order::kY] * ambient_y_minus_x[Order::kY] +
                ambient_y_minus_x[Order::kZ] * ambient_y_minus_x[Order::kZ]);
  if (u_norm > 0.0) {
    const double theta = std::atan2(u_norm, ambient_y_minus_x[Order::kW]);
    y_minus_x[0] = theta * ambient_y_minus_x[Order::kX] / u_norm;
    y_minus_x[1] = theta * ambient_y_minus_x[Order::kY] / u_norm;
    y_minus_x[2] = theta * ambient_y_minus_x[Order::kZ] / u_norm;
  } else {
    y_minus_x[0] = 0.0;
    y_minus_x[1] = 0.0;
    y_minus_x[2] = 0.0;
  }
}

template <typename Order>
inline void QuaternionMinusJacobianImpl(const double* x, double* jacobian_ptr) {
  Eigen::Map<Eigen::Matrix<double, 3, 4, Eigen::RowMajor>> jacobian(
      jacobian_ptr);

  jacobian(0, Order::kW) = -x[Order::kX];
  jacobian(0, Order::kX) = x[Order::kW];
  jacobian(0, Order::kY) = -x[Order::kZ];
  jacobian(0, Order::kZ) = x[Order::kY];
  jacobian(1, Order::kW) = -x[Order::kY];
  jacobian(1, Order::kX) = x[Order::kZ];
  jacobian(1, Order::kY) = x[Order::kW];
  jacobian(1, Order::kZ) = -x[Order::kX];
  jacobian(2, Order::kW) = -x[Order::kZ];
  jacobian(2, Order::kX) = -x[Order::kY];
  jacobian(2, Order::kY) = x[Order::kX];
  jacobian(2, Order::kZ) = x[Order::kW];
}

}  // namespace

Manifold::~Manifold() = default;

bool Manifold::RightMultiplyByPlusJacobian(const double* x,
                                           const int num_rows,
                                           const double* ambient_matrix,
                                           double* tangent_matrix) const {
  const int tangent_size = TangentSize();
  if (tangent_size == 0) {
    return true;
  }

  const int ambient_size = AmbientSize();
  Matrix plus_jacobian(ambient_size, tangent_size);
  if (!PlusJacobian(x, plus_jacobian.data())) {
    return false;
  }

  MatrixRef(tangent_matrix, num_rows, tangent_size) =
      ConstMatrixRef(ambient_matrix, num_rows, ambient_size) * plus_jacobian;
  return true;
}

SubsetManifold::SubsetManifold(const int size,
                               const std::vector<int>& constant_parameters)

    : tangent_size_(size - constant_parameters.size()),
      constancy_mask_(size, false) {
  if (constant_parameters.empty()) {
    return;
  }

  std::vector<int> constant = constant_parameters;
  std::sort(constant.begin(), constant.end());
  CHECK_GE(constant.front(), 0) << "Indices indicating constant parameter must "
                                   "be greater than equal to zero.";
  CHECK_LT(constant.back(), size)
      << "Indices indicating constant parameter must be less than the size "
      << "of the parameter block.";
  CHECK(std::adjacent_find(constant.begin(), constant.end()) == constant.end())
      << "The set of constant parameters cannot contain duplicates";

  for (auto index : constant_parameters) {
    constancy_mask_[index] = true;
  }
}

int SubsetManifold::AmbientSize() const { return constancy_mask_.size(); }

int SubsetManifold::TangentSize() const { return tangent_size_; }

bool SubsetManifold::Plus(const double* x,
                          const double* delta,
                          double* x_plus_delta) const {
  const int ambient_size = AmbientSize();
  for (int i = 0, j = 0; i < ambient_size; ++i) {
    if (constancy_mask_[i]) {
      x_plus_delta[i] = x[i];
    } else {
      x_plus_delta[i] = x[i] + delta[j++];
    }
  }
  return true;
}

bool SubsetManifold::PlusJacobian(const double* x,
                                  double* plus_jacobian) const {
  if (tangent_size_ == 0) {
    return true;
  }

  const int ambient_size = AmbientSize();
  MatrixRef m(plus_jacobian, ambient_size, tangent_size_);
  m.setZero();
  for (int r = 0, c = 0; r < ambient_size; ++r) {
    if (!constancy_mask_[r]) {
      m(r, c++) = 1.0;
    }
  }
  return true;
}

bool SubsetManifold::RightMultiplyByPlusJacobian(const double* x,
                                                 const int num_rows,
                                                 const double* ambient_matrix,
                                                 double* tangent_matrix) const {
  if (tangent_size_ == 0) {
    return true;
  }

  const int ambient_size = AmbientSize();
  for (int r = 0; r < num_rows; ++r) {
    for (int idx = 0, c = 0; idx < ambient_size; ++idx) {
      if (!constancy_mask_[idx]) {
        tangent_matrix[r * tangent_size_ + c++] =
            ambient_matrix[r * ambient_size + idx];
      }
    }
  }
  return true;
}

bool SubsetManifold::Minus(const double* y,
                           const double* x,
                           double* y_minus_x) const {
  if (tangent_size_ == 0) {
    return true;
  }

  const int ambient_size = AmbientSize();
  for (int i = 0, j = 0; i < ambient_size; ++i) {
    if (!constancy_mask_[i]) {
      y_minus_x[j++] = y[i] - x[i];
    }
  }
  return true;
}

bool SubsetManifold::MinusJacobian(const double* x,
                                   double* minus_jacobian) const {
  const int ambient_size = AmbientSize();
  MatrixRef m(minus_jacobian, tangent_size_, ambient_size);
  m.setZero();
  for (int c = 0, r = 0; c < ambient_size; ++c) {
    if (!constancy_mask_[c]) {
      m(r++, c) = 1.0;
    }
  }
  return true;
}

bool QuaternionManifold::Plus(const double* x,
                              const double* delta,
                              double* x_plus_delta) const {
  QuaternionPlusImpl<CeresQuaternionOrder>(x, delta, x_plus_delta);
  return true;
}

bool QuaternionManifold::PlusJacobian(const double* x, double* jacobian) const {
  QuaternionPlusJacobianImpl<CeresQuaternionOrder>(x, jacobian);
  return true;
}

bool QuaternionManifold::Minus(const double* y,
                               const double* x,
                               double* y_minus_x) const {
  QuaternionMinusImpl<CeresQuaternionOrder>(y, x, y_minus_x);
  return true;
}

bool QuaternionManifold::MinusJacobian(const double* x,
                                       double* jacobian) const {
  QuaternionMinusJacobianImpl<CeresQuaternionOrder>(x, jacobian);
  return true;
}

bool EigenQuaternionManifold::Plus(const double* x,
                                   const double* delta,
                                   double* x_plus_delta) const {
  QuaternionPlusImpl<EigenQuaternionOrder>(x, delta, x_plus_delta);
  return true;
}

bool EigenQuaternionManifold::PlusJacobian(const double* x,
                                           double* jacobian) const {
  QuaternionPlusJacobianImpl<EigenQuaternionOrder>(x, jacobian);
  return true;
}

bool EigenQuaternionManifold::Minus(const double* y,
                                    const double* x,
                                    double* y_minus_x) const {
  QuaternionMinusImpl<EigenQuaternionOrder>(y, x, y_minus_x);
  return true;
}

bool EigenQuaternionManifold::MinusJacobian(const double* x,
                                            double* jacobian) const {
  QuaternionMinusJacobianImpl<EigenQuaternionOrder>(x, jacobian);
  return true;
}

}  // namespace ceres
