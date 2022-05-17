/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Helper functions for BLI_assert.h header.
 */

#include "BLI_assert.h" /* Own include. */
#include "BLI_system.h"

#include <stdio.h>
#include <stdlib.h>

void _BLI_assert_print_pos(const char *file, const int line, const char *function, const char *id)
{
  fprintf(stderr, "BLI_assert failed: %s:%d, %s(), at \'%s\'\n", file, line, function, id);
}

void _BLI_assert_print_extra(const char *str)
{
  fprintf(stderr, "  %s\n", str);
}

void _BLI_assert_unreachable_print(const char *file, const int line, const char *function)
{
  fprintf(stderr, "Code marked as unreachable has been executed. Please report this as a bug.\n");
  fprintf(stderr, "Error found at %s:%d in %s.\n", file, line, function);
}

void _BLI_assert_print_backtrace(void)
{
#ifndef NDEBUG
  BLI_system_backtrace(stderr);
#endif
}

void _BLI_assert_abort(void)
{
  /* Wrap to remove 'noreturn' attribute since this suppresses missing return statements,
   * allowing changes to debug builds to accidentally to break release builds.
   *
   * For example `BLI_assert_unreachable();` at the end of a function that returns a value,
   * will hide that it's missing a return. */

  abort();
}
