/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DynamicLibrary DynamicLibrary;

DynamicLibrary *BLI_dynlib_open(const char *name);
void *BLI_dynlib_find_symbol(DynamicLibrary *lib, const char *symname);
char *BLI_dynlib_get_error_as_string(DynamicLibrary *lib);
void BLI_dynlib_close(DynamicLibrary *lib);

#ifdef __cplusplus
}
#endif
