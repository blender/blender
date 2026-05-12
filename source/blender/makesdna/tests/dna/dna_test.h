/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 * \brief DNA file which is used for the makesdna_test
 */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender {

struct Library;
struct TestStruct;

/* Structure that is to be skipped from SDNA. */
#
#
struct RuntimeStruct {
  int foo;
};

/* NOTE: This type must exist in order for the SDNA to be considered valid. */
struct ListBase {
  void *first, *last;
};

struct TestStruct {
#ifdef __cplusplus
  TestStruct() = default;
#endif

  void *next, *prev;

  char name[/*MAX_ID_NAME*/ 258];

  short flag;
  int _pad1;

  ListBase some_list;
  int tag;

  int _pad2;
};

struct StructWithVectorMatrixTypes {
  int2 my_int2;
  float3 my_float3;
  float _pad[3];
  float4x4 my_matrix;
};

enum class TestEnum8 : int8_t {
  A = 0,
  B = 1,
};

enum TestEnum16 : int16_t {
  TEST_ENUM16_A = 0,
  TEST_ENUM16_B = 1,
};

enum class TestEnum32 : int32_t {
  A = 0,
  B = 1,
};

struct StructWithEnumMembers {
  TestEnum8 my_enum8;
  char _pad0;
  TestEnum16 my_enum16;
  TestEnum32 my_enum32;
};

}  // namespace blender
