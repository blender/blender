// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_NUMTRAITS_H
#define EIGEN_NUMTRAITS_H

/** \class NumTraits
  *
  * \brief Holds some data about the various numeric (i.e. scalar) types allowed by Eigen.
  *
  * \param T the numeric type about which this class provides data. Recall that Eigen allows
  *          only the following types for \a T: \c int, \c float, \c double,
  *          \c std::complex<float>, \c std::complex<double>, and \c long \c double (especially
  *          useful to enforce x87 arithmetics when SSE is the default).
  *
  * The provided data consists of:
  * \li A typedef \a Real, giving the "real part" type of \a T. If \a T is already real,
  *     then \a Real is just a typedef to \a T. If \a T is \c std::complex<U> then \a Real
  *     is a typedef to \a U.
  * \li A typedef \a FloatingPoint, giving the "floating-point type" of \a T. If \a T is
  *     \c int, then \a FloatingPoint is a typedef to \c double. Otherwise, \a FloatingPoint
  *     is a typedef to \a T.
  * \li An enum value \a IsComplex. It is equal to 1 if \a T is a \c std::complex
  *     type, and to 0 otherwise.
  * \li An enum \a HasFloatingPoint. It is equal to \c 0 if \a T is \c int,
  *     and to \c 1 otherwise.
  */
template<typename T> struct NumTraits;

template<> struct NumTraits<int>
{
  typedef int Real;
  typedef double FloatingPoint;
  enum {
    IsComplex = 0,
    HasFloatingPoint = 0,
    ReadCost = 1,
    AddCost = 1,
    MulCost = 1
  };
};

template<> struct NumTraits<float>
{
  typedef float Real;
  typedef float FloatingPoint;
  enum {
    IsComplex = 0,
    HasFloatingPoint = 1,
    ReadCost = 1,
    AddCost = 1,
    MulCost = 1
  };
};

template<> struct NumTraits<double>
{
  typedef double Real;
  typedef double FloatingPoint;
  enum {
    IsComplex = 0,
    HasFloatingPoint = 1,
    ReadCost = 1,
    AddCost = 1,
    MulCost = 1
  };
};

template<typename _Real> struct NumTraits<std::complex<_Real> >
{
  typedef _Real Real;
  typedef std::complex<_Real> FloatingPoint;
  enum {
    IsComplex = 1,
    HasFloatingPoint = NumTraits<Real>::HasFloatingPoint,
    ReadCost = 2,
    AddCost = 2 * NumTraits<Real>::AddCost,
    MulCost = 4 * NumTraits<Real>::MulCost + 2 * NumTraits<Real>::AddCost
  };
};

template<> struct NumTraits<long long int>
{
  typedef long long int Real;
  typedef long double FloatingPoint;
  enum {
    IsComplex = 0,
    HasFloatingPoint = 0,
    ReadCost = 1,
    AddCost = 1,
    MulCost = 1
  };
};

template<> struct NumTraits<long double>
{
  typedef long double Real;
  typedef long double FloatingPoint;
  enum {
    IsComplex = 0,
    HasFloatingPoint = 1,
    ReadCost = 1,
    AddCost = 1,
    MulCost = 1
  };
};

template<> struct NumTraits<bool>
{
  typedef bool Real;
  typedef float FloatingPoint;
  enum {
    IsComplex = 0,
    HasFloatingPoint = 0,
    ReadCost = 1,
    AddCost = 1,
    MulCost = 1
  };
};

#endif // EIGEN_NUMTRAITS_H
