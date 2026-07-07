/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include <Python.h>

#include "../generic/py_capi_utils.hh"

namespace blender {

extern struct PyC_StringEnumItems bpygpu_primtype_items[];
extern struct PyC_StringEnumItems bpygpu_dataformat_items[];

/* Doc-string Literal types for shared enums. */

#define PYDOC_PRIMTYPE_LITERAL \
  "Literal[" \
  "'POINTS', " \
  "'LINES', " \
  "'TRIS', " \
  "'LINE_STRIP', " \
  "'LINE_LOOP', " \
  "'TRI_STRIP', " \
  "'TRI_FAN', " \
  "'LINES_ADJ', " \
  "'TRIS_ADJ', " \
  "'LINE_STRIP_ADJ']"
#define PYDOC_DATAFORMAT_LITERAL \
  "Literal[" \
  "'FLOAT', " \
  "'INT', " \
  "'UINT', " \
  "'UBYTE', " \
  "'UINT_24_8', " \
  "'10_11_11_REV']"

[[nodiscard]] bool bpygpu_is_init_or_error();

#define BPYGPU_IS_INIT_OR_ERROR_OBJ \
  if (UNLIKELY(!bpygpu_is_init_or_error())) { \
    return NULL; \
  } \
  ((void)0)
#define BPYGPU_IS_INIT_OR_ERROR_INT \
  if (UNLIKELY(!bpygpu_is_init_or_error())) { \
    return -1; \
  } \
  ((void)0)

}  // namespace blender
