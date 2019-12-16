/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * 4D vector class
 *
 ******************************************************************************/

#ifndef _VECTOR4D_H
#define _VECTOR4D_H

#include "vectorbase.h"

namespace Manta {

//! Basic inlined vector class
template<class S> class Vector4D {
 public:
  //! Constructor
  inline Vector4D() : x(0), y(0), z(0), t(0)
  {
  }

  //! Copy-Constructor
  inline Vector4D(const Vector4D<S> &v) : x(v.x), y(v.y), z(v.z), t(v.t)
  {
  }

  //! Copy-Constructor
  inline Vector4D(const float *v) : x((S)v[0]), y((S)v[1]), z((S)v[2]), t((S)v[3])
  {
  }

  //! Copy-Constructor
  inline Vector4D(const double *v) : x((S)v[0]), y((S)v[1]), z((S)v[2]), t((S)v[3])
  {
  }

  //! Construct a vector from one S
  inline Vector4D(S v) : x(v), y(v), z(v), t(v)
  {
  }

  //! Construct a vector from three Ss
  inline Vector4D(S vx, S vy, S vz, S vw) : x(vx), y(vy), z(vz), t(vw)
  {
  }

  // Operators

  //! Assignment operator
  inline const Vector4D<S> &operator=(const Vector4D<S> &v)
  {
    x = v.x;
    y = v.y;
    z = v.z;
    t = v.t;
    return *this;
  }
  //! Assignment operator
  inline const Vector4D<S> &operator=(S s)
  {
    x = y = z = t = s;
    return *this;
  }
  //! Assign and add operator
  inline const Vector4D<S> &operator+=(const Vector4D<S> &v)
  {
    x += v.x;
    y += v.y;
    z += v.z;
    t += v.t;
    return *this;
  }
  //! Assign and add operator
  inline const Vector4D<S> &operator+=(S s)
  {
    x += s;
    y += s;
    z += s;
    t += s;
    return *this;
  }
  //! Assign and sub operator
  inline const Vector4D<S> &operator-=(const Vector4D<S> &v)
  {
    x -= v.x;
    y -= v.y;
    z -= v.z;
    t -= v.t;
    return *this;
  }
  //! Assign and sub operator
  inline const Vector4D<S> &operator-=(S s)
  {
    x -= s;
    y -= s;
    z -= s;
    t -= s;
    return *this;
  }
  //! Assign and mult operator
  inline const Vector4D<S> &operator*=(const Vector4D<S> &v)
  {
    x *= v.x;
    y *= v.y;
    z *= v.z;
    t *= v.t;
    return *this;
  }
  //! Assign and mult operator
  inline const Vector4D<S> &operator*=(S s)
  {
    x *= s;
    y *= s;
    z *= s;
    t *= s;
    return *this;
  }
  //! Assign and div operator
  inline const Vector4D<S> &operator/=(const Vector4D<S> &v)
  {
    x /= v.x;
    y /= v.y;
    z /= v.z;
    t /= v.t;
    return *this;
  }
  //! Assign and div operator
  inline const Vector4D<S> &operator/=(S s)
  {
    x /= s;
    y /= s;
    z /= s;
    t /= s;
    return *this;
  }
  //! Negation operator
  inline Vector4D<S> operator-() const
  {
    return Vector4D<S>(-x, -y, -z, -t);
  }

  //! Get smallest component
  // inline S min() const { return ( x<y ) ? ( ( x<z ) ? x:z ) : ( ( y<z ) ? y:z ); }
  //! Get biggest component
  // inline S max() const { return ( x>y ) ? ( ( x>z ) ? x:z ) : ( ( y>z ) ? y:z ); }

  //! Test if all components are zero
  inline bool empty()
  {
    return x == 0 && y == 0 && z == 0 && t == 0;
  }

  //! access operator
  inline S &operator[](unsigned int i)
  {
    return value[i];
  }
  //! constant access operator
  inline const S &operator[](unsigned int i) const
  {
    return value[i];
  }

  //! debug output vector to a string
  std::string toString() const;

  //! test if nans are present
  bool isValid() const;

  //! actual values
  union {
    S value[4];
    struct {
      S x;
      S y;
      S z;
      S t;
    };
    struct {
      S X;
      S Y;
      S Z;
      S T;
    };
  };

  // zero element
  static const Vector4D<S> Zero, Invalid;

 protected:
};

//************************************************************************
// Additional operators
//************************************************************************

//! Addition operator
template<class S> inline Vector4D<S> operator+(const Vector4D<S> &v1, const Vector4D<S> &v2)
{
  return Vector4D<S>(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.t + v2.t);
}
//! Addition operator
template<class S, class S2> inline Vector4D<S> operator+(const Vector4D<S> &v, S2 s)
{
  return Vector4D<S>(v.x + s, v.y + s, v.z + s, v.t + s);
}
//! Addition operator
template<class S, class S2> inline Vector4D<S> operator+(S2 s, const Vector4D<S> &v)
{
  return Vector4D<S>(v.x + s, v.y + s, v.z + s, v.t + s);
}

//! Subtraction operator
template<class S> inline Vector4D<S> operator-(const Vector4D<S> &v1, const Vector4D<S> &v2)
{
  return Vector4D<S>(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.t - v2.t);
}
//! Subtraction operator
template<class S, class S2> inline Vector4D<S> operator-(const Vector4D<S> &v, S2 s)
{
  return Vector4D<S>(v.x - s, v.y - s, v.z - s, v.t - s);
}
//! Subtraction operator
template<class S, class S2> inline Vector4D<S> operator-(S2 s, const Vector4D<S> &v)
{
  return Vector4D<S>(s - v.x, s - v.y, s - v.z, s - v.t);
}

//! Multiplication operator
template<class S> inline Vector4D<S> operator*(const Vector4D<S> &v1, const Vector4D<S> &v2)
{
  return Vector4D<S>(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z, v1.t * v2.t);
}
//! Multiplication operator
template<class S, class S2> inline Vector4D<S> operator*(const Vector4D<S> &v, S2 s)
{
  return Vector4D<S>(v.x * s, v.y * s, v.z * s, v.t * s);
}
//! Multiplication operator
template<class S, class S2> inline Vector4D<S> operator*(S2 s, const Vector4D<S> &v)
{
  return Vector4D<S>(s * v.x, s * v.y, s * v.z, s * v.t);
}

//! Division operator
template<class S> inline Vector4D<S> operator/(const Vector4D<S> &v1, const Vector4D<S> &v2)
{
  return Vector4D<S>(v1.x / v2.x, v1.y / v2.y, v1.z / v2.z, v1.t / v2.t);
}
//! Division operator
template<class S, class S2> inline Vector4D<S> operator/(const Vector4D<S> &v, S2 s)
{
  return Vector4D<S>(v.x / s, v.y / s, v.z / s, v.t / s);
}
//! Division operator
template<class S, class S2> inline Vector4D<S> operator/(S2 s, const Vector4D<S> &v)
{
  return Vector4D<S>(s / v.x, s / v.y, s / v.z, s / v.t);
}

//! Comparison operator
template<class S> inline bool operator==(const Vector4D<S> &s1, const Vector4D<S> &s2)
{
  return s1.x == s2.x && s1.y == s2.y && s1.z == s2.z && s1.t == s2.t;
}

//! Comparison operator
template<class S> inline bool operator!=(const Vector4D<S> &s1, const Vector4D<S> &s2)
{
  return s1.x != s2.x || s1.y != s2.y || s1.z != s2.z || s1.t != s2.t;
}

//************************************************************************
// External functions
//************************************************************************

//! Dot product
template<class S> inline S dot(const Vector4D<S> &t, const Vector4D<S> &v)
{
  return t.x * v.x + t.y * v.y + t.z * v.z + t.t * v.t;
}

//! Cross product
/*template<class S>
inline Vector4D<S> cross ( const Vector4D<S> &t, const Vector4D<S> &v ) {
  NYI Vector4D<S> cp (
    ( ( t.y*v.z ) - ( t.z*v.y ) ),
    ( ( t.z*v.x ) - ( t.x*v.z ) ),
    ( ( t.x*v.y ) - ( t.y*v.x ) ) );
  return cp;
}*/

//! Compute the magnitude (length) of the vector
template<class S> inline S norm(const Vector4D<S> &v)
{
  S l = v.x * v.x + v.y * v.y + v.z * v.z + v.t * v.t;
  return (fabs(l - 1.) < VECTOR_EPSILON * VECTOR_EPSILON) ? 1. : sqrt(l);
}

//! Compute squared magnitude
template<class S> inline S normSquare(const Vector4D<S> &v)
{
  return v.x * v.x + v.y * v.y + v.z * v.z + v.t * v.t;
}

//! Returns a normalized vector
template<class S> inline Vector4D<S> getNormalized(const Vector4D<S> &v)
{
  S l = v.x * v.x + v.y * v.y + v.z * v.z + v.t * v.t;
  if (fabs(l - 1.) < VECTOR_EPSILON * VECTOR_EPSILON)
    return v; /* normalized "enough"... */
  else if (l > VECTOR_EPSILON * VECTOR_EPSILON) {
    S fac = 1. / sqrt(l);
    return Vector4D<S>(v.x * fac, v.y * fac, v.z * fac, v.t * fac);
  }
  else
    return Vector4D<S>((S)0);
}

//! Compute the norm of the vector and normalize it.
/*! \return The value of the norm */
template<class S> inline S normalize(Vector4D<S> &v)
{
  S norm;
  S l = v.x * v.x + v.y * v.y + v.z * v.z + v.t * v.t;
  if (fabs(l - 1.) < VECTOR_EPSILON * VECTOR_EPSILON) {
    norm = 1.;
  }
  else if (l > VECTOR_EPSILON * VECTOR_EPSILON) {
    norm = sqrt(l);
    v *= 1. / norm;
  }
  else {
    v = Vector4D<S>::Zero;
    norm = 0.;
  }
  return (S)norm;
}

//! Outputs the object in human readable form as string
template<class S> std::string Vector4D<S>::toString() const
{
  char buf[256];
  snprintf(buf,
           256,
           "[%+4.6f,%+4.6f,%+4.6f,%+4.6f]",
           (double)(*this)[0],
           (double)(*this)[1],
           (double)(*this)[2],
           (double)(*this)[3]);
  // for debugging, optionally increase precision:
  // snprintf ( buf,256,"[%+4.16f,%+4.16f,%+4.16f,%+4.16f]", ( double ) ( *this ) [0], ( double ) (
  // *this ) [1], ( double ) ( *this ) [2], ( double ) ( *this ) [3] );
  return std::string(buf);
}

template<> std::string Vector4D<int>::toString() const;

//! Outputs the object in human readable form to stream
template<class S> std::ostream &operator<<(std::ostream &os, const Vector4D<S> &i)
{
  os << i.toString();
  return os;
}

//! Reads the contents of the object from a stream
template<class S> std::istream &operator>>(std::istream &is, Vector4D<S> &i)
{
  char c;
  char dummy[4];
  is >> c >> i[0] >> dummy >> i[1] >> dummy >> i[2] >> dummy >> i[3] >> c;
  return is;
}

/**************************************************************************/
// Define default vector alias
/**************************************************************************/

//! 3D vector class of type Real (typically float)
typedef Vector4D<Real> Vec4;

//! 3D vector class of type int
typedef Vector4D<int> Vec4i;

//! convert to Real Vector
template<class T> inline Vec4 toVec4(T v)
{
  return Vec4(v[0], v[1], v[2], v[3]);
}
template<class T> inline Vec4i toVec4i(T v)
{
  return Vec4i(v[0], v[1], v[2], v[3]);
}

/**************************************************************************/
// Specializations for common math functions
/**************************************************************************/

template<> inline Vec4 clamp<Vec4>(const Vec4 &a, const Vec4 &b, const Vec4 &c)
{
  return Vec4(
      clamp(a.x, b.x, c.x), clamp(a.y, b.y, c.y), clamp(a.z, b.z, c.z), clamp(a.t, b.t, c.t));
}
template<> inline Vec4 safeDivide<Vec4>(const Vec4 &a, const Vec4 &b)
{
  return Vec4(
      safeDivide(a.x, b.x), safeDivide(a.y, b.y), safeDivide(a.z, b.z), safeDivide(a.t, b.t));
}
template<> inline Vec4 nmod<Vec4>(const Vec4 &a, const Vec4 &b)
{
  return Vec4(nmod(a.x, b.x), nmod(a.y, b.y), nmod(a.z, b.z), nmod(a.t, b.t));
}

/**************************************************************************/
// 4d interpolation (note only 4d here, 2d/3d interpolations are in interpol.h)
/**************************************************************************/

#define BUILD_INDEX_4D \
  Real px = pos.x - 0.5f, py = pos.y - 0.5f, pz = pos.z - 0.5f, pt = pos.t - 0.5f; \
  int xi = (int)px; \
  int yi = (int)py; \
  int zi = (int)pz; \
  int ti = (int)pt; \
  Real s1 = px - (Real)xi, s0 = 1. - s1; \
  Real t1 = py - (Real)yi, t0 = 1. - t1; \
  Real f1 = pz - (Real)zi, f0 = 1. - f1; \
  Real g1 = pt - (Real)ti, g0 = 1. - g1; \
  /* clamp to border */ \
  if (px < 0.) { \
    xi = 0; \
    s0 = 1.0; \
    s1 = 0.0; \
  } \
  if (py < 0.) { \
    yi = 0; \
    t0 = 1.0; \
    t1 = 0.0; \
  } \
  if (pz < 0.) { \
    zi = 0; \
    f0 = 1.0; \
    f1 = 0.0; \
  } \
  if (pt < 0.) { \
    ti = 0; \
    g0 = 1.0; \
    g1 = 0.0; \
  } \
  if (xi >= size.x - 1) { \
    xi = size.x - 2; \
    s0 = 0.0; \
    s1 = 1.0; \
  } \
  if (yi >= size.y - 1) { \
    yi = size.y - 2; \
    t0 = 0.0; \
    t1 = 1.0; \
  } \
  if (zi >= size.z - 1) { \
    zi = size.z - 2; \
    f0 = 0.0; \
    f1 = 1.0; \
  } \
  if (ti >= size.t - 1) { \
    ti = size.t - 2; \
    g0 = 0.0; \
    g1 = 1.0; \
  } \
  const int sX = 1; \
  const int sY = size.x;

static inline void checkIndexInterpol4d(const Vec4i &size, int idx)
{
  if (idx < 0 || idx > size.x * size.y * size.z * size.t) {
    std::ostringstream s;
    s << "Grid interpol4d dim " << size << " : index " << idx << " out of bound ";
    errMsg(s.str());
  }
}

template<class T>
inline T interpol4d(
    const T *data, const Vec4i &size, const IndexInt sZ, const IndexInt sT, const Vec4 &pos)
{
  BUILD_INDEX_4D
  IndexInt idx = (IndexInt)xi + sY * (IndexInt)yi + sZ * (IndexInt)zi + sT * (IndexInt)ti;
  DEBUG_ONLY(checkIndexInterpol4d(size, idx));
  DEBUG_ONLY(checkIndexInterpol4d(size, idx + sX + sY + sZ + sT));

  return (((data[idx] * t0 + data[idx + sY] * t1) * s0 +
           (data[idx + sX] * t0 + data[idx + sX + sY] * t1) * s1) *
              f0 +
          ((data[idx + sZ] * t0 + data[idx + sY + sZ] * t1) * s0 +
           (data[idx + sX + sZ] * t0 + data[idx + sX + sY + sZ] * t1) * s1) *
              f1) *
             g0 +
         (((data[idx + sT] * t0 + data[idx + sT + sY] * t1) * s0 +
           (data[idx + sT + sX] * t0 + data[idx + sT + sX + sY] * t1) * s1) *
              f0 +
          ((data[idx + sT + sZ] * t0 + data[idx + sT + sY + sZ] * t1) * s0 +
           (data[idx + sT + sX + sZ] * t0 + data[idx + sT + sX + sY + sZ] * t1) * s1) *
              f1) *
             g1;
}

};  // namespace Manta

#endif
