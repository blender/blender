/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * The macros below reduce the boilerplate needed to implement basic equality operators for
 * structs. These macros could be removed starting with C++20, because then we can use defaulted
 * comparison operators: https://en.cppreference.com/w/cpp/language/default_comparisons
 *
 * Using these macros also reduces the probably for common typos, like comparing a value to itself
 * or comparing the same value twice.
 */

#define BLI_STRUCT_DERIVED_UNEQUAL_OPERATOR(Type) \
  friend bool operator!=(const Type &a, const Type &b) \
  { \
    return !(a == b); \
  }

#define BLI_STRUCT_EQUALITY_OPERATORS_1(Type, m) \
  friend bool operator==(const Type &a, const Type &b) \
  { \
    return a.m == b.m; \
  } \
  BLI_STRUCT_DERIVED_UNEQUAL_OPERATOR(Type)

#define BLI_STRUCT_EQUALITY_OPERATORS_2(Type, m1, m2) \
  friend bool operator==(const Type &a, const Type &b) \
  { \
    return a.m1 == b.m1 && a.m2 == b.m2; \
  } \
  BLI_STRUCT_DERIVED_UNEQUAL_OPERATOR(Type)

#define BLI_STRUCT_EQUALITY_OPERATORS_3(Type, m1, m2, m3) \
  friend bool operator==(const Type &a, const Type &b) \
  { \
    return a.m1 == b.m1 && a.m2 == b.m2 && a.m3 == b.m3; \
  } \
  BLI_STRUCT_DERIVED_UNEQUAL_OPERATOR(Type)

#define BLI_STRUCT_EQUALITY_OPERATORS_4(Type, m1, m2, m3, m4) \
  friend bool operator==(const Type &a, const Type &b) \
  { \
    return a.m1 == b.m1 && a.m2 == b.m2 && a.m3 == b.m3 && a.m4 == b.m4; \
  } \
  BLI_STRUCT_DERIVED_UNEQUAL_OPERATOR(Type)

#define BLI_STRUCT_EQUALITY_OPERATORS_5(Type, m1, m2, m3, m4, m5) \
  friend bool operator==(const Type &a, const Type &b) \
  { \
    return a.m1 == b.m1 && a.m2 == b.m2 && a.m3 == b.m3 && a.m4 == b.m4 && a.m5 == b.m5; \
  } \
  BLI_STRUCT_DERIVED_UNEQUAL_OPERATOR(Type)
