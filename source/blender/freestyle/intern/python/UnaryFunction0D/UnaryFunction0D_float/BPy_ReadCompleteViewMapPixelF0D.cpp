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
 * \ingroup freestyle
 */

#include "BPy_ReadCompleteViewMapPixelF0D.h"

#include "../../../stroke/AdvancedFunctions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ReadCompleteViewMapPixelF0D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > "
    ":class:`freestyle.types.UnaryFunction0DFloat` > :class:`ReadCompleteViewMapPixelF0D`\n"
    "\n"
    ".. method:: __init__(level)\n"
    "\n"
    "   Builds a ReadCompleteViewMapPixelF0D object.\n"
    "\n"
    "   :arg level: The level of the pyramid from which the pixel must be\n"
    "      read.\n"
    "   :type level: int\n"
    "\n"
    ".. method:: __call__(it)\n"
    "\n"
    "   Reads a pixel in one of the level of the complete viewmap.\n"
    "\n"
    "   :arg it: An Interface0DIterator object.\n"
    "   :type it: :class:`freestyle.types.Interface0DIterator`\n"
    "   :return: A pixel in one of the level of the complete viewmap.\n"
    "   :rtype: float\n";

static int ReadCompleteViewMapPixelF0D___init__(BPy_ReadCompleteViewMapPixelF0D *self,
                                                PyObject *args,
                                                PyObject *kwds)
{
  static const char *kwlist[] = {"level", nullptr};
  int i;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", (char **)kwlist, &i)) {
    return -1;
  }
  self->py_uf0D_float.uf0D_float = new Functions0D::ReadCompleteViewMapPixelF0D(i);
  self->py_uf0D_float.uf0D_float->py_uf0D = (PyObject *)self;
  return 0;
}

/*-----------------------BPy_ReadCompleteViewMapPixelF0D type definition ------------------------*/

PyTypeObject ReadCompleteViewMapPixelF0D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "ReadCompleteViewMapPixelF0D", /* tp_name */
    sizeof(BPy_ReadCompleteViewMapPixelF0D),                      /* tp_basicsize */
    0,                                                            /* tp_itemsize */
    nullptr,                                                            /* tp_dealloc */
    nullptr,                                                            /* tp_print */
    nullptr,                                                            /* tp_getattr */
    nullptr,                                                            /* tp_setattr */
    nullptr,                                                            /* tp_reserved */
    nullptr,                                                            /* tp_repr */
    nullptr,                                                            /* tp_as_number */
    nullptr,                                                            /* tp_as_sequence */
    nullptr,                                                            /* tp_as_mapping */
    nullptr,                                                            /* tp_hash  */
    nullptr,                                                            /* tp_call */
    nullptr,                                                            /* tp_str */
    nullptr,                                                            /* tp_getattro */
    nullptr,                                                            /* tp_setattro */
    nullptr,                                                            /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                     /* tp_flags */
    ReadCompleteViewMapPixelF0D___doc__,                          /* tp_doc */
    nullptr,                                                            /* tp_traverse */
    nullptr,                                                            /* tp_clear */
    nullptr,                                                            /* tp_richcompare */
    0,                                                            /* tp_weaklistoffset */
    nullptr,                                                            /* tp_iter */
    nullptr,                                                            /* tp_iternext */
    nullptr,                                                            /* tp_methods */
    nullptr,                                                            /* tp_members */
    nullptr,                                                            /* tp_getset */
    &UnaryFunction0DFloat_Type,                                   /* tp_base */
    nullptr,                                                            /* tp_dict */
    nullptr,                                                            /* tp_descr_get */
    nullptr,                                                            /* tp_descr_set */
    0,                                                            /* tp_dictoffset */
    (initproc)ReadCompleteViewMapPixelF0D___init__,               /* tp_init */
    nullptr,                                                            /* tp_alloc */
    nullptr,                                                            /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
