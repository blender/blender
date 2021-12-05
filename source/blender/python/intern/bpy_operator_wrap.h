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

struct wmOperatorType;

#ifdef __cplusplus
extern "C" {
#endif

/* these are used for operator methods, used by bpy_operator.c */
PyObject *PYOP_wrap_macro_define(PyObject *self, PyObject *args);

/* exposed to rna/wm api */
/**
 * Generic function used by all Python defined operators
 * it's passed as an argument to #WM_operatortype_append_ptr in for operator registration.
 */
void BPY_RNA_operator_wrapper(struct wmOperatorType *ot, void *userdata);
/**
 * Generic function used by all Python defined macro-operators
 * it's passed as an argument to #WM_operatortype_append_ptr in for operator registration.
 */
void BPY_RNA_operator_macro_wrapper(struct wmOperatorType *ot, void *userdata);

#ifdef __cplusplus
}
#endif
