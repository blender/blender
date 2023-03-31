/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation */

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
static PyTypeObject bmesh_op_Type;

static PyObject *bpy_bmesh_op_CreatePyObject(const char *opname)
{
  BPy_BMeshOpFunc *self = PyObject_New(BPy_BMeshOpFunc, &bmesh_op_Type);

  self->opname = opname;

  return (PyObject *)self;
}

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

static PyObject *bpy_bmesh_op_doc_get(BPy_BMeshOpFunc *self, void *UNUSED(closure))
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
    {"__doc__", (getter)bpy_bmesh_op_doc_get, (setter)NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/* Types
 * ===== */

static PyTypeObject bmesh_op_Type = {
    /*tp_name*/ PyVarObject_HEAD_INIT(NULL, 0) "BMeshOpFunc",
    /*tp_basicsize*/ sizeof(BPy_BMeshOpFunc),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ NULL,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ NULL,
    /*tp_setattr*/ NULL,
    /*tp_as_async*/ NULL,
    /*tp_repr*/ (reprfunc)bpy_bmesh_op_repr,
    /*tp_as_number*/ NULL,
    /*tp_as_sequence*/ NULL,
    /*tp_as_mapping*/ NULL,
    /*tp_hash*/ NULL,
    /*tp_call*/ (ternaryfunc)BPy_BMO_call,
    /*tp_str*/ NULL,
    /*tp_getattro*/ NULL,
    /*tp_setattro*/ NULL,
    /*tp_as_buffer*/ NULL,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ NULL,
    /*tp_traverse*/ NULL,
    /*tp_clear*/ NULL,
    /*tp_richcompare*/ NULL,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ NULL,
    /*tp_iternext*/ NULL,
    /*tp_methods*/ NULL,
    /*tp_members*/ NULL,
    /*tp_getset*/ bpy_bmesh_op_getseters,
    /*tp_base*/ NULL,
    /*tp_dict*/ NULL,
    /*tp_descr_get*/ NULL,
    /*tp_descr_set*/ NULL,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ NULL,
    /*tp_alloc*/ NULL,
    /*tp_new*/ NULL,
    /*tp_free*/ NULL,
    /*tp_is_gc*/ NULL,
    /*tp_bases*/ NULL,
    /*tp_mro*/ NULL,
    /*tp_cache*/ NULL,
    /*tp_subclasses*/ NULL,
    /*tp_weaklist*/ NULL,
    /*tp_del*/ NULL,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ NULL,
    /*tp_vectorcall*/ NULL,
};

/* bmesh module 'bmesh.ops'
 * ************************ */

static PyObject *bpy_bmesh_ops_module_getattro(PyObject *UNUSED(self), PyObject *pyname)
{
  const char *opname = PyUnicode_AsUTF8(pyname);

  if (BMO_opcode_from_opname(opname) != -1) {
    return bpy_bmesh_op_CreatePyObject(opname);
  }

  PyErr_Format(PyExc_AttributeError, "BMeshOpsModule: operator \"%.200s\" doesn't exist", opname);
  return NULL;
}

static PyObject *bpy_bmesh_ops_module_dir(PyObject *UNUSED(self))
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

static struct PyMethodDef BPy_BM_ops_methods[] = {
    {"__getattr__", (PyCFunction)bpy_bmesh_ops_module_getattro, METH_O, NULL},
    {"__dir__", (PyCFunction)bpy_bmesh_ops_module_dir, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(BPy_BM_ops_doc, "Access to BMesh operators");
static struct PyModuleDef BPy_BM_ops_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "bmesh.ops",
    /*m_doc*/ BPy_BM_ops_doc,
    /*m_size*/ 0,
    /*m_methods*/ BPy_BM_ops_methods,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyObject *BPyInit_bmesh_ops(void)
{
  PyObject *submodule = PyModule_Create(&BPy_BM_ops_module_def);

  if (PyType_Ready(&bmesh_op_Type) < 0) {
    return NULL;
  }

  return submodule;
}
