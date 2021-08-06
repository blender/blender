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

PyObject *BPY_rna_props(void);
void BPY_rna_props_clear_all(void);

PyObject *BPy_PointerProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_CollectionProperty(PyObject *self, PyObject *args, PyObject *kw);
StructRNA *pointer_type_from_py(PyObject *value, const char *error_prefix);

typedef struct {
  PyObject_HEAD
  /**
   * Internally a #PyCFunctionObject type.
   * \note This isn't GC tracked, it's a function from `bpy.props` so it's not going away.
   */
  void *fn;
  PyObject *kw;
} BPy_PropDeferred;

extern PyTypeObject bpy_prop_deferred_Type;
#define BPy_PropDeferred_CheckTypeExact(v) (Py_TYPE(v) == &bpy_prop_deferred_Type)

#define PYRNA_STACK_ARRAY RNA_STACK_ARRAY

#ifdef __cplusplus
}
#endif
