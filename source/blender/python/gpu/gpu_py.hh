/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "../generic/py_capi_utils.hh"

extern struct PyC_StringEnumItems bpygpu_primtype_items[];
extern struct PyC_StringEnumItems bpygpu_dataformat_items[];

bool bpygpu_is_init_or_error(void);

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
