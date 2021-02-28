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

#include "BPy_BlenderTextureShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "../../../../python/generic/py_capi_utils.h"

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char BlenderTextureShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`BlenderTextureShader`\n"
    "\n"
    "[Texture shader]\n"
    "\n"
    ".. method:: __init__(texture)\n"
    "\n"
    "   Builds a BlenderTextureShader object.\n"
    "\n"
    "   :arg texture: A line style texture slot or a shader node tree to define\n"
    "       a set of textures.\n"
    "   :type texture: :class:`bpy.types.LineStyleTextureSlot` or\n"
    "       :class:`bpy.types.ShaderNodeTree`\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Assigns a blender texture slot to the stroke  shading in order to\n"
    "   simulate marks.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int BlenderTextureShader___init__(BPy_BlenderTextureShader *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {"texture", nullptr};
  PyObject *obj;
  MTex *_mtex;
  bNodeTree *_nodetree;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (char **)kwlist, &obj)) {
    return -1;
  }
  _mtex = (MTex *)PyC_RNA_AsPointer(obj, "LineStyleTextureSlot");
  if (_mtex) {
    self->py_ss.ss = new StrokeShaders::BlenderTextureShader(_mtex);
    return 0;
  }
  PyErr_Clear();
  _nodetree = (bNodeTree *)PyC_RNA_AsPointer(obj, "ShaderNodeTree");
  if (_nodetree) {
    self->py_ss.ss = new StrokeShaders::BlenderTextureShader(_nodetree);
    return 0;
  }
  PyErr_Format(PyExc_TypeError,
               "expected either 'LineStyleTextureSlot' or 'ShaderNodeTree', "
               "found '%.200s' instead",
               Py_TYPE(obj)->tp_name);
  return -1;
}

/*-----------------------BPy_BlenderTextureShader type definition ------------------------------*/

PyTypeObject BlenderTextureShader_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "BlenderTextureShader", /* tp_name */
    sizeof(BPy_BlenderTextureShader),                         /* tp_basicsize */
    0,                                                        /* tp_itemsize */
    nullptr,                                                  /* tp_dealloc */
    0,                                                        /* tp_vectorcall_offset */
    nullptr,                                                  /* tp_getattr */
    nullptr,                                                  /* tp_setattr */
    nullptr,                                                  /* tp_reserved */
    nullptr,                                                  /* tp_repr */
    nullptr,                                                  /* tp_as_number */
    nullptr,                                                  /* tp_as_sequence */
    nullptr,                                                  /* tp_as_mapping */
    nullptr,                                                  /* tp_hash  */
    nullptr,                                                  /* tp_call */
    nullptr,                                                  /* tp_str */
    nullptr,                                                  /* tp_getattro */
    nullptr,                                                  /* tp_setattro */
    nullptr,                                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                 /* tp_flags */
    BlenderTextureShader___doc__,                             /* tp_doc */
    nullptr,                                                  /* tp_traverse */
    nullptr,                                                  /* tp_clear */
    nullptr,                                                  /* tp_richcompare */
    0,                                                        /* tp_weaklistoffset */
    nullptr,                                                  /* tp_iter */
    nullptr,                                                  /* tp_iternext */
    nullptr,                                                  /* tp_methods */
    nullptr,                                                  /* tp_members */
    nullptr,                                                  /* tp_getset */
    &StrokeShader_Type,                                       /* tp_base */
    nullptr,                                                  /* tp_dict */
    nullptr,                                                  /* tp_descr_get */
    nullptr,                                                  /* tp_descr_set */
    0,                                                        /* tp_dictoffset */
    (initproc)BlenderTextureShader___init__,                  /* tp_init */
    nullptr,                                                  /* tp_alloc */
    nullptr,                                                  /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
