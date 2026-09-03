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
    ".. function:: is_autoexec(path, *, canonicalize=False, strip_filename=False)\n"
    "\n"
    "   Return false when a directory is excluded from running scripts automatically,\n"
    "   see :class:`bpy.types.PreferencesFilePaths.autoexec_paths`.\n"
    "\n"
    "   .. note:: The preference to enable automatic script execution is not checked,\n"
    "      see :class:`bpy.types.PreferencesFilePaths.use_scripts_auto_execute`.\n"
    "\n"
    "   :param path: The directory to check, expected to end with a path separator.\n"
    "   :type path: str | bytes\n"
    "   :param canonicalize: Resolve the path first,\n"
    "      disable when it's known to be resolved.\n"
    "   :type canonicalize: bool\n"
    "   :param strip_filename: Use the directory of `path`, otherwise it is a directory already.\n"
    "   :type strip_filename: bool\n"
    "   :return: True when blend-files in the directory are trusted to run scripts.\n"
    "   :rtype: bool\n");
static PyObject *bpy_path_is_autoexec(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  PyC_UnicodeAsBytesAndSize_Data path_data = {nullptr};
  bool canonicalize = false;
  bool strip_filename = false;

  static const char *_keywords[] = {"path", "canonicalize", "strip_filename", nullptr};
  static _PyArg_Parser _parser = {
      "O&" /* `path` */
      "|$" /* Optional, keyword only arguments. */
      "O&" /* `canonicalize` */
      "O&" /* `strip_filename` */
      ":is_autoexec",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseUnicodeAsBytesAndSize,
                                        &path_data,
                                        PyC_ParseBool,
                                        &canonicalize,
                                        PyC_ParseBool,
                                        &strip_filename))
  {
    return nullptr;
  }

  const bool is_autoexec = !BKE_autoexec_match_unchecked(
      path_data.value, canonicalize, strip_filename);
  Py_XDECREF(path_data.value_coerce);

  return PyBool_FromLong(is_autoexec);
}

static PyMethodDef _bpy_path_methods[] = {
    {"is_autoexec",
     reinterpret_cast<PyCFunction>(bpy_path_is_autoexec),
     METH_VARARGS | METH_KEYWORDS,
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
