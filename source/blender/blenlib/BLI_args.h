/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bli
 * \brief A general argument parsing module.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bArgs;
typedef struct bArgs bArgs;

#include <stdarg.h> /* For `va_list`. */

#include "BLI_compiler_attrs.h"

/**
 * Returns the number of extra arguments consumed by the function.
 * -  0 is normal value,
 * - -1 stops parsing arguments, other negative indicates skip
 */
typedef int (*BA_ArgCallback)(int argc, const char **argv, void *data);

struct bArgs *BLI_args_create(int argc, const char **argv);
void BLI_args_destroy(struct bArgs *ba);

typedef void (*bArgPrintFn)(void *user_data, const char *format, va_list args);
void BLI_args_printf(struct bArgs *ba, const char *format, ...);
void BLI_args_print_fn_set(struct bArgs *ba,
                           ATTR_PRINTF_FORMAT(2, 0) bArgPrintFn print_fn,
                           void *user_data);

/** The pass to use for #BLI_args_add. */
void BLI_args_pass_set(struct bArgs *ba, int current_pass);

/**
 * Pass starts at 1, -1 means valid all the time
 * short_arg or long_arg can be null to specify no short or long versions
 */
void BLI_args_add(struct bArgs *ba,
                  const char *short_arg,
                  const char *long_arg,
                  const char *doc,
                  BA_ArgCallback cb,
                  void *data);

/**
 * Short_case and long_case specify if those arguments are case specific
 */
void BLI_args_add_case(struct bArgs *ba,
                       const char *short_arg,
                       int short_case,
                       const char *long_arg,
                       int long_case,
                       const char *doc,
                       BA_ArgCallback cb,
                       void *data);

void BLI_args_parse(struct bArgs *ba, int pass, BA_ArgCallback default_cb, void *data);

void BLI_args_print_arg_doc(struct bArgs *ba, const char *arg);
void BLI_args_print_other_doc(struct bArgs *ba);

bool BLI_args_has_other_doc(const struct bArgs *ba);

void BLI_args_print(struct bArgs *ba);

#ifdef __cplusplus
}
#endif
