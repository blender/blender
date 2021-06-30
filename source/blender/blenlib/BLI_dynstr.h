/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 * \brief A dynamically sized string ADT.
 * \section aboutdynstr Dynamic String
 * This ADT is designed purely for dynamic string creation
 * through appending, not for general usage, the intent is
 * to build up dynamic strings using a DynStr object, then
 * convert it to a c-string and work with that.
 */

#include <stdarg.h>

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DynStr;

/** The abstract DynStr type */
typedef struct DynStr DynStr;

DynStr *BLI_dynstr_new(void) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
DynStr *BLI_dynstr_new_memarena(void) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;

void BLI_dynstr_append(DynStr *__restrict ds, const char *cstr) ATTR_NONNULL();
void BLI_dynstr_nappend(DynStr *__restrict ds, const char *cstr, int len) ATTR_NONNULL();

void BLI_dynstr_appendf(DynStr *__restrict ds, const char *__restrict format, ...)
    ATTR_PRINTF_FORMAT(2, 3) ATTR_NONNULL(1, 2);
void BLI_dynstr_vappendf(DynStr *__restrict ds, const char *__restrict format, va_list args)
    ATTR_PRINTF_FORMAT(2, 0) ATTR_NONNULL(1, 2);

int BLI_dynstr_get_len(const DynStr *ds) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
char *BLI_dynstr_get_cstring(const DynStr *ds) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void BLI_dynstr_get_cstring_ex(const DynStr *__restrict ds, char *__restrict rets) ATTR_NONNULL();

void BLI_dynstr_clear(DynStr *ds) ATTR_NONNULL();
void BLI_dynstr_free(DynStr *ds) ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
