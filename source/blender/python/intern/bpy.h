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
 * \ingroup pythonintern
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;

void BPy_init_modules(struct bContext *C);
extern PyObject *bpy_package_py;

/* bpy_interface_atexit.c */
void BPY_atexit_register(void);
void BPY_atexit_unregister(void);

extern struct CLG_LogRef *BPY_LOG_CONTEXT;
extern struct CLG_LogRef *BPY_LOG_RNA;
extern struct CLG_LogRef *BPY_LOG_INTERFACE;

#ifdef __cplusplus
}
#endif
