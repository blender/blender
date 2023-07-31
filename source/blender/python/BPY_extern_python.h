/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup python
 *
 * Functionality relating to Python setup & tear down.
 */

#pragma once

struct bContext;

#ifdef __cplusplus
extern "C" {
#endif

/* For 'FILE'. */
#include <stdio.h>

/* `bpy_interface.cc` */

/** Call #BPY_context_set first. */
void BPY_python_start(struct bContext *C, int argc, const char **argv);
void BPY_python_end(void);
void BPY_python_reset(struct bContext *C);
void BPY_python_use_system_env(void);
void BPY_python_backtrace(FILE *fp);

/* `bpy_app.cc` */

/* Access `main_args_help_as_string` needed to resolve bad level call. */
extern char *(*BPY_python_app_help_text_fn)(bool all);

#ifdef __cplusplus
} /* extern "C" */
#endif
