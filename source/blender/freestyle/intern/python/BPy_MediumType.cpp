/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_MediumType.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

/*-----------------------BPy_MediumType type definition ------------------------------*/

PyDoc_STRVAR(MediumType_doc,
             "Class hierarchy: int > :class:`MediumType`\n"
             "\n"
             "The different blending modes available to simulate the interaction\n"
             "media-medium:\n"
             "\n"
             "* Stroke.DRY_MEDIUM: To simulate a dry medium such as Pencil or Charcoal.\n"
             "* Stroke.HUMID_MEDIUM: To simulate ink painting (color subtraction blending).\n"
             "* Stroke.OPAQUE_MEDIUM: To simulate an opaque medium (oil, spray...).");

PyTypeObject MediumType_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "MediumType",
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
    /*tp_doc*/ MediumType_doc,
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

PyLongObject _BPy_MediumType_DRY_MEDIUM = {
    PyVarObject_HEAD_INIT(&MediumType_Type, 1){Stroke::DRY_MEDIUM},
};
PyLongObject _BPy_MediumType_HUMID_MEDIUM = {
    PyVarObject_HEAD_INIT(&MediumType_Type, 1){Stroke::HUMID_MEDIUM},
};
PyLongObject _BPy_MediumType_OPAQUE_MEDIUM = {
    PyVarObject_HEAD_INIT(&MediumType_Type, 1){Stroke::OPAQUE_MEDIUM},
};

//-------------------MODULE INITIALIZATION--------------------------------

int MediumType_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&MediumType_Type) < 0) {
    return -1;
  }
  Py_INCREF(&MediumType_Type);
  PyModule_AddObject(module, "MediumType", (PyObject *)&MediumType_Type);

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
