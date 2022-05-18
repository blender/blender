#include "ceres/internal/export.h"
#include "ceres/local_parameterization.h"
#include "ceres/manifold.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// Adapter to wrap LocalParameterization and make them look like Manifolds.
//
// ManifoldAdapter NEVER takes ownership of local_parameterization.
class CERES_NO_EXPORT ManifoldAdapter final : public Manifold {
 public:
  explicit ManifoldAdapter(const LocalParameterization* local_parameterization)
      : local_parameterization_(local_parameterization) {
    CHECK(local_parameterization != nullptr);
  }

  bool Plus(const double* x,
            const double* delta,
            double* x_plus_delta) const override {
    return local_parameterization_->Plus(x, delta, x_plus_delta);
  }

  bool PlusJacobian(const double* x, double* jacobian) const override {
    return local_parameterization_->ComputeJacobian(x, jacobian);
  }

  bool RightMultiplyByPlusJacobian(const double* x,
                                   const int num_rows,
                                   const double* ambient_matrix,
                                   double* tangent_matrix) const override {
    return local_parameterization_->MultiplyByJacobian(
        x, num_rows, ambient_matrix, tangent_matrix);
  }

  bool Minus(const double* y, const double* x, double* delta) const override {
    LOG(FATAL) << "This should never be called.";
    return false;
  }

  bool MinusJacobian(const double* x, double* jacobian) const override {
    LOG(FATAL) << "This should never be called.";
    return false;
  }

  int AmbientSize() const override {
    return local_parameterization_->GlobalSize();
  }

  int TangentSize() const override {
    return local_parameterization_->LocalSize();
  }

 private:
  const LocalParameterization* local_parameterization_;
};

}  // namespace internal
}  // namespace ceres
