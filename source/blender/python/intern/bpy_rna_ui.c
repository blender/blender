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
 *
 * This adds helpers to #uiLayout which can't be added easily to RNA itself.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"

#include "UI_interface.h"

#include "RNA_types.h"

#include "bpy_rna.h"
#include "bpy_rna_ui.h"

PyDoc_STRVAR(bpy_rna_uilayout_introspect_doc,
             ".. method:: introspect()\n"
             "\n"
             "   Return a dictionary containing a textual representation of the UI layout.\n");
static PyObject *bpy_rna_uilayout_introspect(PyObject *self)
{
  BPy_StructRNA *pyrna = (BPy_StructRNA *)self;
  uiLayout *layout = pyrna->ptr.data;

  const char *expr = UI_layout_introspect(layout);
  PyObject *main_mod = NULL;
  PyC_MainModule_Backup(&main_mod);
  PyObject *py_dict = PyC_DefaultNameSpace("<introspect>");
  PyObject *result = PyRun_String(expr, Py_eval_input, py_dict, py_dict);
  MEM_freeN((void *)expr);
  Py_DECREF(py_dict);
  PyC_MainModule_Restore(main_mod);
  return result;
}

PyMethodDef BPY_rna_uilayout_introspect_method_def = {
    "introspect",
    (PyCFunction)bpy_rna_uilayout_introspect,
    METH_NOARGS,
    bpy_rna_uilayout_introspect_doc,
};
