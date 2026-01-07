/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 * \brief DNA file which is used for the makesdna_test
 */

#pragma once

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

}  // namespace blender
