
/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BPY_OPERATOR_WRAP_H
#define BPY_OPERATOR_WRAP_H

#include <Python.h>

/* these are used for operator methods, used by bpy_operator.c */
PyObject *PYOP_wrap_add(PyObject *self, PyObject *args);
PyObject *PYOP_wrap_add_macro(PyObject *self, PyObject *args);
PyObject *PYOP_wrap_macro_define(PyObject *self, PyObject *args);
PyObject *PYOP_wrap_remove(PyObject *self, PyObject *args);

#endif
