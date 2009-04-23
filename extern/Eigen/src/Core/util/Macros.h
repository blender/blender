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

#ifndef EIGEN_MACROS_H
#define EIGEN_MACROS_H

#undef minor

#define EIGEN_WORLD_VERSION 2
#define EIGEN_MAJOR_VERSION 0
#define EIGEN_MINOR_VERSION 1

#define EIGEN_VERSION_AT_LEAST(x,y,z) (EIGEN_WORLD_VERSION>x || (EIGEN_WORLD_VERSION>=x && \
                                      (EIGEN_MAJOR_VERSION>y || (EIGEN_MAJOR_VERSION>=y && \
                                                                 EIGEN_MINOR_VERSION>=z))))

// if the compiler is GNUC, disable 16 byte alignment on exotic archs that probably don't need it, and on which
// it may be extra trouble to get aligned memory allocation to work (example: on ARM, overloading new[] is a PITA
// because extra memory must be allocated for bookkeeping).
// if the compiler is not GNUC, just cross fingers that the architecture isn't too exotic, because we don't want
// to keep track of all the different preprocessor symbols for all compilers.
#if !defined(__GNUC__) || defined(__i386__) || defined(__x86_64__) || defined(__ppc__) || defined(__ia64__)
  #define EIGEN_ARCH_WANTS_ALIGNMENT 1
#else
  #ifdef EIGEN_VECTORIZE
    #error Vectorization enabled, but the architecture is not listed among those for which we require 16 byte alignment. If you added vectorization for another architecture, you also need to edit this list.
  #endif
  #define EIGEN_ARCH_WANTS_ALIGNMENT 0
  #ifndef EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT
    #define EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT
  #endif
#endif


#ifdef EIGEN_DEFAULT_TO_ROW_MAJOR
#define EIGEN_DEFAULT_MATRIX_STORAGE_ORDER_OPTION RowMajor
#else
#define EIGEN_DEFAULT_MATRIX_STORAGE_ORDER_OPTION ColMajor
#endif

/** \internal  Defines the maximal loop size to enable meta unrolling of loops.
  *            Note that the value here is expressed in Eigen's own notion of "number of FLOPS",
  *            it does not correspond to the number of iterations or the number of instructions
  */
#ifndef EIGEN_UNROLLING_LIMIT
#define EIGEN_UNROLLING_LIMIT 100
#endif

/** \internal Define the maximal size in Bytes of blocks fitting in CPU cache.
  * The current value is set to generate blocks of 256x256 for float
  *
  * Typically for a single-threaded application you would set that to 25% of the size of your CPU caches in bytes
  */
#ifndef EIGEN_TUNE_FOR_CPU_CACHE_SIZE
#define EIGEN_TUNE_FOR_CPU_CACHE_SIZE (sizeof(float)*256*256)
#endif

// FIXME this should go away quickly
#ifdef EIGEN_TUNE_FOR_L2_CACHE_SIZE
#error EIGEN_TUNE_FOR_L2_CACHE_SIZE is now called EIGEN_TUNE_FOR_CPU_CACHE_SIZE.
#endif

#define USING_PART_OF_NAMESPACE_EIGEN \
EIGEN_USING_MATRIX_TYPEDEFS \
using Eigen::Matrix; \
using Eigen::MatrixBase; \
using Eigen::ei_random; \
using Eigen::ei_real; \
using Eigen::ei_imag; \
using Eigen::ei_conj; \
using Eigen::ei_abs; \
using Eigen::ei_abs2; \
using Eigen::ei_sqrt; \
using Eigen::ei_exp; \
using Eigen::ei_log; \
using Eigen::ei_sin; \
using Eigen::ei_cos;

#ifdef NDEBUG
# ifndef EIGEN_NO_DEBUG
#  define EIGEN_NO_DEBUG
# endif
#endif

#ifndef ei_assert
#ifdef EIGEN_NO_DEBUG
#define ei_assert(x)
#else
#define ei_assert(x) assert(x)
#endif
#endif

#ifdef EIGEN_INTERNAL_DEBUGGING
#define ei_internal_assert(x) ei_assert(x)
#else
#define ei_internal_assert(x)
#endif

#ifdef EIGEN_NO_DEBUG
#define EIGEN_ONLY_USED_FOR_DEBUG(x) (void)x
#else
#define EIGEN_ONLY_USED_FOR_DEBUG(x)
#endif

// EIGEN_ALWAYS_INLINE_ATTRIB should be use in the declaration of function
// which should be inlined even in debug mode.
// FIXME with the always_inline attribute,
// gcc 3.4.x reports the following compilation error:
//   Eval.h:91: sorry, unimplemented: inlining failed in call to 'const Eigen::Eval<Derived> Eigen::MatrixBase<Scalar, Derived>::eval() const'
//    : function body not available
#if EIGEN_GNUC_AT_LEAST(4,0)
#define EIGEN_ALWAYS_INLINE_ATTRIB __attribute__((always_inline))
#else
#define EIGEN_ALWAYS_INLINE_ATTRIB
#endif

// EIGEN_FORCE_INLINE means "inline as much as possible"
#if (defined _MSC_VER)
#define EIGEN_STRONG_INLINE __forceinline
#else
#define EIGEN_STRONG_INLINE inline
#endif

#if (defined __GNUC__)
#define EIGEN_DONT_INLINE __attribute__((noinline))
#elif (defined _MSC_VER)
#define EIGEN_DONT_INLINE __declspec(noinline)
#else
#define EIGEN_DONT_INLINE
#endif

#if (defined __GNUC__)
#define EIGEN_DEPRECATED __attribute__((deprecated))
#elif (defined _MSC_VER)
#define EIGEN_DEPRECATED __declspec(deprecated)
#else
#define EIGEN_DEPRECATED
#endif

/* EIGEN_ALIGN_128 forces data to be 16-byte aligned, EVEN if vectorization (EIGEN_VECTORIZE) is disabled,
 * so that vectorization doesn't affect binary compatibility.
 *
 * If we made alignment depend on whether or not EIGEN_VECTORIZE is defined, it would be impossible to link
 * vectorized and non-vectorized code.
 */
#if !EIGEN_ARCH_WANTS_ALIGNMENT
#define EIGEN_ALIGN_128
#elif (defined __GNUC__)
#define EIGEN_ALIGN_128 __attribute__((aligned(16)))
#elif (defined _MSC_VER)
#define EIGEN_ALIGN_128 __declspec(align(16))
#else
#error Please tell me what is the equivalent of __attribute__((aligned(16))) for your compiler
#endif

#define EIGEN_RESTRICT __restrict

#ifndef EIGEN_STACK_ALLOCATION_LIMIT
#define EIGEN_STACK_ALLOCATION_LIMIT 16000000
#endif

#ifndef EIGEN_DEFAULT_IO_FORMAT
#define EIGEN_DEFAULT_IO_FORMAT Eigen::IOFormat()
#endif

// format used in Eigen's documentation
// needed to define it here as escaping characters in CMake add_definition's argument seems very problematic.
#define EIGEN_DOCS_IO_FORMAT IOFormat(3, AlignCols, " ", "\n", "", "")

#define EIGEN_INHERIT_ASSIGNMENT_OPERATOR(Derived, Op) \
template<typename OtherDerived> \
EIGEN_STRONG_INLINE Derived& operator Op(const Eigen::MatrixBase<OtherDerived>& other) \
{ \
  return Base::operator Op(other.derived()); \
} \
EIGEN_STRONG_INLINE Derived& operator Op(const Derived& other) \
{ \
  return Base::operator Op(other); \
}

#define EIGEN_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(Derived, Op) \
template<typename Other> \
EIGEN_STRONG_INLINE Derived& operator Op(const Other& scalar) \
{ \
  return Base::operator Op(scalar); \
}

#define EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Derived) \
EIGEN_INHERIT_ASSIGNMENT_OPERATOR(Derived, =) \
EIGEN_INHERIT_ASSIGNMENT_OPERATOR(Derived, +=) \
EIGEN_INHERIT_ASSIGNMENT_OPERATOR(Derived, -=) \
EIGEN_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(Derived, *=) \
EIGEN_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(Derived, /=)

#define _EIGEN_GENERIC_PUBLIC_INTERFACE(Derived, BaseClass) \
typedef BaseClass Base; \
typedef typename Eigen::ei_traits<Derived>::Scalar Scalar; \
typedef typename Eigen::NumTraits<Scalar>::Real RealScalar; \
typedef typename Base::PacketScalar PacketScalar; \
typedef typename Eigen::ei_nested<Derived>::type Nested; \
enum { RowsAtCompileTime = Eigen::ei_traits<Derived>::RowsAtCompileTime, \
       ColsAtCompileTime = Eigen::ei_traits<Derived>::ColsAtCompileTime, \
       MaxRowsAtCompileTime = Eigen::ei_traits<Derived>::MaxRowsAtCompileTime, \
       MaxColsAtCompileTime = Eigen::ei_traits<Derived>::MaxColsAtCompileTime, \
       Flags = Eigen::ei_traits<Derived>::Flags, \
       CoeffReadCost = Eigen::ei_traits<Derived>::CoeffReadCost, \
       SizeAtCompileTime = Base::SizeAtCompileTime, \
       MaxSizeAtCompileTime = Base::MaxSizeAtCompileTime, \
       IsVectorAtCompileTime = Base::IsVectorAtCompileTime };

#define EIGEN_GENERIC_PUBLIC_INTERFACE(Derived) \
_EIGEN_GENERIC_PUBLIC_INTERFACE(Derived, Eigen::MatrixBase<Derived>)

#define EIGEN_ENUM_MIN(a,b) (((int)a <= (int)b) ? (int)a : (int)b)
#define EIGEN_ENUM_MAX(a,b) (((int)a >= (int)b) ? (int)a : (int)b)

#endif // EIGEN_MACROS_H
