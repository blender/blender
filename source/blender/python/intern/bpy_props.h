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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BPY_PROPS_H
#define BPY_PROPS_H

#include <Python.h>

PyObject *BPY_rna_props( void );

/* functions for setting up new props - experemental */
PyObject *BPy_BoolProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_BoolVectorProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_IntProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_IntVectorProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_FloatProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_FloatVectorProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_StringProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_EnumProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_PointerProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_CollectionProperty(PyObject *self, PyObject *args, PyObject *kw);

PyObject *BPy_RemoveProperty(PyObject *self, PyObject *args, PyObject *kw);

#define PYRNA_STACK_ARRAY 32

#endif
