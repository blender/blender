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
 */

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

/* bpy_interface.c */
void BPY_python_start(struct bContext *C, int argc, const char **argv);
void BPY_python_end(void);
void BPY_python_reset(struct bContext *C);
void BPY_python_use_system_env(void);
void BPY_python_backtrace(FILE *fp);

#ifdef __cplusplus
} /* extern "C" */
#endif
