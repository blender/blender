/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_BlenderTextureShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#include "../../../../python/generic/py_capi_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "BlenderTextureShader",
    /*tp_basicsize*/ sizeof(BPy_BlenderTextureShader),
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
    /*tp_doc*/ BlenderTextureShader___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &StrokeShader_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)BlenderTextureShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
