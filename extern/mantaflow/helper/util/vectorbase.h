/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011-2016 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Basic vector class
 *
 ******************************************************************************/

#ifndef _VECTORBASE_H
#define _VECTORBASE_H

// get rid of windos min/max defines
#if (defined(WIN32) || defined(_WIN32)) && !defined(NOMINMAX)
#  define NOMINMAX
#endif

#include <stdio.h>
#include <string>
#include <limits>
#include <iostream>
#include "general.h"

// if min/max are still around...
#if defined(WIN32) || defined(_WIN32)
#  undef min
#  undef max
#endif

// redefine usage of some windows functions
#if defined(WIN32) || defined(_WIN32)
#  ifndef snprintf
#    define snprintf _snprintf
#  endif
#endif

// use which fp-precision? 1=float, 2=double
#ifndef FLOATINGPOINT_PRECISION
#  define FLOATINGPOINT_PRECISION 1
#endif

// VECTOR_EPSILON is the minimal vector length
// In order to be able to discriminate floating point values near zero, and
// to be sure not to fail a comparison because of roundoff errors, use this
// value as a threshold.
#if FLOATINGPOINT_PRECISION == 1
typedef float Real;
#  define VECTOR_EPSILON (1e-6f)
#else
typedef double Real;
#  define VECTOR_EPSILON (1e-10)
#endif

#ifndef M_PI
#  define M_PI 3.1415926536
#endif
#ifndef M_E
#  define M_E 2.7182818284
#endif

namespace Manta {

//! Basic inlined vector class
template<class S> class Vector3D {
 public:
  //! Constructor
  inline Vector3D() : x(0), y(0), z(0)
  {
  }

  //! Copy-Constructor
  inline Vector3D(const Vector3D<S> &v) : x(v.x), y(v.y), z(v.z)
  {
  }

  //! Copy-Constructor
  inline Vector3D(const int *v) : x((S)v[0]), y((S)v[1]), z((S)v[2])
  {
  }

  //! Copy-Constructor
  inline Vector3D(const float *v) : x((S)v[0]), y((S)v[1]), z((S)v[2])
  {
  }

  //! Copy-Constructor
  inline Vector3D(const double *v) : x((S)v[0]), y((S)v[1]), z((S)v[2])
  {
  }

  //! Construct a vector from one S
  inline Vector3D(S v) : x(v), y(v), z(v)
  {
  }

  //! Construct a vector from three Ss
  inline Vector3D(S vx, S vy, S vz) : x(vx), y(vy), z(vz)
  {
  }

  // Operators

  //! Assignment operator
  inline const Vector3D<S> &operator=(const Vector3D<S> &v)
  {
    x = v.x;
    y = v.y;
    z = v.z;
    return *this;
  }
  //! Assignment operator
  inline const Vector3D<S> &operator=(S s)
  {
    x = y = z = s;
    return *this;
  }
  //! Assign and add operator
  inline const Vector3D<S> &operator+=(const Vector3D<S> &v)
  {
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
  }
  //! Assign and add operator
  inline const Vector3D<S> &operator+=(S s)
  {
    x += s;
    y += s;
    z += s;
    return *this;
  }
  //! Assign and sub operator
  inline const Vector3D<S> &operator-=(const Vector3D<S> &v)
  {
    x -= v.x;
    y -= v.y;
    z -= v.z;
    return *this;
  }
  //! Assign and sub operator
  inline const Vector3D<S> &operator-=(S s)
  {
    x -= s;
    y -= s;
    z -= s;
    return *this;
  }
  //! Assign and mult operator
  inline const Vector3D<S> &operator*=(const Vector3D<S> &v)
  {
    x *= v.x;
    y *= v.y;
    z *= v.z;
    return *this;
  }
  //! Assign and mult operator
  inline const Vector3D<S> &operator*=(S s)
  {
    x *= s;
    y *= s;
    z *= s;
    return *this;
  }
  //! Assign and div operator
  inline const Vector3D<S> &operator/=(const Vector3D<S> &v)
  {
    x /= v.x;
    y /= v.y;
    z /= v.z;
    return *this;
  }
  //! Assign and div operator
  inline const Vector3D<S> &operator/=(S s)
  {
    x /= s;
    y /= s;
    z /= s;
    return *this;
  }
  //! Negation operator
  inline Vector3D<S> operator-() const
  {
    return Vector3D<S>(-x, -y, -z);
  }

  //! Get smallest component
  inline S min() const
  {
    return (x < y) ? ((x < z) ? x : z) : ((y < z) ? y : z);
  }
  //! Get biggest component
  inline S max() const
  {
    return (x > y) ? ((x > z) ? x : z) : ((y > z) ? y : z);
  }

  //! Test if all components are zero
  inline bool empty()
  {
    return x == 0 && y == 0 && z == 0;
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
    S value[3];
    struct {
      S x;
      S y;
      S z;
    };
    struct {
      S X;
      S Y;
      S Z;
    };
  };

  //! zero element
  static const Vector3D<S> Zero, Invalid;

  //! For compatibility with 4d vectors (discards 4th comp)
  inline Vector3D(S vx, S vy, S vz, S vDummy) : x(vx), y(vy), z(vz)
  {
  }

 protected:
};

//! helper to check whether value is non-zero
template<class S> inline bool notZero(S v)
{
  return (std::abs(v) > VECTOR_EPSILON);
}
template<class S> inline bool notZero(Vector3D<S> v)
{
  return (std::abs(norm(v)) > VECTOR_EPSILON);
}

//************************************************************************
// Additional operators
//************************************************************************

//! Addition operator
template<class S> inline Vector3D<S> operator+(const Vector3D<S> &v1, const Vector3D<S> &v2)
{
  return Vector3D<S>(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
}
//! Addition operator
template<class S, class S2> inline Vector3D<S> operator+(const Vector3D<S> &v, S2 s)
{
  return Vector3D<S>(v.x + s, v.y + s, v.z + s);
}
//! Addition operator
template<class S, class S2> inline Vector3D<S> operator+(S2 s, const Vector3D<S> &v)
{
  return Vector3D<S>(v.x + s, v.y + s, v.z + s);
}

//! Subtraction operator
template<class S> inline Vector3D<S> operator-(const Vector3D<S> &v1, const Vector3D<S> &v2)
{
  return Vector3D<S>(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}
//! Subtraction operator
template<class S, class S2> inline Vector3D<S> operator-(const Vector3D<S> &v, S2 s)
{
  return Vector3D<S>(v.x - s, v.y - s, v.z - s);
}
//! Subtraction operator
template<class S, class S2> inline Vector3D<S> operator-(S2 s, const Vector3D<S> &v)
{
  return Vector3D<S>(s - v.x, s - v.y, s - v.z);
}

//! Multiplication operator
template<class S> inline Vector3D<S> operator*(const Vector3D<S> &v1, const Vector3D<S> &v2)
{
  return Vector3D<S>(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z);
}
//! Multiplication operator
template<class S, class S2> inline Vector3D<S> operator*(const Vector3D<S> &v, S2 s)
{
  return Vector3D<S>(v.x * s, v.y * s, v.z * s);
}
//! Multiplication operator
template<class S, class S2> inline Vector3D<S> operator*(S2 s, const Vector3D<S> &v)
{
  return Vector3D<S>(s * v.x, s * v.y, s * v.z);
}

//! Division operator
template<class S> inline Vector3D<S> operator/(const Vector3D<S> &v1, const Vector3D<S> &v2)
{
  return Vector3D<S>(v1.x / v2.x, v1.y / v2.y, v1.z / v2.z);
}
//! Division operator
template<class S, class S2> inline Vector3D<S> operator/(const Vector3D<S> &v, S2 s)
{
  return Vector3D<S>(v.x / s, v.y / s, v.z / s);
}
//! Division operator
template<class S, class S2> inline Vector3D<S> operator/(S2 s, const Vector3D<S> &v)
{
  return Vector3D<S>(s / v.x, s / v.y, s / v.z);
}

//! Comparison operator
template<class S> inline bool operator==(const Vector3D<S> &s1, const Vector3D<S> &s2)
{
  return s1.x == s2.x && s1.y == s2.y && s1.z == s2.z;
}

//! Comparison operator
template<class S> inline bool operator!=(const Vector3D<S> &s1, const Vector3D<S> &s2)
{
  return s1.x != s2.x || s1.y != s2.y || s1.z != s2.z;
}

//************************************************************************
// External functions
//************************************************************************

//! Min operator
template<class S> inline Vector3D<S> vmin(const Vector3D<S> &s1, const Vector3D<S> &s2)
{
  return Vector3D<S>(std::min(s1.x, s2.x), std::min(s1.y, s2.y), std::min(s1.z, s2.z));
}

//! Min operator
template<class S, class S2> inline Vector3D<S> vmin(const Vector3D<S> &s1, S2 s2)
{
  return Vector3D<S>(std::min(s1.x, s2), std::min(s1.y, s2), std::min(s1.z, s2));
}

//! Min operator
template<class S1, class S> inline Vector3D<S> vmin(S1 s1, const Vector3D<S> &s2)
{
  return Vector3D<S>(std::min(s1, s2.x), std::min(s1, s2.y), std::min(s1, s2.z));
}

//! Max operator
template<class S> inline Vector3D<S> vmax(const Vector3D<S> &s1, const Vector3D<S> &s2)
{
  return Vector3D<S>(std::max(s1.x, s2.x), std::max(s1.y, s2.y), std::max(s1.z, s2.z));
}

//! Max operator
template<class S, class S2> inline Vector3D<S> vmax(const Vector3D<S> &s1, S2 s2)
{
  return Vector3D<S>(std::max(s1.x, s2), std::max(s1.y, s2), std::max(s1.z, s2));
}

//! Max operator
template<class S1, class S> inline Vector3D<S> vmax(S1 s1, const Vector3D<S> &s2)
{
  return Vector3D<S>(std::max(s1, s2.x), std::max(s1, s2.y), std::max(s1, s2.z));
}

//! Dot product
template<class S> inline S dot(const Vector3D<S> &t, const Vector3D<S> &v)
{
  return t.x * v.x + t.y * v.y + t.z * v.z;
}

//! Cross product
template<class S> inline Vector3D<S> cross(const Vector3D<S> &t, const Vector3D<S> &v)
{
  Vector3D<S> cp(
      ((t.y * v.z) - (t.z * v.y)), ((t.z * v.x) - (t.x * v.z)), ((t.x * v.y) - (t.y * v.x)));
  return cp;
}

//! Project a vector into a plane, defined by its normal
/*! Projects a vector into a plane normal to the given vector, which must
  have unit length. Self is modified.
  \param v The vector to project
  \param n The plane normal
  \return The projected vector */
template<class S>
inline const Vector3D<S> &projectNormalTo(const Vector3D<S> &v, const Vector3D<S> &n)
{
  S sprod = dot(v, n);
  return v - n * dot(v, n);
}

//! Compute the magnitude (length) of the vector
//! (clamps to 0 and 1 with VECTOR_EPSILON)
template<class S> inline S norm(const Vector3D<S> &v)
{
  S l = v.x * v.x + v.y * v.y + v.z * v.z;
  if (l <= VECTOR_EPSILON * VECTOR_EPSILON)
    return (0.);
  return (fabs(l - 1.) < VECTOR_EPSILON * VECTOR_EPSILON) ? 1. : sqrt(l);
}

//! Compute squared magnitude
template<class S> inline S normSquare(const Vector3D<S> &v)
{
  return v.x * v.x + v.y * v.y + v.z * v.z;
}

//! compatibility, allow use of int, Real and Vec inputs with norm/normSquare
inline Real norm(const Real v)
{
  return fabs(v);
}
inline Real normSquare(const Real v)
{
  return square(v);
}
inline Real norm(const int v)
{
  return abs(v);
}
inline Real normSquare(const int v)
{
  return square(v);
}

//! Compute sum of all components, allow use of int, Real too
template<class S> inline S sum(const S v)
{
  return v;
}
template<class S> inline S sum(const Vector3D<S> &v)
{
  return v.x + v.y + v.z;
}

//! Get absolute representation of vector, allow use of int, Real too
inline Real abs(const Real v)
{
  return std::fabs(v);
}
inline int abs(const int v)
{
  return std::abs(v);
}

template<class S> inline Vector3D<S> abs(const Vector3D<S> &v)
{
  Vector3D<S> cp(v.x, v.y, v.z);
  for (int i = 0; i < 3; ++i) {
    if (cp[i] < 0)
      cp[i] *= (-1.0);
  }
  return cp;
}

//! Returns a normalized vector
template<class S> inline Vector3D<S> getNormalized(const Vector3D<S> &v)
{
  S l = v.x * v.x + v.y * v.y + v.z * v.z;
  if (fabs(l - 1.) < VECTOR_EPSILON * VECTOR_EPSILON)
    return v; /* normalized "enough"... */
  else if (l > VECTOR_EPSILON * VECTOR_EPSILON) {
    S fac = 1. / sqrt(l);
    return Vector3D<S>(v.x * fac, v.y * fac, v.z * fac);
  }
  else
    return Vector3D<S>((S)0);
}

//! Compute the norm of the vector and normalize it.
/*! \return The value of the norm */
template<class S> inline S normalize(Vector3D<S> &v)
{
  S norm;
  S l = v.x * v.x + v.y * v.y + v.z * v.z;
  if (fabs(l - 1.) < VECTOR_EPSILON * VECTOR_EPSILON) {
    norm = 1.;
  }
  else if (l > VECTOR_EPSILON * VECTOR_EPSILON) {
    norm = sqrt(l);
    v *= 1. / norm;
  }
  else {
    v = Vector3D<S>::Zero;
    norm = 0.;
  }
  return (S)norm;
}

//! Obtain an orthogonal vector
/*! Compute a vector that is orthonormal to the given vector.
 *  Nothing else can be assumed for the direction of the new vector.
 *  \return The orthonormal vector */
template<class S> Vector3D<S> getOrthogonalVector(const Vector3D<S> &v)
{
  // Determine the  component with max. absolute value
  int maxIndex = (fabs(v.x) > fabs(v.y)) ? 0 : 1;
  maxIndex = (fabs(v[maxIndex]) > fabs(v.z)) ? maxIndex : 2;

  // Choose another axis than the one with max. component and project
  // orthogonal to self
  Vector3D<S> o(0.0);
  o[(maxIndex + 1) % 3] = 1;

  Vector3D<S> c = cross(v, o);
  normalize(c);
  return c;
}

//! Convert vector to polar coordinates
/*! Stable vector to angle conversion
 *\param v vector to convert
  \param phi unique angle [0,2PI]
  \param theta unique angle [0,PI]
 */
template<class S> inline void vecToAngle(const Vector3D<S> &v, S &phi, S &theta)
{
  if (fabs(v.y) < VECTOR_EPSILON)
    theta = M_PI / 2;
  else if (fabs(v.x) < VECTOR_EPSILON && fabs(v.z) < VECTOR_EPSILON)
    theta = (v.y >= 0) ? 0 : M_PI;
  else
    theta = atan(sqrt(v.x * v.x + v.z * v.z) / v.y);
  if (theta < 0)
    theta += M_PI;

  if (fabs(v.x) < VECTOR_EPSILON)
    phi = M_PI / 2;
  else
    phi = atan(v.z / v.x);
  if (phi < 0)
    phi += M_PI;
  if (fabs(v.z) < VECTOR_EPSILON)
    phi = (v.x >= 0) ? 0 : M_PI;
  else if (v.z < 0)
    phi += M_PI;
}

//! Compute vector reflected at a surface
/*! Compute a vector, that is self (as an incoming vector)
 * reflected at a surface with a distinct normal vector.
 * Note that the normal is reversed, if the scalar product with it is positive.
  \param t The incoming vector
  \param n The surface normal
  \return The new reflected vector
  */
template<class S> inline Vector3D<S> reflectVector(const Vector3D<S> &t, const Vector3D<S> &n)
{
  Vector3D<S> nn = (dot(t, n) > 0.0) ? (n * -1.0) : n;
  return (t - nn * (2.0 * dot(nn, t)));
}

//! Compute vector refracted at a surface
/*! \param t The incoming vector
 *  \param n The surface normal
 *  \param nt The "inside" refraction index
 *  \param nair The "outside" refraction index
 *  \param refRefl Set to 1 on total reflection
 *  \return The refracted vector
 */
template<class S>
inline Vector3D<S> refractVector(
    const Vector3D<S> &t, const Vector3D<S> &normal, S nt, S nair, int &refRefl)
{
  // from Glassner's book, section 5.2 (Heckberts method)
  S eta = nair / nt;
  S n = -dot(t, normal);
  S tt = 1.0 + eta * eta * (n * n - 1.0);
  if (tt < 0.0) {
    // we have total reflection!
    refRefl = 1;
  }
  else {
    // normal reflection
    tt = eta * n - sqrt(tt);
    return (t * eta + normal * tt);
  }
  return t;
}

//! Outputs the object in human readable form as string
template<class S> std::string Vector3D<S>::toString() const
{
  char buf[256];
  snprintf(buf,
           256,
           "[%+4.6f,%+4.6f,%+4.6f]",
           (double)(*this)[0],
           (double)(*this)[1],
           (double)(*this)[2]);
  // for debugging, optionally increase precision:
  // snprintf ( buf,256,"[%+4.16f,%+4.16f,%+4.16f]", ( double ) ( *this ) [0], ( double ) ( *this )
  // [1], ( double ) ( *this ) [2] );
  return std::string(buf);
}

template<> std::string Vector3D<int>::toString() const;

//! Outputs the object in human readable form to stream
/*! Output format [x,y,z] */
template<class S> std::ostream &operator<<(std::ostream &os, const Vector3D<S> &i)
{
  os << i.toString();
  return os;
}

//! Reads the contents of the object from a stream
/*! Input format [x,y,z] */
template<class S> std::istream &operator>>(std::istream &is, Vector3D<S> &i)
{
  char c;
  char dummy[3];
  is >> c >> i[0] >> dummy >> i[1] >> dummy >> i[2] >> c;
  return is;
}

/**************************************************************************/
// Define default vector alias
/**************************************************************************/

//! 3D vector class of type Real (typically float)
typedef Vector3D<Real> Vec3;

//! 3D vector class of type int
typedef Vector3D<int> Vec3i;

//! convert to Real Vector
template<class T> inline Vec3 toVec3(T v)
{
  return Vec3(v[0], v[1], v[2]);
}

//! convert to int Vector
template<class T> inline Vec3i toVec3i(T v)
{
  return Vec3i((int)v[0], (int)v[1], (int)v[2]);
}

//! convert to int Vector
template<class T> inline Vec3i toVec3i(T v0, T v1, T v2)
{
  return Vec3i((int)v0, (int)v1, (int)v2);
}

//! round, and convert to int Vector
template<class T> inline Vec3i toVec3iRound(T v)
{
  return Vec3i((int)round(v[0]), (int)round(v[1]), (int)round(v[2]));
}

//! convert to int Vector if values are close enough to an int
template<class T> inline Vec3i toVec3iChecked(T v)
{
  Vec3i ret;
  for (size_t i = 0; i < 3; i++) {
    Real a = v[i];
    if (fabs(a - floor(a + 0.5)) > 1e-5)
      errMsg("argument is not an int, cannot convert");
    ret[i] = (int)(a + 0.5);
  }
  return ret;
}

//! convert to double Vector
template<class T> inline Vector3D<double> toVec3d(T v)
{
  return Vector3D<double>(v[0], v[1], v[2]);
}

//! convert to float Vector
template<class T> inline Vector3D<float> toVec3f(T v)
{
  return Vector3D<float>(v[0], v[1], v[2]);
}

/**************************************************************************/
// Specializations for common math functions
/**************************************************************************/

template<> inline Vec3 clamp<Vec3>(const Vec3 &a, const Vec3 &b, const Vec3 &c)
{
  return Vec3(clamp(a.x, b.x, c.x), clamp(a.y, b.y, c.y), clamp(a.z, b.z, c.z));
}
template<> inline Vec3 safeDivide<Vec3>(const Vec3 &a, const Vec3 &b)
{
  return Vec3(safeDivide(a.x, b.x), safeDivide(a.y, b.y), safeDivide(a.z, b.z));
}
template<> inline Vec3 nmod<Vec3>(const Vec3 &a, const Vec3 &b)
{
  return Vec3(nmod(a.x, b.x), nmod(a.y, b.y), nmod(a.z, b.z));
}

};  // namespace Manta

#endif
