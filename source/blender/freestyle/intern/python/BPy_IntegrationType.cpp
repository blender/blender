/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_IntegrationType.h"

#include "BPy_Convert.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DFloat.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DUnsigned.h"

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------ MODULE FUNCTIONS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    Integrator_integrate_doc,
    ".. function:: integrate(func, it, it_end, integration_type)\n"
    "\n"
    "   Returns a single value from a set of values evaluated at each 0D\n"
    "   element of this 1D element.\n"
    "\n"
    "   :arg func: The UnaryFunction0D used to compute a value at each\n"
    "      Interface0D.\n"
    "   :type func: :class:`UnaryFunction0D`\n"
    "   :arg it: The Interface0DIterator used to iterate over the 0D\n"
    "      elements of this 1D element. The integration will occur over\n"
    "      the 0D elements starting from the one pointed by it.\n"
    "   :type it: :class:`Interface0DIterator`\n"
    "   :arg it_end: The Interface0DIterator pointing the end of the 0D\n"
    "      elements of the 1D element.\n"
    "   :type it_end: :class:`Interface0DIterator`\n"
    "   :arg integration_type: The integration method used to compute a\n"
    "      single value from a set of values.\n"
    "   :type integration_type: :class:`IntegrationType`\n"
    "   :return: The single value obtained for the 1D element. The return\n"
    "      value type is float if func is of the :class:`UnaryFunction0DDouble`\n"
    "      or :class:`UnaryFunction0DFloat` type, and int if func is of the\n"
    "      :class:`UnaryFunction0DUnsigned` type.\n"
    "   :rtype: int or float");

static PyObject *Integrator_integrate(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"func", "it", "it_end", "integration_type", nullptr};
  PyObject *obj1, *obj4 = nullptr;
  BPy_Interface0DIterator *obj2, *obj3;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwds,
                                   "O!O!O!|O!",
                                   (char **)kwlist,
                                   &UnaryFunction0D_Type,
                                   &obj1,
                                   &Interface0DIterator_Type,
                                   &obj2,
                                   &Interface0DIterator_Type,
                                   &obj3,
                                   &IntegrationType_Type,
                                   &obj4))
  {
    return nullptr;
  }

  Interface0DIterator it(*(obj2->if0D_it)), it_end(*(obj3->if0D_it));
  IntegrationType t = (obj4) ? IntegrationType_from_BPy_IntegrationType(obj4) : MEAN;

  if (BPy_UnaryFunction0DDouble_Check(obj1)) {
    UnaryFunction0D<double> *fun = ((BPy_UnaryFunction0DDouble *)obj1)->uf0D_double;
    double res = integrate(*fun, it, it_end, t);
    return PyFloat_FromDouble(res);
  }
  if (BPy_UnaryFunction0DFloat_Check(obj1)) {
    UnaryFunction0D<float> *fun = ((BPy_UnaryFunction0DFloat *)obj1)->uf0D_float;
    float res = integrate(*fun, it, it_end, t);
    return PyFloat_FromDouble(res);
  }
  if (BPy_UnaryFunction0DUnsigned_Check(obj1)) {
    UnaryFunction0D<uint> *fun = ((BPy_UnaryFunction0DUnsigned *)obj1)->uf0D_unsigned;
    uint res = integrate(*fun, it, it_end, t);
    return PyLong_FromLong(res);
  }

  string class_name(Py_TYPE(obj1)->tp_name);
  PyErr_SetString(PyExc_TypeError, ("unsupported function type: " + class_name).c_str());
  return nullptr;
}

/*-----------------------Integrator module docstring---------------------------------------*/

PyDoc_STRVAR(
    /* Wrap. */
    module_docstring,
    "The Blender Freestyle.Integrator submodule\n"
    "\n");

/*-----------------------Integrator module functions definitions---------------------------*/

static PyMethodDef module_functions[] = {
    {"integrate",
     (PyCFunction)Integrator_integrate,
     METH_VARARGS | METH_KEYWORDS,
     Integrator_integrate_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*-----------------------Integrator module definition--------------------------------------*/

static PyModuleDef module_definition = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "Freestyle.Integrator",
    /*m_doc*/ module_docstring,
    /*m_size*/ -1,
    /*m_methods*/ module_functions,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

/*-----------------------BPy_IntegrationType type definition ------------------------------*/

PyDoc_STRVAR(
    /* Wrap. */
    IntegrationType_doc,
    "Class hierarchy: int > :class:`IntegrationType`\n"
    "\n"
    "Different integration methods that can be invoked to integrate into a\n"
    "single value the set of values obtained from each 0D element of an 1D\n"
    "element:\n"
    "\n"
    "* IntegrationType.MEAN: The value computed for the 1D element is the\n"
    "  mean of the values obtained for the 0D elements.\n"
    "* IntegrationType.MIN: The value computed for the 1D element is the\n"
    "  minimum of the values obtained for the 0D elements.\n"
    "* IntegrationType.MAX: The value computed for the 1D element is the\n"
    "  maximum of the values obtained for the 0D elements.\n"
    "* IntegrationType.FIRST: The value computed for the 1D element is the\n"
    "  first of the values obtained for the 0D elements.\n"
    "* IntegrationType.LAST: The value computed for the 1D element is the\n"
    "  last of the values obtained for the 0D elements.");

PyTypeObject IntegrationType_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "IntegrationType",
    /*tp_basicsize*/ sizeof(PyLongObject),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ IntegrationType_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &PyLong_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

/*-----------------------BPy_IntegrationType instance definitions -------------------------*/

//-------------------MODULE INITIALIZATION--------------------------------
int IntegrationType_Init(PyObject *module)
{
  PyObject *m, *d, *f;

  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&IntegrationType_Type) < 0) {
    return -1;
  }
  Py_INCREF(&IntegrationType_Type);
  PyModule_AddObject(module, "IntegrationType", (PyObject *)&IntegrationType_Type);

#define ADD_TYPE_CONST(id) \
  PyLong_subtype_add_to_dict( \
      IntegrationType_Type.tp_dict, &IntegrationType_Type, STRINGIFY(id), id)

  ADD_TYPE_CONST(MEAN);
  ADD_TYPE_CONST(MIN);
  ADD_TYPE_CONST(MAX);
  ADD_TYPE_CONST(FIRST);
  ADD_TYPE_CONST(LAST);

#undef ADD_TYPE_CONST

  m = PyModule_Create(&module_definition);
  if (m == nullptr) {
    return -1;
  }
  Py_INCREF(m);
  PyModule_AddObject(module, "Integrator", m);

  // from Integrator import *
  d = PyModule_GetDict(m);
  for (PyMethodDef *p = module_functions; p->ml_name; p++) {
    f = PyDict_GetItemString(d, p->ml_name);
    Py_INCREF(f);
    PyModule_AddObject(module, p->ml_name, f);
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
