/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines '_bpy_path' module, Some 'C' functionality used by 'bpy.path'
 */

#include <Python.h>

#include "bpy_path.hh"

#include "../generic/py_capi_utils.hh"

#include "BKE_autoexec.hh"

/* #include "IMB_imbuf_types.hh" */

namespace blender {

extern const char *imb_ext_image[];
extern const char *imb_ext_movie[];
extern const char *imb_ext_audio[];

PyDoc_STRVAR(
    /* Wrap. */
    bpy_path_is_autoexec_doc,
    ".. function:: is_autoexec(dirpath)\n"
    "\n"
    "   Check if blend-files in a directory are trusted to run scripts automatically,\n"
    "   based on the excluded paths in the preferences.\n"
    "   The preference to enable automatic script execution isn't taken into account.\n"
    "\n"
    "   :param dirpath: The directory to check, expected to end with a path separator.\n"
    "   :type dirpath: str | bytes\n"
    "   :return: False when the directory matches an excluded path, otherwise True.\n"
    "   :rtype: bool\n");
static PyObject *bpy_path_is_autoexec(PyObject * /*self*/, PyObject *value)
{
  PyC_UnicodeAsBytesAndSize_Data path_data = {nullptr};
  if (!PyC_ParseUnicodeAsBytesAndSize(value, &path_data)) {
    return nullptr;
  }

  const bool is_autoexec = !BKE_autoexec_match_unchecked(path_data.value);
  Py_XDECREF(path_data.value_coerce);

  return PyBool_FromLong(is_autoexec);
}

static PyMethodDef _bpy_path_methods[] = {
    {"is_autoexec",
     static_cast<PyCFunction>(bpy_path_is_autoexec),
     METH_O,
     bpy_path_is_autoexec_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------------MODULE INIT-------------------------*/
static PyModuleDef _bpy_path_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "_bpy_path",
    /*m_doc*/ nullptr,
    /*m_size*/ 0,
    /*m_methods*/ _bpy_path_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit__bpy_path()
{
  PyObject *submodule;

  submodule = PyModule_Create(&_bpy_path_module_def);

  PyModule_AddObject(submodule, "extensions_image", PyC_FrozenSetFromStrings(imb_ext_image));
  PyModule_AddObject(submodule, "extensions_movie", PyC_FrozenSetFromStrings(imb_ext_movie));
  PyModule_AddObject(submodule, "extensions_audio", PyC_FrozenSetFromStrings(imb_ext_audio));

  return submodule;
}

}  // namespace blender
