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
#include "BKE_path_templates.hh"

/* #include "IMB_imbuf_types.hh" */

namespace blender {

extern const char *imb_ext_image[];
extern const char *imb_ext_movie[];
extern const char *imb_ext_audio[];

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * Wrap #PyC_UnicodeAsBytesAndSize_Data, storing the argument so its type can be checked.
 */
struct BPyPath_UnicodeAsBytesAndSize_Data {
  PyC_UnicodeAsBytesAndSize_Data data;
  PyObject *value_src;
};

static int bpy_path_parse_unicode_as_bytes_and_size(PyObject *o, void *p)
{
  BPyPath_UnicodeAsBytesAndSize_Data *data = static_cast<BPyPath_UnicodeAsBytesAndSize_Data *>(p);
  /* A null `o` signals cleanup, keep the source argument. */
  if (o != nullptr) {
    data->value_src = o;
  }
  return PyC_ParseUnicodeAsBytesAndSize(o, &data->data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Methods
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_path_is_autoexec_doc,
    ".. function:: is_autoexec(path, *, skip_overrides=False, canonicalize=False, "
    "strip_filename=False)\n"
    "\n"
    "   Return false when a directory is excluded from running scripts automatically,\n"
    "   see :class:`bpy.types.PreferencesFilePaths.autoexec_paths`.\n"
    "\n"
    "   .. note:: The preference to enable automatic script execution & any command line\n"
    "      override are checked unless ``skip_overrides`` is set,\n"
    "      see :class:`bpy.types.PreferencesFilePaths.use_scripts_auto_execute`.\n"
    "\n"
    "   :param path: The directory to check, expected to end with a path separator,\n"
    "      must not be empty.\n"
    "   :type path: str | bytes\n"
    "   :param skip_overrides: Only check the excluded paths, skipping the preference to enable\n"
    "      automatic script execution & any command line override.\n"
    "      When false, the result is the default for \"Trusted Source\" when opening the path.\n"
    "   :type skip_overrides: bool\n"
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
  bool skip_overrides = false;
  bool canonicalize = false;
  bool strip_filename = false;

  static const char *_keywords[] = {
      "path", "skip_overrides", "canonicalize", "strip_filename", nullptr};
  static _PyArg_Parser _parser = {
      "O&" /* `path` */
      "|$" /* Optional, keyword only arguments. */
      "O&" /* `skip_overrides` */
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
                                        &skip_overrides,
                                        PyC_ParseBool,
                                        &canonicalize,
                                        PyC_ParseBool,
                                        &strip_filename))
  {
    return nullptr;
  }

  PyObject *result = nullptr;
  if (path_data.value_len == 0) [[unlikely]] {
    PyErr_SetString(PyExc_ValueError, "is_autoexec: path must not be empty");
  }
  else {
    const bool is_autoexec = BKE_autoexec_default_trust_source(
        path_data.value,
        {
            .skip_overrides = skip_overrides,
            .canonicalize = canonicalize,
            .strip_filename = strip_filename,
        });
    result = PyBool_FromLong(is_autoexec);
  }

  Py_XDECREF(path_data.value_coerce);
  return result;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_path_template_escape_doc,
    ".. function:: template_escape(path)\n"
    "\n"
    "   Escape ``{`` and ``}`` in a path, so they are not treated as path templates.\n"
    "\n"
    "   :param path: The path to escape.\n"
    "   :type path: str | bytes\n"
    "   :return: The escaped path, matching the type of ``path``.\n"
    "   :rtype: str | bytes\n");
static PyObject *bpy_path_template_escape(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  BPyPath_UnicodeAsBytesAndSize_Data path_data = {};

  static const char *_keywords[] = {"path", nullptr};
  static _PyArg_Parser _parser = {
      "O&" /* `path` */
      ":template_escape",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, bpy_path_parse_unicode_as_bytes_and_size, &path_data))
  {
    return nullptr;
  }

  const std::string escaped = BKE_path_template_escape(
      StringRef(path_data.data.value, path_data.data.value_len));
  Py_XDECREF(path_data.data.value_coerce);
  /* Escaping only adds characters, so an unchanged size means there was nothing to escape. */
  if (Py_ssize_t(escaped.size()) == path_data.data.value_len) {
    return Py_NewRef(path_data.value_src);
  }
  return PyBytes_Check(path_data.value_src) ?
             PyBytes_FromStringAndSize(escaped.data(), escaped.size()) :
             PyC_UnicodeFromStdStr(escaped);
}

static PyMethodDef _bpy_path_methods[] = {
    {"is_autoexec",
     reinterpret_cast<PyCFunction>(bpy_path_is_autoexec),
     METH_VARARGS | METH_KEYWORDS,
     bpy_path_is_autoexec_doc},
    {"template_escape",
     reinterpret_cast<PyCFunction>(bpy_path_template_escape),
     METH_VARARGS | METH_KEYWORDS,
     bpy_path_template_escape_doc},
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

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
