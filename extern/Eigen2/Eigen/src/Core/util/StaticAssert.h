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

#ifndef EIGEN_STATIC_ASSERT_H
#define EIGEN_STATIC_ASSERT_H

/* Some notes on Eigen's static assertion mechanism:
 *
 *  - in EIGEN_STATIC_ASSERT(CONDITION,MSG) the parameter CONDITION must be a compile time boolean
 *    expression, and MSG an enum listed in struct ei_static_assert<true>
 *
 *  - define EIGEN_NO_STATIC_ASSERT to disable them (and save compilation time)
 *    in that case, the static assertion is converted to the following runtime assert:
 *      ei_assert(CONDITION && "MSG")
 *
 *  - currently EIGEN_STATIC_ASSERT can only be used in function scope
 *
 */

#ifndef EIGEN_NO_STATIC_ASSERT

  #ifdef __GXX_EXPERIMENTAL_CXX0X__

    // if native static_assert is enabled, let's use it
    #define EIGEN_STATIC_ASSERT(X,MSG) static_assert(X,#MSG);

  #else // CXX0X

    template<bool condition>
    struct ei_static_assert {};

    template<>
    struct ei_static_assert<true>
    {
      enum {
        YOU_TRIED_CALLING_A_VECTOR_METHOD_ON_A_MATRIX,
        YOU_MIXED_VECTORS_OF_DIFFERENT_SIZES,
        YOU_MIXED_MATRICES_OF_DIFFERENT_SIZES,
        THIS_METHOD_IS_ONLY_FOR_VECTORS_OF_A_SPECIFIC_SIZE,
        THIS_METHOD_IS_ONLY_FOR_MATRICES_OF_A_SPECIFIC_SIZE,
        YOU_MADE_A_PROGRAMMING_MISTAKE,
        YOU_CALLED_A_FIXED_SIZE_METHOD_ON_A_DYNAMIC_SIZE_MATRIX_OR_VECTOR,
        UNALIGNED_LOAD_AND_STORE_OPERATIONS_UNIMPLEMENTED_ON_ALTIVEC,
        NUMERIC_TYPE_MUST_BE_FLOATING_POINT,
        COEFFICIENT_WRITE_ACCESS_TO_SELFADJOINT_NOT_SUPPORTED,
        WRITING_TO_TRIANGULAR_PART_WITH_UNIT_DIAGONAL_IS_NOT_SUPPORTED,
        THIS_METHOD_IS_ONLY_FOR_FIXED_SIZE,
        INVALID_MATRIX_PRODUCT,
        INVALID_VECTOR_VECTOR_PRODUCT__IF_YOU_WANTED_A_DOT_OR_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTIONS,
        INVALID_MATRIX_PRODUCT__IF_YOU_WANTED_A_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTION,
        YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY,
        THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES,
        THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES,
        INVALID_MATRIX_TEMPLATE_PARAMETERS
      };
    };

    // Specialized implementation for MSVC to avoid "conditional
    // expression is constant" warnings.  This implementation doesn't
    // appear to work under GCC, hence the multiple implementations.
    #ifdef _MSC_VER

      #define EIGEN_STATIC_ASSERT(CONDITION,MSG) \
        {Eigen::ei_static_assert<CONDITION ? true : false>::MSG;}

    #else

      #define EIGEN_STATIC_ASSERT(CONDITION,MSG) \
        if (Eigen::ei_static_assert<CONDITION ? true : false>::MSG) {}

    #endif

  #endif // not CXX0X

#else // EIGEN_NO_STATIC_ASSERT

  #define EIGEN_STATIC_ASSERT(CONDITION,MSG) ei_assert((CONDITION) && #MSG);

#endif // EIGEN_NO_STATIC_ASSERT


// static assertion failing if the type \a TYPE is not a vector type
#define EIGEN_STATIC_ASSERT_VECTOR_ONLY(TYPE) \
  EIGEN_STATIC_ASSERT(TYPE::IsVectorAtCompileTime, \
                      YOU_TRIED_CALLING_A_VECTOR_METHOD_ON_A_MATRIX)

// static assertion failing if the type \a TYPE is not fixed-size
#define EIGEN_STATIC_ASSERT_FIXED_SIZE(TYPE) \
  EIGEN_STATIC_ASSERT(TYPE::SizeAtCompileTime!=Eigen::Dynamic, \
                      YOU_CALLED_A_FIXED_SIZE_METHOD_ON_A_DYNAMIC_SIZE_MATRIX_OR_VECTOR)

// static assertion failing if the type \a TYPE is not a vector type of the given size
#define EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(TYPE, SIZE) \
  EIGEN_STATIC_ASSERT(TYPE::IsVectorAtCompileTime && TYPE::SizeAtCompileTime==SIZE, \
                      THIS_METHOD_IS_ONLY_FOR_VECTORS_OF_A_SPECIFIC_SIZE)

// static assertion failing if the type \a TYPE is not a vector type of the given size
#define EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(TYPE, ROWS, COLS) \
  EIGEN_STATIC_ASSERT(TYPE::RowsAtCompileTime==ROWS && TYPE::ColsAtCompileTime==COLS, \
                      THIS_METHOD_IS_ONLY_FOR_MATRICES_OF_A_SPECIFIC_SIZE)

// static assertion failing if the two vector expression types are not compatible (same fixed-size or dynamic size)
#define EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(TYPE0,TYPE1) \
  EIGEN_STATIC_ASSERT( \
      (int(TYPE0::SizeAtCompileTime)==Eigen::Dynamic \
    || int(TYPE1::SizeAtCompileTime)==Eigen::Dynamic \
    || int(TYPE0::SizeAtCompileTime)==int(TYPE1::SizeAtCompileTime)),\
    YOU_MIXED_VECTORS_OF_DIFFERENT_SIZES)

#define EIGEN_PREDICATE_SAME_MATRIX_SIZE(TYPE0,TYPE1) \
      ((int(TYPE0::RowsAtCompileTime)==Eigen::Dynamic \
    || int(TYPE1::RowsAtCompileTime)==Eigen::Dynamic \
    || int(TYPE0::RowsAtCompileTime)==int(TYPE1::RowsAtCompileTime)) \
   && (int(TYPE0::ColsAtCompileTime)==Eigen::Dynamic \
    || int(TYPE1::ColsAtCompileTime)==Eigen::Dynamic \
    || int(TYPE0::ColsAtCompileTime)==int(TYPE1::ColsAtCompileTime)))

// static assertion failing if it is guaranteed at compile-time that the two matrix expression types have different sizes
#define EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(TYPE0,TYPE1) \
  EIGEN_STATIC_ASSERT( \
     EIGEN_PREDICATE_SAME_MATRIX_SIZE(TYPE0,TYPE1),\
    YOU_MIXED_MATRICES_OF_DIFFERENT_SIZES)

#endif // EIGEN_STATIC_ASSERT_H
