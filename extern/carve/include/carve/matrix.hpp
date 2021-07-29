// Begin License:
// Copyright (C) 2006-2014 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of either the GNU General
// Public License version 2 or 3 (at your option) as published by the
// Free Software Foundation and appearing in the files LICENSE.GPL2
// and LICENSE.GPL3 included in the packaging of this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include <cstring>

#include <carve/carve.hpp>

#include <carve/math.hpp>
#include <carve/geom.hpp>

namespace carve {
  namespace math {

    struct Quaternion {
      double x, y, z, w;

      Quaternion(double _x, double _y, double _z, double _w) : x(_x), y(_y), z(_z), w(_w) {
      }

      Quaternion(double angle, const carve::geom::vector<3> &axis) {
        double s = axis.length();
        if (!carve::math::ZERO(s)) {
          double c = 1.0 / s;
          double omega = -0.5 * angle;
          s = sin(omega);
          x = axis.x * c * s;
          y = axis.y * c * s;
          z = axis.z * c * s;
          w = cos(omega);
          normalize();
        } else {
          x = y = z = 0.0;
          w = 1.0;
        }
      }

      double lengthSquared() const {
        return x * x + y * y + z * z + w * w;
      }

      double length() const {
        return sqrt(lengthSquared());
      }

      Quaternion normalized() const {
        return Quaternion(*this).normalize();
      }

      Quaternion &normalize() {
        double l = length();
        if (l == 0.0) {
          x = 1.0; y = 0.0; z = 0.0; w = 0.0;
        } else {
          x /= l; y /= l; z /= l; w /= l;
        }
        return *this;
      }
    };

    struct Matrix3 {
      // access: .m[col][row], .v[col * 4 + row], ._cr
      union {
        double m[3][3];
        double v[9];
        struct {
          // transposed
          double _11, _12, _13;
          double _21, _22, _23;
          double _31, _32, _33;
        };
      };
      Matrix3(double __11, double __21, double __31,
              double __12, double __22, double __32,
              double __13, double __23, double __33) {
        // nb, args are row major, storage is column major.
        _11 = __11; _12 = __12; _13 = __13;
        _21 = __21; _22 = __22; _23 = __23;
        _31 = __31; _32 = __32; _33 = __33;
      }
      Matrix3(double _m[3][3]) {
        std::memcpy(m, _m, sizeof(m));
      }
      Matrix3(double _v[9]) {
        std::memcpy(v, _v, sizeof(v));
      }
      Matrix3() {
        _11 = 1.00; _12 = 0.00; _13 = 0.00;
        _21 = 0.00; _22 = 1.00; _23 = 0.00;
        _31 = 0.00; _32 = 0.00; _33 = 1.00;
      }
    };

    struct Matrix {
      // access: .m[col][row], .v[col * 4 + row], ._cr
      union {
        double m[4][4];
        double v[16];
        struct {
          // transposed
          double _11, _12, _13, _14;
          double _21, _22, _23, _24;
          double _31, _32, _33, _34;
          double _41, _42 ,_43, _44;
        };
      };
      Matrix(double __11, double __21, double __31, double __41,
             double __12, double __22, double __32, double __42,
             double __13, double __23, double __33, double __43,
             double __14, double __24, double __34, double __44) {
        // nb, args are row major, storage is column major.
        _11 = __11; _12 = __12; _13 = __13; _14 = __14;
        _21 = __21; _22 = __22; _23 = __23; _24 = __24;
        _31 = __31; _32 = __32; _33 = __33; _34 = __34;
        _41 = __41; _42 = __42; _43 = __43; _44 = __44;
      }
      Matrix(double _m[4][4]) {
        std::memcpy(m, _m, sizeof(m));
      }
      Matrix(double _v[16]) {
        std::memcpy(v, _v, sizeof(v));
      }
      Matrix() {
        _11 = 1.00; _12 = 0.00; _13 = 0.00; _14 = 0.00;
        _21 = 0.00; _22 = 1.00; _23 = 0.00; _24 = 0.00;
        _31 = 0.00; _32 = 0.00; _33 = 1.00; _34 = 0.00;
        _41 = 0.00; _42 = 0.00; _43 = 0.00; _44 = 1.00;
      }

      static Matrix ROT(const Quaternion &q) {
        const double w = q.w;
        const double x = q.x;
        const double y = q.y;
        const double z = q.z;
        return Matrix(1 - 2*y*y - 2*z*z,   2*x*y - 2*z*w,       2*x*z + 2*y*w,       0.0,  
                      2*x*y + 2*z*w,       1 - 2*x*x - 2*z*z,   2*y*z - 2*x*w,       0.0,  
                      2*x*z - 2*y*w,       2*y*z + 2*x*w,       1 - 2*x*x - 2*y*y,   0.0,  
                      0.0,                 0.0,                 0.0,                 1.0);
      }
      static Matrix ROT(double angle, const carve::geom::vector<3> &axis) {
        return ROT(Quaternion(angle, axis));
      }
      static Matrix ROT(double angle, double x, double y, double z) {
        return ROT(Quaternion(angle, carve::geom::VECTOR(x, y, z)));
      }
      static Matrix TRANS(double x, double y, double z) {
        return Matrix(1.0,   0.0,   0.0,   x,
                      0.0,   1.0,   0.0,   y,
                      0.0,   0.0,   1.0,   z,
                      0.0,   0.0,   0.0,   1.0);
      }
      static Matrix TRANS(const carve::geom::vector<3> &v) {
        return TRANS(v.x, v.y, v.z);
      }
      static Matrix SCALE(double x, double y, double z) {
        return Matrix(x,     0.0,   0.0,   0.0,
                      0.0,   y,     0.0,   0.0,
                      0.0,   0.0,   z,     0.0,
                      0.0,   0.0,   0.0,   1.0);
      }
      static Matrix SCALE(const carve::geom::vector<3> &v) {
        return SCALE(v.x, v.y, v.z);
      }
      static Matrix IDENT() {
        return Matrix(1.0,   0.0,   0.0,   0.0,
                      0.0,   1.0,   0.0,   0.0,
                      0.0,   0.0,   1.0,   0.0,
                      0.0,   0.0,   0.0,   1.0);
      }
    };

    static inline bool operator==(const Matrix &A, const Matrix &B) {
      for (size_t i = 0; i < 16; ++i) if (A.v[i] != B.v[i]) return false;
      return true;
    }
    static inline bool operator!=(const Matrix &A, const Matrix &B) {
      return !(A == B);
    }
    static inline carve::geom::vector<3> operator*(const Matrix &A, const carve::geom::vector<3> &b) {
      return carve::geom::VECTOR(
                    A._11 * b.x + A._21 * b.y + A._31 * b.z + A._41,
                    A._12 * b.x + A._22 * b.y + A._32 * b.z + A._42,
                    A._13 * b.x + A._23 * b.y + A._33 * b.z + A._43
                    );
    }

    static inline carve::geom::vector<3> &operator*=(carve::geom::vector<3> &b, const Matrix &A) {
      b = A * b;
      return b;
    }

    static inline carve::geom::vector<3> operator*(const Matrix3 &A, const carve::geom::vector<3> &b) {
      return carve::geom::VECTOR(
                    A._11 * b.x + A._21 * b.y + A._31 * b.z,
                    A._12 * b.x + A._22 * b.y + A._32 * b.z,
                    A._13 * b.x + A._23 * b.y + A._33 * b.z
                    );
    }

    static inline carve::geom::vector<3> &operator*=(carve::geom::vector<3> &b, const Matrix3 &A) {
      b = A * b;
      return b;
    }

    static inline Matrix operator*(const Matrix &A, const Matrix &B) {
      Matrix c;
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          c.m[i][j] = 0.0;
          for (int k = 0; k < 4; k++) {
            c.m[i][j] += A.m[k][j] * B.m[i][k];
          }
        }
      }
      return c;
    }

    static inline Matrix3 operator*(const Matrix3 &A, const Matrix3 &B) {
      Matrix3 c;
      for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
          c.m[i][j] = 0.0;
          for (int k = 0; k < 3; k++) {
            c.m[i][j] += A.m[k][j] * B.m[i][k];
          }
        }
      }
      return c;
    }



    struct matrix_transformation {
      Matrix matrix;

      matrix_transformation(const Matrix &_matrix) : matrix(_matrix) {
      }

      carve::geom::vector<3> operator()(const carve::geom::vector<3> &vector) const {
        return matrix * vector;
      }
    };



  }
}
