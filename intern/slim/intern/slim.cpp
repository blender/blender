/* SPDX-FileCopyrightText: 2016 Michael Rabinovich
 *                         2023 Blender Authors
 *
 * SPDX-License-Identifier: MPL-2.0 */

/** \file
 * \ingroup intern_slim
 */

#include "slim.h"
#include "doublearea.h"
#include "flip_avoiding_line_search.h"

#include "BLI_assert.h"
#include "BLI_math_base.h" /* M_PI */

#include <vector>

#include <Eigen/Geometry>
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/SVD>
#include <Eigen/SparseCholesky>

namespace slim {

/* GRAD
 * G = grad(V,F)
 *
 * Compute the numerical gradient operator
 *
 * Inputs:
 *   V          #vertices by 3 list of mesh vertex positions
 *   F          #faces by 3 list of mesh face indices
 *   uniform    #boolean (default false) - Use a uniform mesh instead of the vertices V
 * Outputs:
 *   G  #faces*dim by #V Gradient operator
 *
 *
 * Gradient of a scalar function defined on piecewise linear elements (mesh)
 * is constant on each triangle i,j,k:
 * grad(Xijk) = (Xj-Xi) * (Vi - Vk)^R90 / 2A + (Xk-Xi) * (Vj - Vi)^R90 / 2A
 * where Xi is the scalar value at vertex i, Vi is the 3D position of vertex
 * i, and A is the area of triangle (i,j,k). ^R90 represent a rotation of
 * 90 degrees
 */
template<typename DerivedV, typename DerivedF>
static inline void grad(const Eigen::PlainObjectBase<DerivedV> &V,
                        const Eigen::PlainObjectBase<DerivedF> &F,
                        Eigen::SparseMatrix<typename DerivedV::Scalar> &G,
                        bool uniform = false)
{
  Eigen::Matrix<typename DerivedV::Scalar, Eigen::Dynamic, 3> eperp21(F.rows(), 3),
      eperp13(F.rows(), 3);

  for (int i = 0; i < F.rows(); ++i) {
    /* Renaming indices of vertices of triangles for convenience. */
    int i1 = F(i, 0);
    int i2 = F(i, 1);
    int i3 = F(i, 2);

    /* #F x 3 matrices of triangle edge vectors, named after opposite vertices. */
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> v32 = V.row(i3) - V.row(i2);
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> v13 = V.row(i1) - V.row(i3);
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> v21 = V.row(i2) - V.row(i1);
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> n = v32.cross(v13);
    /* Area of parallelogram is twice area of triangle.
     * Area of parallelogram is || v1 x v2 ||.
     * This does correct l2 norm of rows, so that it contains #F list of twice.
     * triangle areas. */
    double dblA = std::sqrt(n.dot(n));
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> u;
    if (!uniform) {
      /* Now normalize normals to get unit normals. */
      u = n / dblA;
    }
    else {
      /* Abstract equilateral triangle v1=(0,0), v2=(h,0), v3=(h/2, (sqrt(3)/2)*h) */

      /* Get h (by the area of the triangle). */
      double h = sqrt((dblA) /
                      sin(M_PI / 3.0)); /* (h^2*sin(60))/2. = Area => h = sqrt(2*Area/sin_60) */

      Eigen::VectorXd v1, v2, v3;
      v1 << 0, 0, 0;
      v2 << h, 0, 0;
      v3 << h / 2., (sqrt(3) / 2.) * h, 0;

      /* Now fix v32,v13,v21 and the normal. */
      v32 = v3 - v2;
      v13 = v1 - v3;
      v21 = v2 - v1;
      n = v32.cross(v13);
    }

    /* Rotate each vector 90 degrees around normal. */
    double norm21 = std::sqrt(v21.dot(v21));
    double norm13 = std::sqrt(v13.dot(v13));
    eperp21.row(i) = u.cross(v21);
    eperp21.row(i) = eperp21.row(i) / std::sqrt(eperp21.row(i).dot(eperp21.row(i)));
    eperp21.row(i) *= norm21 / dblA;
    eperp13.row(i) = u.cross(v13);
    eperp13.row(i) = eperp13.row(i) / std::sqrt(eperp13.row(i).dot(eperp13.row(i)));
    eperp13.row(i) *= norm13 / dblA;
  }

  std::vector<int> rs;
  rs.reserve(F.rows() * 4 * 3);
  std::vector<int> cs;
  cs.reserve(F.rows() * 4 * 3);
  std::vector<double> vs;
  vs.reserve(F.rows() * 4 * 3);

  /* Row indices. */
  for (int r = 0; r < 3; r++) {
    for (int j = 0; j < 4; j++) {
      for (int i = r * F.rows(); i < (r + 1) * F.rows(); i++) {
        rs.push_back(i);
      }
    }
  }

  /* Column indices. */
  for (int r = 0; r < 3; r++) {
    for (int i = 0; i < F.rows(); i++) {
      cs.push_back(F(i, 1));
    }
    for (int i = 0; i < F.rows(); i++) {
      cs.push_back(F(i, 0));
    }
    for (int i = 0; i < F.rows(); i++) {
      cs.push_back(F(i, 2));
    }
    for (int i = 0; i < F.rows(); i++) {
      cs.push_back(F(i, 0));
    }
  }

  /* Values. */
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(eperp13(i, 0));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(-eperp13(i, 0));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(eperp21(i, 0));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(-eperp21(i, 0));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(eperp13(i, 1));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(-eperp13(i, 1));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(eperp21(i, 1));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(-eperp21(i, 1));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(eperp13(i, 2));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(-eperp13(i, 2));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(eperp21(i, 2));
  }
  for (int i = 0; i < F.rows(); i++) {
    vs.push_back(-eperp21(i, 2));
  }

  /* Create sparse gradient operator matrix.. */
  G.resize(3 * F.rows(), V.rows());
  std::vector<Eigen::Triplet<typename DerivedV::Scalar>> triplets;
  for (int i = 0; i < (int)vs.size(); ++i) {
    triplets.push_back(Eigen::Triplet<typename DerivedV::Scalar>(rs[i], cs[i], vs[i]));
  }
  G.setFromTriplets(triplets.begin(), triplets.end());
}

/* Computes the polar decomposition (R,T) of a matrix A using SVD singular
 * value decomposition
 *
 * Inputs:
 *   A  3 by 3 matrix to be decomposed
 * Outputs:
 *   R  3 by 3 rotation matrix part of decomposition (**always rotataion**)
 *   T  3 by 3 stretch matrix part of decomposition
 *   U  3 by 3 left-singular vectors
 *   S  3 by 1 singular values
 *   V  3 by 3 right-singular vectors
 */
template<typename DerivedA,
         typename DerivedR,
         typename DerivedT,
         typename DerivedU,
         typename DerivedS,
         typename DerivedV>
static inline void polar_svd(const Eigen::PlainObjectBase<DerivedA> &A,
                             Eigen::PlainObjectBase<DerivedR> &R,
                             Eigen::PlainObjectBase<DerivedT> &T,
                             Eigen::PlainObjectBase<DerivedU> &U,
                             Eigen::PlainObjectBase<DerivedS> &S,
                             Eigen::PlainObjectBase<DerivedV> &V)
{
  using namespace std;
  Eigen::JacobiSVD<DerivedA> svd;
  svd.compute(A, Eigen::ComputeFullU | Eigen::ComputeFullV);
  U = svd.matrixU();
  V = svd.matrixV();
  S = svd.singularValues();
  R = U * V.transpose();
  const auto &SVT = S.asDiagonal() * V.adjoint();
  /* Check for reflection. */
  if (R.determinant() < 0) {
    /* Annoyingly the .eval() is necessary. */
    auto W = V.eval();
    W.col(V.cols() - 1) *= -1.;
    R = U * W.transpose();
    T = W * SVT;
  }
  else {
    T = V * SVT;
  }
}

static inline void compute_surface_gradient_matrix(const Eigen::MatrixXd &V,
                                                   const Eigen::MatrixXi &F,
                                                   const Eigen::MatrixXd &F1,
                                                   const Eigen::MatrixXd &F2,
                                                   Eigen::SparseMatrix<double> &D1,
                                                   Eigen::SparseMatrix<double> &D2)
{
  Eigen::SparseMatrix<double> G;
  grad(V, F, G);
  Eigen::SparseMatrix<double> Dx = G.block(0, 0, F.rows(), V.rows());
  Eigen::SparseMatrix<double> Dy = G.block(F.rows(), 0, F.rows(), V.rows());
  Eigen::SparseMatrix<double> Dz = G.block(2 * F.rows(), 0, F.rows(), V.rows());

  D1 = F1.col(0).asDiagonal() * Dx + F1.col(1).asDiagonal() * Dy + F1.col(2).asDiagonal() * Dz;
  D2 = F2.col(0).asDiagonal() * Dx + F2.col(1).asDiagonal() * Dy + F2.col(2).asDiagonal() * Dz;
}

static inline void compute_weighted_jacobians(SLIMData &s, const Eigen::MatrixXd &uv)
{
  BLI_assert(s.valid);

  /* Ji=[D1*u,D2*u,D1*v,D2*v] */
  s.Ji.col(0) = s.Dx * uv.col(0);
  s.Ji.col(1) = s.Dy * uv.col(0);
  s.Ji.col(2) = s.Dx * uv.col(1);
  s.Ji.col(3) = s.Dy * uv.col(1);

  /* Add weights. */
  Eigen::VectorXd weights = s.weightPerFaceMap.cast<double>();
  s.Ji.col(0) = weights.cwiseProduct(s.Ji.col(0));
  s.Ji.col(1) = weights.cwiseProduct(s.Ji.col(1));
  s.Ji.col(2) = weights.cwiseProduct(s.Ji.col(2));
  s.Ji.col(3) = weights.cwiseProduct(s.Ji.col(3));
}

static inline void compute_unweighted_jacobians(SLIMData &s, const Eigen::MatrixXd &uv)
{
  BLI_assert(s.valid);

  /* Ji=[D1*u,D2*u,D1*v,D2*v] */
  s.Ji.col(0) = s.Dx * uv.col(0);
  s.Ji.col(1) = s.Dy * uv.col(0);
  s.Ji.col(2) = s.Dx * uv.col(1);
  s.Ji.col(3) = s.Dy * uv.col(1);
}

static inline void compute_jacobians(SLIMData &s, const Eigen::MatrixXd &uv)
{
  BLI_assert(s.valid);

  if (s.withWeightedParameterization) {
    compute_weighted_jacobians(s, uv);
  }
  else {
    compute_unweighted_jacobians(s, uv);
  }
}

static inline void update_weights_and_closest_rotations(SLIMData &s, Eigen::MatrixXd &uv)
{
  BLI_assert(s.valid);
  compute_jacobians(s, uv);

  const double eps = 1e-8;
  double exp_f = s.exp_factor;

  for (int i = 0; i < s.Ji.rows(); ++i) {
    using Mat2 = Eigen::Matrix<double, 2, 2>;
    using Vec2 = Eigen::Matrix<double, 2, 1>;
    Mat2 ji, ri, ti, ui, vi;
    Vec2 sing;
    Vec2 closest_sing_vec;
    Mat2 mat_W;
    Vec2 m_sing_new;
    double s1, s2;

    ji(0, 0) = s.Ji(i, 0);
    ji(0, 1) = s.Ji(i, 1);
    ji(1, 0) = s.Ji(i, 2);
    ji(1, 1) = s.Ji(i, 3);

    polar_svd(ji, ri, ti, ui, sing, vi);

    s1 = sing(0);
    s2 = sing(1);

    /* Update Weights according to energy. */
    switch (s.slim_energy) {
      case SLIMData::ARAP: {
        m_sing_new << 1, 1;
        break;
      }
      case SLIMData::SYMMETRIC_DIRICHLET: {
        double s1_g = 2 * (s1 - pow(s1, -3));
        double s2_g = 2 * (s2 - pow(s2, -3));
        m_sing_new << sqrt(s1_g / (2 * (s1 - 1))), sqrt(s2_g / (2 * (s2 - 1)));
        break;
      }
      case SLIMData::LOG_ARAP: {
        double s1_g = 2 * (log(s1) / s1);
        double s2_g = 2 * (log(s2) / s2);
        m_sing_new << sqrt(s1_g / (2 * (s1 - 1))), sqrt(s2_g / (2 * (s2 - 1)));
        break;
      }
      case SLIMData::CONFORMAL: {
        double s1_g = 1 / (2 * s2) - s2 / (2 * pow(s1, 2));
        double s2_g = 1 / (2 * s1) - s1 / (2 * pow(s2, 2));

        double geo_avg = sqrt(s1 * s2);
        double s1_min = geo_avg;
        double s2_min = geo_avg;

        m_sing_new << sqrt(s1_g / (2 * (s1 - s1_min))), sqrt(s2_g / (2 * (s2 - s2_min)));

        /* Change local step. */
        closest_sing_vec << s1_min, s2_min;
        ri = ui * closest_sing_vec.asDiagonal() * vi.transpose();
        break;
      }
      case SLIMData::EXP_CONFORMAL: {
        double s1_g = 2 * (s1 - pow(s1, -3));
        double s2_g = 2 * (s2 - pow(s2, -3));

        double in_exp = exp_f * ((pow(s1, 2) + pow(s2, 2)) / (2 * s1 * s2));
        double exp_thing = exp(in_exp);

        s1_g *= exp_thing * exp_f;
        s2_g *= exp_thing * exp_f;

        m_sing_new << sqrt(s1_g / (2 * (s1 - 1))), sqrt(s2_g / (2 * (s2 - 1)));
        break;
      }
      case SLIMData::EXP_SYMMETRIC_DIRICHLET: {
        double s1_g = 2 * (s1 - pow(s1, -3));
        double s2_g = 2 * (s2 - pow(s2, -3));

        double in_exp = exp_f * (pow(s1, 2) + pow(s1, -2) + pow(s2, 2) + pow(s2, -2));
        double exp_thing = exp(in_exp);

        s1_g *= exp_thing * exp_f;
        s2_g *= exp_thing * exp_f;

        m_sing_new << sqrt(s1_g / (2 * (s1 - 1))), sqrt(s2_g / (2 * (s2 - 1)));
        break;
      }
    }

    if (std::abs(s1 - 1) < eps) {
      m_sing_new(0) = 1;
    }
    if (std::abs(s2 - 1) < eps) {
      m_sing_new(1) = 1;
    }
    mat_W = ui * m_sing_new.asDiagonal() * ui.transpose();

    s.W_11(i) = mat_W(0, 0);
    s.W_12(i) = mat_W(0, 1);
    s.W_21(i) = mat_W(1, 0);
    s.W_22(i) = mat_W(1, 1);

    /* 2) Update local step (doesn't have to be a rotation, for instance in case of conformal
     * energy). */
    s.Ri(i, 0) = ri(0, 0);
    s.Ri(i, 1) = ri(1, 0);
    s.Ri(i, 2) = ri(0, 1);
    s.Ri(i, 3) = ri(1, 1);
  }
}

template<typename DerivedV, typename DerivedF>
static inline void local_basis(const Eigen::PlainObjectBase<DerivedV> &V,
                               const Eigen::PlainObjectBase<DerivedF> &F,
                               Eigen::PlainObjectBase<DerivedV> &B1,
                               Eigen::PlainObjectBase<DerivedV> &B2,
                               Eigen::PlainObjectBase<DerivedV> &B3)
{
  using namespace Eigen;
  using namespace std;
  B1.resize(F.rows(), 3);
  B2.resize(F.rows(), 3);
  B3.resize(F.rows(), 3);

  for (unsigned i = 0; i < F.rows(); ++i) {
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> v1 =
        (V.row(F(i, 1)) - V.row(F(i, 0))).normalized();
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> t = V.row(F(i, 2)) - V.row(F(i, 0));
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> v3 = v1.cross(t).normalized();
    Eigen::Matrix<typename DerivedV::Scalar, 1, 3> v2 = v1.cross(v3).normalized();

    B1.row(i) = v1;
    B2.row(i) = -v2;
    B3.row(i) = v3;
  }
}

static inline void pre_calc(SLIMData &s)
{
  BLI_assert(s.valid);
  if (!s.has_pre_calc) {
    s.v_n = s.v_num;
    s.f_n = s.f_num;

    s.dim = 2;
    Eigen::MatrixXd F1, F2, F3;
    local_basis(s.V, s.F, F1, F2, F3);
    compute_surface_gradient_matrix(s.V, s.F, F1, F2, s.Dx, s.Dy);

    s.W_11.resize(s.f_n);
    s.W_12.resize(s.f_n);
    s.W_21.resize(s.f_n);
    s.W_22.resize(s.f_n);

    s.Dx.makeCompressed();
    s.Dy.makeCompressed();
    s.Dz.makeCompressed();
    s.Ri.resize(s.f_n, s.dim * s.dim);
    s.Ji.resize(s.f_n, s.dim * s.dim);
    s.rhs.resize(s.dim * s.v_num);

    /* Flattened weight matrix. */
    s.WGL_M.resize(s.dim * s.dim * s.f_n);
    for (int i = 0; i < s.dim * s.dim; i++) {
      for (int j = 0; j < s.f_n; j++) {
        s.WGL_M(i * s.f_n + j) = s.M(j);
      }
    }

    s.first_solve = true;
    s.has_pre_calc = true;
  }
}

static inline void buildA(SLIMData &s, Eigen::SparseMatrix<double> &A)
{
  BLI_assert(s.valid);
  /* Formula (35) in paper. */
  std::vector<Eigen::Triplet<double>> IJV;

  IJV.reserve(4 * (s.Dx.outerSize() + s.Dy.outerSize()));

  /* A = [W11*Dx, W12*Dx;
   *      W11*Dy, W12*Dy;
   *      W21*Dx, W22*Dx;
   *      W21*Dy, W22*Dy]; */
  for (int k = 0; k < s.Dx.outerSize(); ++k) {
    for (Eigen::SparseMatrix<double>::InnerIterator it(s.Dx, k); it; ++it) {
      int dx_r = it.row();
      int dx_c = it.col();
      double val = it.value();
      double weight = s.weightPerFaceMap(dx_r);

      IJV.emplace_back(dx_r, dx_c, weight * val * s.W_11(dx_r));
      IJV.emplace_back(dx_r, s.v_n + dx_c, weight * val * s.W_12(dx_r));

      IJV.emplace_back(2 * s.f_n + dx_r, dx_c, weight * val * s.W_21(dx_r));
      IJV.emplace_back(2 * s.f_n + dx_r, s.v_n + dx_c, weight * val * s.W_22(dx_r));
    }
  }

  for (int k = 0; k < s.Dy.outerSize(); ++k) {
    for (Eigen::SparseMatrix<double>::InnerIterator it(s.Dy, k); it; ++it) {
      int dy_r = it.row();
      int dy_c = it.col();
      double val = it.value();
      double weight = s.weightPerFaceMap(dy_r);

      IJV.emplace_back(s.f_n + dy_r, dy_c, weight * val * s.W_11(dy_r));
      IJV.emplace_back(s.f_n + dy_r, s.v_n + dy_c, weight * val * s.W_12(dy_r));

      IJV.emplace_back(3 * s.f_n + dy_r, dy_c, weight * val * s.W_21(dy_r));
      IJV.emplace_back(3 * s.f_n + dy_r, s.v_n + dy_c, weight * val * s.W_22(dy_r));
    }
  }

  A.setFromTriplets(IJV.begin(), IJV.end());
}

static inline void buildRhs(SLIMData &s, const Eigen::SparseMatrix<double> &At)
{
  BLI_assert(s.valid);

  Eigen::VectorXd f_rhs(s.dim * s.dim * s.f_n);
  f_rhs.setZero();

  /* b = [W11*R11 + W12*R21; (formula (36))
   *      W11*R12 + W12*R22;
   *      W21*R11 + W22*R21;
   *      W21*R12 + W22*R22]; */
  for (int i = 0; i < s.f_n; i++) {
    f_rhs(i + 0 * s.f_n) = s.W_11(i) * s.Ri(i, 0) + s.W_12(i) * s.Ri(i, 1);
    f_rhs(i + 1 * s.f_n) = s.W_11(i) * s.Ri(i, 2) + s.W_12(i) * s.Ri(i, 3);
    f_rhs(i + 2 * s.f_n) = s.W_21(i) * s.Ri(i, 0) + s.W_22(i) * s.Ri(i, 1);
    f_rhs(i + 3 * s.f_n) = s.W_21(i) * s.Ri(i, 2) + s.W_22(i) * s.Ri(i, 3);
  }

  Eigen::VectorXd uv_flat(s.dim * s.v_n);
  for (int i = 0; i < s.dim; i++) {
    for (int j = 0; j < s.v_n; j++) {
      uv_flat(s.v_n * i + j) = s.V_o(j, i);
    }
  }

  s.rhs = (At * s.WGL_M.asDiagonal() * f_rhs + s.proximal_p * uv_flat);
}

static inline void add_soft_constraints(SLIMData &s, Eigen::SparseMatrix<double> &L)
{
  BLI_assert(s.valid);
  int v_n = s.v_num;
  for (int d = 0; d < s.dim; d++) {
    for (int i = 0; i < s.b.rows(); i++) {
      int v_idx = s.b(i);
      s.rhs(d * v_n + v_idx) += s.soft_const_p * s.bc(i, d);          /* Right hand side. */
      L.coeffRef(d * v_n + v_idx, d * v_n + v_idx) += s.soft_const_p; /* Diagonal of matrix. */
    }
  }
}

static inline void build_linear_system(SLIMData &s, Eigen::SparseMatrix<double> &L)
{
  BLI_assert(s.valid);
  /* Formula (35) in paper. */
  Eigen::SparseMatrix<double> A(s.dim * s.dim * s.f_n, s.dim * s.v_n);
  buildA(s, A);

  Eigen::SparseMatrix<double> At = A.transpose();
  At.makeCompressed();

  Eigen::SparseMatrix<double> id_m(At.rows(), At.rows());
  id_m.setIdentity();

  /* Add proximal penalty. */
  L = At * s.WGL_M.asDiagonal() * A + s.proximal_p * id_m; /* Add also a proximal term. */
  L.makeCompressed();

  buildRhs(s, At);
  Eigen::SparseMatrix<double> OldL = L;
  add_soft_constraints(s, L);
  L.makeCompressed();
}

static inline double compute_energy_with_jacobians(SLIMData &s,
                                                   const Eigen::MatrixXd &Ji,
                                                   Eigen::VectorXd &areas,
                                                   Eigen::VectorXd &singularValues,
                                                   bool gatherSingularValues)
{
  BLI_assert(s.valid);
  double energy = 0;

  Eigen::Matrix<double, 2, 2> ji;
  for (int i = 0; i < s.f_n; i++) {
    ji(0, 0) = Ji(i, 0);
    ji(0, 1) = Ji(i, 1);
    ji(1, 0) = Ji(i, 2);
    ji(1, 1) = Ji(i, 3);

    using Mat2 = Eigen::Matrix<double, 2, 2>;
    using Vec2 = Eigen::Matrix<double, 2, 1>;
    Mat2 ri, ti, ui, vi;
    Vec2 sing;
    polar_svd(ji, ri, ti, ui, sing, vi);
    double s1 = sing(0);
    double s2 = sing(1);

    switch (s.slim_energy) {
      case SLIMData::ARAP: {
        energy += areas(i) * (pow(s1 - 1, 2) + pow(s2 - 1, 2));
        break;
      }
      case SLIMData::SYMMETRIC_DIRICHLET: {
        energy += areas(i) * (pow(s1, 2) + pow(s1, -2) + pow(s2, 2) + pow(s2, -2));

        if (gatherSingularValues) {
          singularValues(i) = s1;
          singularValues(i + s.F.rows()) = s2;
        }
        break;
      }
      case SLIMData::EXP_SYMMETRIC_DIRICHLET: {
        energy += areas(i) *
                  exp(s.exp_factor * (pow(s1, 2) + pow(s1, -2) + pow(s2, 2) + pow(s2, -2)));
        break;
      }
      case SLIMData::LOG_ARAP: {
        energy += areas(i) * (pow(log(s1), 2) + pow(log(s2), 2));
        break;
      }
      case SLIMData::CONFORMAL: {
        energy += areas(i) * ((pow(s1, 2) + pow(s2, 2)) / (2 * s1 * s2));
        break;
      }
      case SLIMData::EXP_CONFORMAL: {
        energy += areas(i) * exp(s.exp_factor * ((pow(s1, 2) + pow(s2, 2)) / (2 * s1 * s2)));
        break;
      }
    }
  }

  return energy;
}

static inline double compute_soft_const_energy(SLIMData &s, Eigen::MatrixXd &V_o)
{
  BLI_assert(s.valid);
  double e = 0;
  for (int i = 0; i < s.b.rows(); i++) {
    e += s.soft_const_p * (s.bc.row(i) - V_o.row(s.b(i))).squaredNorm();
  }
  return e;
}

static inline double compute_energy(SLIMData &s,
                                    Eigen::MatrixXd &V_new,
                                    Eigen::VectorXd &singularValues,
                                    bool gatherSingularValues)
{
  BLI_assert(s.valid);
  compute_jacobians(s, V_new);
  return compute_energy_with_jacobians(s, s.Ji, s.M, singularValues, gatherSingularValues) +
         compute_soft_const_energy(s, V_new);
}

static inline double compute_energy(SLIMData &s, Eigen::MatrixXd &V_new)
{
  BLI_assert(s.valid);
  Eigen::VectorXd temp;
  return compute_energy(s, V_new, temp, false);
}

static inline double compute_energy(SLIMData &s,
                                    Eigen::MatrixXd &V_new,
                                    Eigen::VectorXd &singularValues)
{
  BLI_assert(s.valid);
  return compute_energy(s, V_new, singularValues, true);
}

void slim_precompute(Eigen::MatrixXd &V,
                     Eigen::MatrixXi &F,
                     Eigen::MatrixXd &V_init,
                     SLIMData &data,
                     SLIMData::SLIM_ENERGY slim_energy,
                     Eigen::VectorXi &b,
                     Eigen::MatrixXd &bc,
                     double soft_p)
{
  BLI_assert(data.valid);
  data.V = V;
  data.F = F;
  data.V_o = V_init;

  data.v_num = V.rows();
  data.f_num = F.rows();

  data.slim_energy = slim_energy;

  data.b = b;
  data.bc = bc;
  data.soft_const_p = soft_p;

  data.proximal_p = 0.0001;

  doublearea(V, F, data.M);
  data.M /= 2.;
  data.mesh_area = data.M.sum();

  data.mesh_improvement_3d = false; /* Whether to use a jacobian derived from a real mesh or an
                                     * abstract regular mesh (used for mesh improvement). */
  data.exp_factor =
      1.0; /* Param used only for exponential energies (e.g exponential symmetric dirichlet). */

  assert(F.cols() == 3);

  pre_calc(data);

  data.energy = compute_energy(data, data.V_o) / data.mesh_area;
}

inline double computeGlobalScaleInvarianceFactor(Eigen::VectorXd &singularValues,
                                                 Eigen::VectorXd &areas)
{
  int nFaces = singularValues.rows() / 2;

  Eigen::VectorXd areasChained(2 * nFaces);
  areasChained << areas, areas;

  /* Per face energy for face i with singvals si1 and si2 and area ai when scaling geometry by x is
   *
   *  ai*(si1*x)^2 + ai*(si2*x)^2 + ai/(si1*x)^2 + ai/(si2*x)^2)
   *
   * The combined Energy of all faces is therefore
   * (s1 and s2 are the sums over all ai*(si1^2) and ai*(si2^2) respectively. t1 and t2
   * are the sums over all ai/(si1^2) and ai/(si2^2) respectively)
   *
   *   s1*(x^2) + s2*(x^2) + t1/(x^2) + t2/(x^2)
   *
   * with a = (s1 + s2) and b = (t1 + t2) we get
   *
   *   ax^2 + b/x^2
   *
   * it's derivative is
   *
   *   2ax - 2b/(x^3)
   *
   * and when we set it zero we get
   *
   *  x^4 = b/a => x = sqrt(sqrt(b/a))
   */

  Eigen::VectorXd squaredSingularValues = singularValues.cwiseProduct(singularValues);
  Eigen::VectorXd inverseSquaredSingularValues =
      singularValues.cwiseProduct(singularValues).cwiseInverse();

  Eigen::VectorXd weightedSquaredSingularValues = squaredSingularValues.cwiseProduct(areasChained);
  Eigen::VectorXd weightedInverseSquaredSingularValues = inverseSquaredSingularValues.cwiseProduct(
      areasChained);

  double s1 = weightedSquaredSingularValues.head(nFaces).sum();
  double s2 = weightedSquaredSingularValues.tail(nFaces).sum();

  double t1 = weightedInverseSquaredSingularValues.head(nFaces).sum();
  double t2 = weightedInverseSquaredSingularValues.tail(nFaces).sum();

  double a = s1 + s2;
  double b = t1 + t2;

  double x = sqrt(sqrt(b / a));

  return 1 / x;
}

static inline void solve_weighted_arap(SLIMData &s, Eigen::MatrixXd &uv)
{
  BLI_assert(s.valid);
  using namespace Eigen;

  Eigen::SparseMatrix<double> L;
  build_linear_system(s, L);

  /* Solve. */
  Eigen::VectorXd Uc;
  SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
  Uc = solver.compute(L).solve(s.rhs);

  for (int i = 0; i < Uc.size(); i++) {
    if (!std::isfinite(Uc(i))) {
      throw SlimFailedException();
    }
  }

  for (int i = 0; i < s.dim; i++) {
    uv.col(i) = Uc.block(i * s.v_n, 0, s.v_n, 1);
  }
}

Eigen::MatrixXd slim_solve(SLIMData &data, int iter_num)
{
  BLI_assert(data.valid);
  Eigen::VectorXd singularValues;
  bool are_pins_present = data.b.rows() > 0;

  if (are_pins_present) {
    singularValues.resize(data.F.rows() * 2);
    data.energy = compute_energy(data, data.V_o, singularValues) / data.mesh_area;
  }

  for (int i = 0; i < iter_num; i++) {
    Eigen::MatrixXd dest_res;
    dest_res = data.V_o;

    /* Solve Weighted Proxy. */
    update_weights_and_closest_rotations(data, dest_res);
    solve_weighted_arap(data, dest_res);

    std::function<double(Eigen::MatrixXd &)> compute_energy_func = [&](Eigen::MatrixXd &aaa) {
      return are_pins_present ? compute_energy(data, aaa, singularValues) :
                                compute_energy(data, aaa);
    };

    data.energy = flip_avoiding_line_search(data.F,
                                            data.V_o,
                                            dest_res,
                                            compute_energy_func,
                                            data.energy * data.mesh_area) /
                  data.mesh_area;

    if (are_pins_present) {
      data.globalScaleInvarianceFactor = computeGlobalScaleInvarianceFactor(singularValues,
                                                                            data.M);
      data.Dx /= data.globalScaleInvarianceFactor;
      data.Dy /= data.globalScaleInvarianceFactor;
      data.energy = compute_energy(data, data.V_o, singularValues) / data.mesh_area;
    }
  }

  return data.V_o;
}

}  // namespace slim
