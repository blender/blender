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

#include "BPy_SpatialNoiseShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char SpatialNoiseShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`SpatialNoiseShader`\n"
    "\n"
    "[Geometry shader]\n"
    "\n"
    ".. method:: __init__(amount, scale, num_octaves, smooth, pure_random)\n"
    "\n"
    "   Builds a SpatialNoiseShader object.\n"
    "\n"
    "   :arg amount: The amplitude of the noise.\n"
    "   :type amount: float\n"
    "   :arg scale: The noise frequency.\n"
    "   :type scale: float\n"
    "   :arg num_octaves: The number of octaves\n"
    "   :type num_octaves: int\n"
    "   :arg smooth: True if you want the noise to be smooth.\n"
    "   :type smooth: bool\n"
    "   :arg pure_random: True if you don't want any coherence.\n"
    "   :type pure_random: bool\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Spatial Noise stroke shader.  Moves the vertices to make the stroke\n"
    "   more noisy.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int SpatialNoiseShader___init__(BPy_SpatialNoiseShader *self,
                                       PyObject *args,
                                       PyObject *kwds)
{
  static const char *kwlist[] = {
      "amount", "scale", "num_octaves", "smooth", "pure_random", nullptr};
  float f1, f2;
  int i3;
  PyObject *obj4 = nullptr, *obj5 = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwds,
                                   "ffiO!O!",
                                   (char **)kwlist,
                                   &f1,
                                   &f2,
                                   &i3,
                                   &PyBool_Type,
                                   &obj4,
                                   &PyBool_Type,
                                   &obj5)) {
    return -1;
  }
  self->py_ss.ss = new SpatialNoiseShader(
      f1, f2, i3, bool_from_PyBool(obj4), bool_from_PyBool(obj5));
  return 0;
}

/*-----------------------BPy_SpatialNoiseShader type definition ------------------------------*/

PyTypeObject SpatialNoiseShader_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "SpatialNoiseShader", /* tp_name */
    sizeof(BPy_SpatialNoiseShader),                         /* tp_basicsize */
    0,                                                      /* tp_itemsize */
    nullptr,                                                /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                  /* tp_getattr */
    nullptr,                                  /* tp_setattr */
    nullptr,                                  /* tp_reserved */
    nullptr,                                  /* tp_repr */
    nullptr,                                  /* tp_as_number */
    nullptr,                                  /* tp_as_sequence */
    nullptr,                                  /* tp_as_mapping */
    nullptr,                                  /* tp_hash  */
    nullptr,                                  /* tp_call */
    nullptr,                                  /* tp_str */
    nullptr,                                  /* tp_getattro */
    nullptr,                                  /* tp_setattro */
    nullptr,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    SpatialNoiseShader___doc__,               /* tp_doc */
    nullptr,                                  /* tp_traverse */
    nullptr,                                  /* tp_clear */
    nullptr,                                  /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    nullptr,                                  /* tp_iter */
    nullptr,                                  /* tp_iternext */
    nullptr,                                  /* tp_methods */
    nullptr,                                  /* tp_members */
    nullptr,                                  /* tp_getset */
    &StrokeShader_Type,                       /* tp_base */
    nullptr,                                  /* tp_dict */
    nullptr,                                  /* tp_descr_get */
    nullptr,                                  /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)SpatialNoiseShader___init__,    /* tp_init */
    nullptr,                                  /* tp_alloc */
    nullptr,                                  /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
