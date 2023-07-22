/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 *
 * This file defines the 'bmesh.ops' module.
 * Operators from 'opdefines' are wrapped.
 */

#include <Python.h>

#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"

#include "bmesh_py_ops.h" /* own include */
#include "bmesh_py_ops_call.h"

/* bmesh operator 'bmesh.ops.*' callable types
 * ******************************************* */

static PyObject *bpy_bmesh_op_repr(BPy_BMeshOpFunc *self)
{
  return PyUnicode_FromFormat("<%.200s bmesh.ops.%.200s()>", Py_TYPE(self)->tp_name, self->opname);
}

/* methods
 * ======= */

/* __doc__
 * ------- */

static char *bmp_slots_as_args(const BMOSlotType slot_types[BMO_OP_MAX_SLOTS], const bool is_out)
{
  DynStr *dyn_str = BLI_dynstr_new();
  char *ret;
  bool quoted;
  bool set;

  int i = 0;

  while (*slot_types[i].name) {
    quoted = false;
    set = false;
    /* cut off '.out' by using a string size arg */
    const int name_len = is_out ? (strchr(slot_types[i].name, '.') - slot_types[i].name) :
                                  sizeof(slot_types[i].name);
    const char *value = "<Unknown>";
    switch (slot_types[i].type) {
      case BMO_OP_SLOT_BOOL:
        value = "False";
        break;
      case BMO_OP_SLOT_INT:
        if (slot_types[i].subtype.intg == BMO_OP_SLOT_SUBTYPE_INT_ENUM) {
          value = slot_types[i].enum_flags[0].identifier;
          quoted = true;
        }
        else if (slot_types[i].subtype.intg == BMO_OP_SLOT_SUBTYPE_INT_FLAG) {
          value = "";
          set = true;
        }
        else {
          value = "0";
        }
        break;
      case BMO_OP_SLOT_FLT:
        value = "0.0";
        break;
      case BMO_OP_SLOT_PTR:
        value = "None";
        break;
      case BMO_OP_SLOT_MAT:
        value = "Matrix()";
        break;
      case BMO_OP_SLOT_VEC:
        value = "Vector()";
        break;
      case BMO_OP_SLOT_ELEMENT_BUF:
        value = (slot_types[i].subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE) ? "None" : "[]";
        break;
      case BMO_OP_SLOT_MAPPING:
        value = "{}";
        break;
    }
    BLI_dynstr_appendf(dyn_str,
                       i ? ", %.*s=%s%s%s%s%s" : "%.*s=%s%s%s%s%s",
                       name_len,
                       slot_types[i].name,
                       set ? "{" : "",
                       quoted ? "'" : "",
                       value,
                       quoted ? "'" : "",
                       set ? "}" : "");
    i++;
  }

  ret = BLI_dynstr_get_cstring(dyn_str);
  BLI_dynstr_free(dyn_str);
  return ret;
}

static PyObject *bpy_bmesh_op_doc_get(BPy_BMeshOpFunc *self, void * /*closure*/)
{
  PyObject *ret;
  char *slot_in;
  char *slot_out;
  int i;

  i = BMO_opcode_from_opname(self->opname);

  slot_in = bmp_slots_as_args(bmo_opdefines[i]->slot_types_in, false);
  slot_out = bmp_slots_as_args(bmo_opdefines[i]->slot_types_out, true);

  ret = PyUnicode_FromFormat("%.200s bmesh.ops.%.200s(bmesh, %s)\n  -> dict(%s)",
                             Py_TYPE(self)->tp_name,
                             self->opname,
                             slot_in,
                             slot_out);

  MEM_freeN(slot_in);
  MEM_freeN(slot_out);

  return ret;
}

static PyGetSetDef bpy_bmesh_op_getseters[] = {
    {"__doc__", (getter)bpy_bmesh_op_doc_get, (setter) nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/* Types
 * ===== */

static PyTypeObject bmesh_op_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "BMeshOpFunc",
    /*tp_basicsize*/ sizeof(BPy_BMeshOpFunc),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)bpy_bmesh_op_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)BPy_BMO_call,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ bpy_bmesh_op_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

/* bmesh module 'bmesh.ops'
 * ************************ */

static PyObject *bpy_bmesh_op_CreatePyObject(const char *opname)
{
  BPy_BMeshOpFunc *self = PyObject_New(BPy_BMeshOpFunc, &bmesh_op_Type);

  self->opname = opname;

  return (PyObject *)self;
}

static PyObject *bpy_bmesh_ops_module_getattro(PyObject * /*self*/, PyObject *pyname)
{
  const char *opname = PyUnicode_AsUTF8(pyname);

  if (BMO_opcode_from_opname(opname) != -1) {
    return bpy_bmesh_op_CreatePyObject(opname);
  }

  PyErr_Format(PyExc_AttributeError, "BMeshOpsModule: operator \"%.200s\" doesn't exist", opname);
  return nullptr;
}

static PyObject *bpy_bmesh_ops_module_dir(PyObject * /*self*/)
{
  const uint tot = bmo_opdefines_total;
  uint i;
  PyObject *ret;

  ret = PyList_New(bmo_opdefines_total);

  for (i = 0; i < tot; i++) {
    PyList_SET_ITEM(ret, i, PyUnicode_FromString(bmo_opdefines[i]->opname));
  }

  return ret;
}

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static PyMethodDef BPy_BM_ops_methods[] = {
    {"__getattr__", (PyCFunction)bpy_bmesh_ops_module_getattro, METH_O, nullptr},
    {"__dir__", (PyCFunction)bpy_bmesh_ops_module_dir, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

PyDoc_STRVAR(BPy_BM_ops_doc, "Access to BMesh operators");
static PyModuleDef BPy_BM_ops_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bmesh.ops",
    /*m_doc*/ BPy_BM_ops_doc,
    /*m_size*/ 0,
    /*m_methods*/ BPy_BM_ops_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_bmesh_ops(void)
{
  PyObject *submodule = PyModule_Create(&BPy_BM_ops_module_def);

  if (PyType_Ready(&bmesh_op_Type) < 0) {
    return nullptr;
  }

  return submodule;
}
