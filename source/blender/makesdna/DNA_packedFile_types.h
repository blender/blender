/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PackedFile {
  int size;
  int seek;
  void *data;
} PackedFile;

#ifdef __cplusplus
}
#endif
