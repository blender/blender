/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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

/** The abstract DynStr type. */
typedef struct DynStr DynStr;

/**
 * Create a new #DynStr.
 *
 * \return Pointer to a new #DynStr.
 */
DynStr *BLI_dynstr_new(void) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
/**
 * Create a new #DynStr.
 *
 * \return Pointer to a new #DynStr.
 */
DynStr *BLI_dynstr_new_memarena(void) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;

/**
 * Append a c-string to a #DynStr.
 *
 * \param ds: The #DynStr to append to.
 * \param cstr: The c-string to append.
 */
void BLI_dynstr_append(DynStr *__restrict ds, const char *cstr) ATTR_NONNULL();
/**
 * Append a length clamped c-string to a #DynStr.
 *
 * \param ds: The #DynStr to append to.
 * \param cstr: The c-string to append.
 * \param len: The maximum length of the c-string to copy.
 */
void BLI_dynstr_nappend(DynStr *__restrict ds, const char *cstr, int len) ATTR_NONNULL();

/**
 * Append a c-string to a #DynStr, but with formatting like `printf`.
 *
 * \param ds: The #DynStr to append to.
 * \param format: The `printf` format string to use.
 */
void BLI_dynstr_appendf(DynStr *__restrict ds, const char *__restrict format, ...)
    ATTR_PRINTF_FORMAT(2, 3) ATTR_NONNULL(1, 2);
void BLI_dynstr_vappendf(DynStr *__restrict ds, const char *__restrict format, va_list args)
    ATTR_PRINTF_FORMAT(2, 0) ATTR_NONNULL(1, 2);

/**
 * Find the length of a #DynStr.
 *
 * \param ds: The #DynStr of interest.
 * \return The length of \a ds.
 */
int BLI_dynstr_get_len(const DynStr *ds) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Get a #DynStr's contents as a c-string.
 * \return The c-string which must be freed using #MEM_freeN.
 *
 * \param ds: The #DynStr of interest.
 * \return The contents of \a ds as a c-string.
 */
char *BLI_dynstr_get_cstring(const DynStr *ds) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Get a #DynStr's contents as a c-string.
 * The \a rets argument must be allocated to be at
 * least the size of `BLI_dynstr_get_len(ds) + 1`.
 *
 * \param ds: The DynStr of interest.
 * \param rets: The string to fill.
 */
void BLI_dynstr_get_cstring_ex(const DynStr *__restrict ds, char *__restrict rets) ATTR_NONNULL();

/**
 * Clear the #DynStr
 *
 * \param ds: The DynStr to clear.
 */
void BLI_dynstr_clear(DynStr *ds) ATTR_NONNULL();
/**
 * Free the #DynStr
 *
 * \param ds: The DynStr to free.
 */
void BLI_dynstr_free(DynStr *ds) ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
