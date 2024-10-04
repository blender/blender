/* SPDX-FileCopyrightText: 2013 Alec Jacobson
 *
 * SPDX-License-Identifier: MPL-2.0 */

/** \file
 * \ingroup intern_slim
 */

#include "doublearea.h"
#include "edge_lengths.h"

#include <cassert>

#include <BLI_task.hh>

namespace slim {

/* Sort the elements of a matrix X along a given dimension like matlabs sort
 * function, assuming X.cols() == 3.
 *
 * Templates:
 *   DerivedX derived scalar type, e.g. MatrixXi or MatrixXd
 *   DerivedIX derived integer type, e.g. MatrixXi
 * Inputs:
 *   X  m by n matrix whose entries are to be sorted
 *   dim  dimensional along which to sort:
 *     1  sort each column (matlab default)
 *     2  sort each row
 *   ascending  sort ascending (true, matlab default) or descending (false)
 * Outputs:
 *   Y  m by n matrix whose entries are sorted
 *   IX  m by n matrix of indices so that if dim = 1, then in matlab notation
 *     for j = 1:n, Y(:,j) = X(I(:,j),j); end
 */
template<typename DerivedX, typename DerivedY, typename DerivedIX>
static inline void doublearea_sort3(const Eigen::PlainObjectBase<DerivedX> &X,
                                    const int dim,
                                    const bool ascending,
                                    Eigen::PlainObjectBase<DerivedY> &Y,
                                    Eigen::PlainObjectBase<DerivedIX> &IX)
{
  using namespace Eigen;
  using namespace std;
  typedef typename Eigen::PlainObjectBase<DerivedY>::Scalar YScalar;
  Y = X.template cast<YScalar>();
  /* Get number of columns (or rows). */
  int num_outer = (dim == 1 ? X.cols() : X.rows());
  /* Get number of rows (or columns). */
  int num_inner = (dim == 1 ? X.rows() : X.cols());
  assert(num_inner == 3);
  (void)num_inner;
  typedef typename Eigen::PlainObjectBase<DerivedIX>::Scalar Index;
  IX.resize(X.rows(), X.cols());
  if (dim == 1) {
    IX.row(0).setConstant(0); /* = Eigen::PlainObjectBase<DerivedIX>::Zero(1,IX.cols());. */
    IX.row(1).setConstant(1); /* = Eigen::PlainObjectBase<DerivedIX>::Ones (1,IX.cols());. */
    IX.row(2).setConstant(2); /* = Eigen::PlainObjectBase<DerivedIX>::Ones (1,IX.cols());. */
  }
  else {
    IX.col(0).setConstant(0); /* = Eigen::PlainObjectBase<DerivedIX>::Zero(IX.rows(),1);. */
    IX.col(1).setConstant(1); /* = Eigen::PlainObjectBase<DerivedIX>::Ones (IX.rows(),1);. */
    IX.col(2).setConstant(2); /* = Eigen::PlainObjectBase<DerivedIX>::Ones (IX.rows(),1);. */
  }

  using namespace blender;
  threading::parallel_for(
      IndexRange(num_outer), 16000, [&IX, &Y, &dim, &ascending](const IndexRange range) {
        for (const Index i : range) {
          YScalar &a = (dim == 1 ? Y(0, i) : Y(i, 0));
          YScalar &b = (dim == 1 ? Y(1, i) : Y(i, 1));
          YScalar &c = (dim == 1 ? Y(2, i) : Y(i, 2));
          Index &ai = (dim == 1 ? IX(0, i) : IX(i, 0));
          Index &bi = (dim == 1 ? IX(1, i) : IX(i, 1));
          Index &ci = (dim == 1 ? IX(2, i) : IX(i, 2));
          if (ascending) {
            /* 123 132 213 231 312 321. */
            if (a > b) {
              std::swap(a, b);
              std::swap(ai, bi);
            }
            /* 123 132 123 231 132 231. */
            if (b > c) {
              std::swap(b, c);
              std::swap(bi, ci);
              /* 123 123 123 213 123 213. */
              if (a > b) {
                std::swap(a, b);
                std::swap(ai, bi);
              }
              /* 123 123 123 123 123 123. */
            }
          }
          else {
            /* 123 132 213 231 312 321. */
            if (a < b) {
              std::swap(a, b);
              std::swap(ai, bi);
            }
            /* 213 312 213 321 312 321. */
            if (b < c) {
              std::swap(b, c);
              std::swap(bi, ci);
              /* 231 321 231 321 321 321. */
              if (a < b) {
                std::swap(a, b);
                std::swap(ai, bi);
              }
              /* 321 321 321 321 321 321. */
            }
          }
        }
      });
}

template<typename DerivedV, typename DerivedF, typename DeriveddblA>
inline void doublearea(const Eigen::PlainObjectBase<DerivedV> &V,
                       const Eigen::PlainObjectBase<DerivedF> &F,
                       Eigen::PlainObjectBase<DeriveddblA> &dblA)
{
  const int dim = V.cols();
  /* Only support triangles. */
  assert(F.cols() == 3);
  const size_t m = F.rows();
  /* Compute edge lengths. */
  Eigen::Matrix<typename DerivedV::Scalar, Eigen::Dynamic, 3> l;

  /* Projected area helper. */
  const auto &proj_doublearea = [&V, &F](const int x, const int y, const int f) -> double {
    auto rx = V(F(f, 0), x) - V(F(f, 2), x);
    auto sx = V(F(f, 1), x) - V(F(f, 2), x);
    auto ry = V(F(f, 0), y) - V(F(f, 2), y);
    auto sy = V(F(f, 1), y) - V(F(f, 2), y);
    return rx * sy - ry * sx;
  };

  switch (dim) {
    case 3: {
      dblA = Eigen::PlainObjectBase<DeriveddblA>::Zero(m, 1);
      for (size_t f = 0; f < m; f++) {
        for (int d = 0; d < 3; d++) {
          double dblAd = proj_doublearea(d, (d + 1) % 3, f);
          dblA(f) += dblAd * dblAd;
        }
      }
      dblA = dblA.array().sqrt().eval();
      break;
    }
    case 2: {
      dblA.resize(m, 1);
      for (size_t f = 0; f < m; f++) {
        dblA(f) = proj_doublearea(0, 1, f);
      }
      break;
    }
    default: {
      edge_lengths(V, F, l);
      return doublearea(l, dblA);
    }
  }
}

template<typename Derivedl, typename DeriveddblA>
inline void doublearea(const Eigen::PlainObjectBase<Derivedl> &ul,
                       Eigen::PlainObjectBase<DeriveddblA> &dblA)
{
  using namespace Eigen;
  using namespace std;
  typedef typename Derivedl::Index Index;
  /* Only support triangles. */
  assert(ul.cols() == 3);
  /* Number of triangles. */
  const Index m = ul.rows();
  Eigen::Matrix<typename Derivedl::Scalar, Eigen::Dynamic, 3> l;
  MatrixXi _;
  /* "Lecture Notes on Geometric Robustness" Shewchuck 09, Section 3.1
   * http://www.cs.berkeley.edu/~jrs/meshpapers/robnotes.pdf
   *
   * "Miscalculating Area and Angles of a Needle-like Triangle"
   * https://people.eecs.berkeley.edu/~wkahan/Triangle.pdf
   */
  doublearea_sort3(ul, 2, false, l, _);
  dblA.resize(l.rows(), 1);

  using namespace blender;
  threading::parallel_for(IndexRange(m), 1000, [&l, &dblA](const IndexRange range) {
    for (const Index i : range) {
      /* Kahan's Heron's formula. */
      const typename Derivedl::Scalar arg = (l(i, 0) + (l(i, 1) + l(i, 2))) *
                                            (l(i, 2) - (l(i, 0) - l(i, 1))) *
                                            (l(i, 2) + (l(i, 0) - l(i, 1))) *
                                            (l(i, 0) + (l(i, 1) - l(i, 2)));
      dblA(i) = 2.0 * 0.25 * sqrt(arg);
      assert(l(i, 2) - (l(i, 0) - l(i, 1)) && "FAILED KAHAN'S ASSERTION");
      assert(dblA(i) == dblA(i) && "DOUBLEAREA() PRODUCED NaN");
    }
  });
}

}  // namespace slim
