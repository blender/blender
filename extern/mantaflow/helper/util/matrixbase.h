/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2015 Kiwon Um, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL)
 * http://www.gnu.org/licenses
 *
 * Matrix (3x3) class
 *
 ******************************************************************************/

#ifndef MATRIXBASE_H
#define MATRIXBASE_H

#include "vectorbase.h"

namespace Manta {

template<typename T> class Matrix3x3 {
 public:
  // NOTE: default is the identity matrix!
  explicit Matrix3x3(const T &p00 = 1,
                     const T &p01 = 0,
                     const T &p02 = 0,
                     const T &p10 = 0,
                     const T &p11 = 1,
                     const T &p12 = 0,
                     const T &p20 = 0,
                     const T &p21 = 0,
                     const T &p22 = 1)
  {
    v[0][0] = p00;
    v[0][1] = p01;
    v[0][2] = p02;
    v[1][0] = p10;
    v[1][1] = p11;
    v[1][2] = p12;
    v[2][0] = p20;
    v[2][1] = p21;
    v[2][2] = p22;
  }

  explicit Matrix3x3(const Vector3D<T> &diag)
  {
    v[0][0] = diag.x;
    v[0][1] = 0;
    v[0][2] = 0;
    v[1][0] = 0;
    v[1][1] = diag.y;
    v[1][2] = 0;
    v[2][0] = 0;
    v[2][1] = 0;
    v[2][2] = diag.z;
  }

  Matrix3x3(const Vector3D<T> &c0, const Vector3D<T> &c1, const Vector3D<T> &c2)
  {
    v[0][0] = c0.x;
    v[0][1] = c1.x;
    v[0][2] = c2.x;
    v[1][0] = c0.y;
    v[1][1] = c1.y;
    v[1][2] = c2.y;
    v[2][0] = c0.z;
    v[2][1] = c1.z;
    v[2][2] = c2.z;
  }

  // assignment operators
  Matrix3x3 &operator+=(const Matrix3x3 &m)
  {
    v00 += m.v00;
    v01 += m.v01;
    v02 += m.v02;
    v10 += m.v10;
    v11 += m.v11;
    v12 += m.v12;
    v20 += m.v20;
    v21 += m.v21;
    v22 += m.v22;
    return *this;
  }
  Matrix3x3 &operator-=(const Matrix3x3 &m)
  {
    v00 -= m.v00;
    v01 -= m.v01;
    v02 -= m.v02;
    v10 -= m.v10;
    v11 -= m.v11;
    v12 -= m.v12;
    v20 -= m.v20;
    v21 -= m.v21;
    v22 -= m.v22;
    return *this;
  }
  Matrix3x3 &operator*=(const T s)
  {
    v00 *= s;
    v01 *= s;
    v02 *= s;
    v10 *= s;
    v11 *= s;
    v12 *= s;
    v20 *= s;
    v21 *= s;
    v22 *= s;
    return *this;
  }
  Matrix3x3 &operator/=(const T s)
  {
    v00 /= s;
    v01 /= s;
    v02 /= s;
    v10 /= s;
    v11 /= s;
    v12 /= s;
    v20 /= s;
    v21 /= s;
    v22 /= s;
    return *this;
  }

  // binary operators
  Matrix3x3 operator+(const Matrix3x3 &m) const
  {
    return Matrix3x3(*this) += m;
  }
  Matrix3x3 operator-(const Matrix3x3 &m) const
  {
    return Matrix3x3(*this) -= m;
  }
  Matrix3x3 operator*(const Matrix3x3 &m) const
  {
    return Matrix3x3(v00 * m.v00 + v01 * m.v10 + v02 * m.v20,
                     v00 * m.v01 + v01 * m.v11 + v02 * m.v21,
                     v00 * m.v02 + v01 * m.v12 + v02 * m.v22,

                     v10 * m.v00 + v11 * m.v10 + v12 * m.v20,
                     v10 * m.v01 + v11 * m.v11 + v12 * m.v21,
                     v10 * m.v02 + v11 * m.v12 + v12 * m.v22,

                     v20 * m.v00 + v21 * m.v10 + v22 * m.v20,
                     v20 * m.v01 + v21 * m.v11 + v22 * m.v21,
                     v20 * m.v02 + v21 * m.v12 + v22 * m.v22);
  }
  Matrix3x3 operator*(const T s) const
  {
    return Matrix3x3(*this) *= s;
  }
  Vector3D<T> operator*(const Vector3D<T> &v) const
  {
    return Vector3D<T>(v00 * v.x + v01 * v.y + v02 * v.z,
                       v10 * v.x + v11 * v.y + v12 * v.z,
                       v20 * v.x + v21 * v.y + v22 * v.z);
  }
  Vector3D<T> transposedMul(const Vector3D<T> &v) const
  {
    // M^T*v
    return Vector3D<T>(v00 * v.x + v10 * v.y + v20 * v.z,
                       v01 * v.x + v11 * v.y + v21 * v.z,
                       v02 * v.x + v12 * v.y + v22 * v.z);
  }
  Matrix3x3 transposedMul(const Matrix3x3 &m) const
  {
    // M^T*M
    return Matrix3x3(v00 * m.v00 + v10 * m.v10 + v20 * m.v20,
                     v00 * m.v01 + v10 * m.v11 + v20 * m.v21,
                     v00 * m.v02 + v10 * m.v12 + v20 * m.v22,

                     v01 * m.v00 + v11 * m.v10 + v21 * m.v20,
                     v01 * m.v01 + v11 * m.v11 + v21 * m.v21,
                     v01 * m.v02 + v11 * m.v12 + v21 * m.v22,

                     v02 * m.v00 + v12 * m.v10 + v22 * m.v20,
                     v02 * m.v01 + v12 * m.v11 + v22 * m.v21,
                     v02 * m.v02 + v12 * m.v12 + v22 * m.v22);
  }
  Matrix3x3 mulTranspose(const Matrix3x3 &m) const
  {
    // M*m^T
    return Matrix3x3(v00 * m.v00 + v01 * m.v01 + v02 * m.v02,
                     v00 * m.v10 + v01 * m.v11 + v02 * m.v12,
                     v00 * m.v20 + v01 * m.v21 + v02 * m.v22,

                     v10 * m.v00 + v11 * m.v01 + v12 * m.v02,
                     v10 * m.v10 + v11 * m.v11 + v12 * m.v12,
                     v10 * m.v20 + v11 * m.v21 + v12 * m.v22,

                     v20 * m.v00 + v21 * m.v01 + v22 * m.v02,
                     v20 * m.v10 + v21 * m.v11 + v22 * m.v12,
                     v20 * m.v20 + v21 * m.v21 + v22 * m.v22);
  }

  bool operator==(const Matrix3x3 &m) const
  {
    return (v00 == m.v00 && v01 == m.v01 && v02 == m.v02 && v10 == m.v10 && v11 == m.v11 &&
            v12 == m.v12 && v20 == m.v20 && v21 == m.v21 && v22 == m.v22);
  }

  const T &operator()(const int r, const int c) const
  {
    return v[r][c];
  }
  T &operator()(const int r, const int c)
  {
    return const_cast<T &>(const_cast<const Matrix3x3 &>(*this)(r, c));
  }

  T trace() const
  {
    return v00 + v11 + v22;
  }
  T sumSqr() const
  {
    return (v00 * v00 + v01 * v01 + v02 * v02 + v10 * v10 + v11 * v11 + v12 * v12 + v20 * v20 +
            v21 * v21 + v22 * v22);
  }

  Real determinant() const
  {
    return (v00 * v11 * v22 - v00 * v12 * v21 + v01 * v12 * v20 - v01 * v10 * v22 +
            v02 * v10 * v21 - v02 * v11 * v20);
  }
  Matrix3x3 &transpose()
  {
    return *this = transposed();
  }
  Matrix3x3 transposed() const
  {
    return Matrix3x3(v00, v10, v20, v01, v11, v21, v02, v12, v22);
  }
  Matrix3x3 &invert()
  {
    return *this = inverse();
  }
  Matrix3x3 inverse() const
  {
    const Real det = determinant();  // FIXME: assert(det);
    const Real idet = 1e0 / det;
    return Matrix3x3(idet * (v11 * v22 - v12 * v21),
                     idet * (v02 * v21 - v01 * v22),
                     idet * (v01 * v12 - v02 * v11),
                     idet * (v12 * v20 - v10 * v22),
                     idet * (v00 * v22 - v02 * v20),
                     idet * (v02 * v10 - v00 * v12),
                     idet * (v10 * v21 - v11 * v20),
                     idet * (v01 * v20 - v00 * v21),
                     idet * (v00 * v11 - v01 * v10));
  }
  bool getInverse(Matrix3x3 &inv) const
  {
    const Real det = determinant();
    if (det == 0e0)
      return false;  // FIXME: is it likely to happen the floating error?

    const Real idet = 1e0 / det;
    inv.v00 = idet * (v11 * v22 - v12 * v21);
    inv.v01 = idet * (v02 * v21 - v01 * v22);
    inv.v02 = idet * (v01 * v12 - v02 * v11);

    inv.v10 = idet * (v12 * v20 - v10 * v22);
    inv.v11 = idet * (v00 * v22 - v02 * v20);
    inv.v12 = idet * (v02 * v10 - v00 * v12);

    inv.v20 = idet * (v10 * v21 - v11 * v20);
    inv.v21 = idet * (v01 * v20 - v00 * v21);
    inv.v22 = idet * (v00 * v11 - v01 * v10);

    return true;
  }

  Real normOne() const
  {
    // the maximum absolute column sum of the matrix
    return max(std::fabs(v00) + std::fabs(v10) + std::fabs(v20),
               std::fabs(v01) + std::fabs(v11) + std::fabs(v21),
               std::fabs(v02) + std::fabs(v12) + std::fabs(v22));
  }
  Real normInf() const
  {
    // the maximum absolute row sum of the matrix
    return max(std::fabs(v00) + std::fabs(v01) + std::fabs(v02),
               std::fabs(v10) + std::fabs(v11) + std::fabs(v12),
               std::fabs(v20) + std::fabs(v21) + std::fabs(v22));
  }

  Vector3D<T> eigenvalues() const
  {
    Vector3D<T> eigen;

    const Real b = -v00 - v11 - v22;
    const Real c = v00 * (v11 + v22) + v11 * v22 - v12 * v21 - v01 * v10 - v02 * v20;
    Real d = -v00 * (v11 * v22 - v12 * v21) - v20 * (v01 * v12 - v11 * v02) -
             v10 * (v02 * v21 - v22 * v01);
    const Real f = (3.0 * c - b * b) / 3.0;
    const Real g = (2.0 * b * b * b - 9.0 * b * c + 27.0 * d) / 27.0;
    const Real h = g * g / 4.0 + f * f * f / 27.0;

    Real sign;
    if (h > 0) {
      Real r = -g / 2.0 + std::sqrt(h);
      if (r < 0) {
        r = -r;
        sign = -1.0;
      }
      else
        sign = 1.0;
      Real s = sign * std::pow(r, 1.0 / 3.0);
      Real t = -g / 2.0 - std::sqrt(h);
      if (t < 0) {
        t = -t;
        sign = -1.0;
      }
      else
        sign = 1.0;
      Real u = sign * std::pow(t, 1.0 / 3.0);
      eigen[0] = (s + u) - b / 3.0;
      eigen[1] = eigen[2] = 0;
    }
    else if (h == 0) {
      if (d < 0) {
        d = -d;
        sign = -1.0;
      }
      sign = 1.0;
      eigen[0] = -1.0 * sign * std::pow(d, 1.0 / 3.0);
      eigen[1] = eigen[2] = 0;
    }
    else {
      const Real i = std::sqrt(g * g / 4.0 - h);
      const Real j = std::pow(i, 1.0 / 3.0);
      const Real k = std::acos(-g / (2.0 * i));
      const Real l = -j;
      const Real m = std::cos(k / 3.0);
      const Real n = std::sqrt(3.0) * std::sin(k / 3.0);
      const Real p = -b / 3.0;
      eigen[0] = 2e0 * j * m + p;
      eigen[1] = l * (m + n) + p;
      eigen[2] = l * (m - n) + p;
    }

    return eigen;
  }

  static Matrix3x3 I()
  {
    return Matrix3x3(1, 0, 0, 0, 1, 0, 0, 0, 1);
  }

#ifdef _WIN32
#  pragma warning(disable : 4201)
#endif
  union {
    struct {
      T v00, v01, v02, v10, v11, v12, v20, v21, v22;
    };
    T v[3][3];
    T v1[9];
  };
#ifdef _WIN32
#  pragma warning(default : 4201)
#endif
};

template<typename T1, typename T> inline Matrix3x3<T> operator*(const T1 s, const Matrix3x3<T> &m)
{
  return m * static_cast<T>(s);
}

template<typename T> inline Matrix3x3<T> crossProductMatrix(const Vector3D<T> &v)
{
  return Matrix3x3<T>(0, -v.z, v.y, v.z, 0, -v.x, -v.y, v.x, 0);
}

template<typename T> inline Matrix3x3<T> outerProduct(const Vector3D<T> &a, const Vector3D<T> &b)
{
  return Matrix3x3<T>(a.x * b.x,
                      a.x * b.y,
                      a.x * b.z,
                      a.y * b.x,
                      a.y * b.y,
                      a.y * b.z,
                      a.z * b.x,
                      a.z * b.y,
                      a.z * b.z);
}

typedef Matrix3x3<Real> Matrix3x3f;

}  // namespace Manta

#endif /* MATRIXBASE_H */
