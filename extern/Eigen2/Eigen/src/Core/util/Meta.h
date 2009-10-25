// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_META_H
#define EIGEN_META_H

/** \internal
  * \file Meta.h
  * This file contains generic metaprogramming classes which are not specifically related to Eigen.
  * \note In case you wonder, yes we're aware that Boost already provides all these features,
  * we however don't want to add a dependency to Boost.
  */

struct ei_meta_true {  enum { ret = 1 }; };
struct ei_meta_false { enum { ret = 0 }; };

template<bool Condition, typename Then, typename Else>
struct ei_meta_if { typedef Then ret; };

template<typename Then, typename Else>
struct ei_meta_if <false, Then, Else> { typedef Else ret; };

template<typename T, typename U> struct ei_is_same_type { enum { ret = 0 }; };
template<typename T> struct ei_is_same_type<T,T> { enum { ret = 1 }; };

template<typename T> struct ei_unref { typedef T type; };
template<typename T> struct ei_unref<T&> { typedef T type; };

template<typename T> struct ei_unpointer { typedef T type; };
template<typename T> struct ei_unpointer<T*> { typedef T type; };
template<typename T> struct ei_unpointer<T*const> { typedef T type; };

template<typename T> struct ei_unconst { typedef T type; };
template<typename T> struct ei_unconst<const T> { typedef T type; };
template<typename T> struct ei_unconst<T const &> { typedef T & type; };
template<typename T> struct ei_unconst<T const *> { typedef T * type; };

template<typename T> struct ei_cleantype { typedef T type; };
template<typename T> struct ei_cleantype<const T>   { typedef typename ei_cleantype<T>::type type; };
template<typename T> struct ei_cleantype<const T&>  { typedef typename ei_cleantype<T>::type type; };
template<typename T> struct ei_cleantype<T&>        { typedef typename ei_cleantype<T>::type type; };
template<typename T> struct ei_cleantype<const T*>  { typedef typename ei_cleantype<T>::type type; };
template<typename T> struct ei_cleantype<T*>        { typedef typename ei_cleantype<T>::type type; };

/** \internal
  * Convenient struct to get the result type of a unary or binary functor.
  *
  * It supports both the current STL mechanism (using the result_type member) as well as
  * upcoming next STL generation (using a templated result member).
  * If none of these members is provided, then the type of the first argument is returned. FIXME, that behavior is a pretty bad hack.
  */
template<typename T> struct ei_result_of {};

struct ei_has_none {int a[1];};
struct ei_has_std_result_type {int a[2];};
struct ei_has_tr1_result {int a[3];};

template<typename Func, typename ArgType, int SizeOf=sizeof(ei_has_none)>
struct ei_unary_result_of_select {typedef ArgType type;};

template<typename Func, typename ArgType>
struct ei_unary_result_of_select<Func, ArgType, sizeof(ei_has_std_result_type)> {typedef typename Func::result_type type;};

template<typename Func, typename ArgType>
struct ei_unary_result_of_select<Func, ArgType, sizeof(ei_has_tr1_result)> {typedef typename Func::template result<Func(ArgType)>::type type;};

template<typename Func, typename ArgType>
struct ei_result_of<Func(ArgType)> {
    template<typename T>
    static ei_has_std_result_type testFunctor(T const *, typename T::result_type const * = 0);
    template<typename T>
    static ei_has_tr1_result      testFunctor(T const *, typename T::template result<T(ArgType)>::type const * = 0);
    static ei_has_none            testFunctor(...);

    // note that the following indirection is needed for gcc-3.3
    enum {FunctorType = sizeof(testFunctor(static_cast<Func*>(0)))};
    typedef typename ei_unary_result_of_select<Func, ArgType, FunctorType>::type type;
};

template<typename Func, typename ArgType0, typename ArgType1, int SizeOf=sizeof(ei_has_none)>
struct ei_binary_result_of_select {typedef ArgType0 type;};

template<typename Func, typename ArgType0, typename ArgType1>
struct ei_binary_result_of_select<Func, ArgType0, ArgType1, sizeof(ei_has_std_result_type)>
{typedef typename Func::result_type type;};

template<typename Func, typename ArgType0, typename ArgType1>
struct ei_binary_result_of_select<Func, ArgType0, ArgType1, sizeof(ei_has_tr1_result)>
{typedef typename Func::template result<Func(ArgType0,ArgType1)>::type type;};

template<typename Func, typename ArgType0, typename ArgType1>
struct ei_result_of<Func(ArgType0,ArgType1)> {
    template<typename T>
    static ei_has_std_result_type testFunctor(T const *, typename T::result_type const * = 0);
    template<typename T>
    static ei_has_tr1_result      testFunctor(T const *, typename T::template result<T(ArgType0,ArgType1)>::type const * = 0);
    static ei_has_none            testFunctor(...);

    // note that the following indirection is needed for gcc-3.3
    enum {FunctorType = sizeof(testFunctor(static_cast<Func*>(0)))};
    typedef typename ei_binary_result_of_select<Func, ArgType0, ArgType1, FunctorType>::type type;
};

/** \internal In short, it computes int(sqrt(\a Y)) with \a Y an integer.
  * Usage example: \code ei_meta_sqrt<1023>::ret \endcode
  */
template<int Y,
         int InfX = 0,
         int SupX = ((Y==1) ? 1 : Y/2),
         bool Done = ((SupX-InfX)<=1 ? true : ((SupX*SupX <= Y) && ((SupX+1)*(SupX+1) > Y))) >
                                // use ?: instead of || just to shut up a stupid gcc 4.3 warning
class ei_meta_sqrt
{
    enum {
      MidX = (InfX+SupX)/2,
      TakeInf = MidX*MidX > Y ? 1 : 0,
      NewInf = int(TakeInf) ? InfX : int(MidX),
      NewSup = int(TakeInf) ? int(MidX) : SupX
    };
  public:
    enum { ret = ei_meta_sqrt<Y,NewInf,NewSup>::ret };
};

template<int Y, int InfX, int SupX>
class ei_meta_sqrt<Y, InfX, SupX, true> { public:  enum { ret = (SupX*SupX <= Y) ? SupX : InfX }; };

/** \internal determines whether the product of two numeric types is allowed and what the return type is */
template<typename T, typename U> struct ei_scalar_product_traits
{
  // dummy general case where T and U aren't compatible -- not allowed anyway but we catch it elsewhere
  //enum { Cost = NumTraits<T>::MulCost };
  typedef T ReturnType;
};

template<typename T> struct ei_scalar_product_traits<T,T>
{
  //enum { Cost = NumTraits<T>::MulCost };
  typedef T ReturnType;
};

template<typename T> struct ei_scalar_product_traits<T,std::complex<T> >
{
  //enum { Cost = 2*NumTraits<T>::MulCost };
  typedef std::complex<T> ReturnType;
};

template<typename T> struct ei_scalar_product_traits<std::complex<T>, T>
{
  //enum { Cost = 2*NumTraits<T>::MulCost  };
  typedef std::complex<T> ReturnType;
};

// FIXME quick workaround around current limitation of ei_result_of
template<typename Scalar, typename ArgType0, typename ArgType1>
struct ei_result_of<ei_scalar_product_op<Scalar>(ArgType0,ArgType1)> {
typedef typename ei_scalar_product_traits<typename ei_cleantype<ArgType0>::type, typename ei_cleantype<ArgType1>::type>::ReturnType type;
};



#endif // EIGEN_META_H
