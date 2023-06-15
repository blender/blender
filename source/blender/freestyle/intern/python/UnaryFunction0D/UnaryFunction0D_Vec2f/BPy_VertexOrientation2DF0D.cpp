/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_VertexOrientation2DF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char VertexOrientation2DF0D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > "
    ":class:`freestyle.types.UnaryFunction0DVec2f` > :class:`VertexOrientation2DF0D`\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Builds a VertexOrientation2DF0D object.\n"
    "\n"
    ".. method:: __call__(it)\n"
    "\n"
    "   Returns a two-dimensional vector giving the 2D oriented tangent to the\n"
    "   1D element to which the :class:`freestyle.types.Interface0D` pointed\n"
    "   by the Interface0DIterator belongs.  The 2D oriented tangent is\n"
    "   evaluated at the pointed Interface0D.\n"
    "\n"
    "   :arg it: An Interface0DIterator object.\n"
    "   :type it: :class:`freestyle.types.Interface0DIterator`\n"
    "   :return: The 2D oriented tangent to the 1D element evaluated at the\n"
    "      pointed Interface0D.\n"
    "   :rtype: :class:`mathutils.Vector`\n";

static int VertexOrientation2DF0D___init__(BPy_VertexOrientation2DF0D *self,
                                           PyObject *args,
                                           PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->py_uf0D_vec2f.uf0D_vec2f = new Functions0D::VertexOrientation2DF0D();
  self->py_uf0D_vec2f.uf0D_vec2f->py_uf0D = (PyObject *)self;
  return 0;
}

/*-----------------------BPy_VertexOrientation2DF0D type definition -----------------------------*/

PyTypeObject VertexOrientation2DF0D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "VertexOrientation2DF0D",
    /*tp_basicsize*/ sizeof(BPy_VertexOrientation2DF0D),
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
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ VertexOrientation2DF0D___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &UnaryFunction0DVec2f_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)VertexOrientation2DF0D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
