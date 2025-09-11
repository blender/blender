/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines the '_bpy' module which is used by python's 'bpy' package
 * to access C defined builtin functions.
 * A script writer should never directly access this module.
 */

/* Future-proof, See https://docs.python.org/3/c-api/arg.html#strings-and-buffers */
#define PY_SSIZE_T_CLEAN

#include <Python.h>

#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"
#include "BKE_bpath.hh"
#include "BKE_global.hh" /* XXX, G_MAIN only */

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "GPU_state.hh"

#include "WM_api.hh" /* For #WM_ghost_backend */

#include "bpy.hh"
#include "bpy_app.hh"
#include "bpy_cli_command.hh"
#include "bpy_driver.hh"
#include "bpy_geometry_set.hh"
#include "bpy_inline_shader_nodes.hh"
#include "bpy_library.hh"
#include "bpy_operator.hh"
#include "bpy_props.hh"
#include "bpy_rna.hh"
#include "bpy_rna_data.hh"
#include "bpy_rna_gizmo.hh"
#include "bpy_rna_types_capi.hh"
#include "bpy_utils_previews.hh"
#include "bpy_utils_units.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */
#include "../generic/python_utildefines.hh"

/* external util modules */
#include "../generic/idprop_py_api.hh"
#include "../generic/idprop_py_ui_api.hh"
#include "bpy_msgbus.hh"

#ifdef WITH_FREESTYLE
#  include "BPy_Freestyle.h"
#endif

PyObject *bpy_package_py = nullptr;

PyDoc_STRVAR(
    /* Wrap. */
    bpy_script_paths_doc,
    ".. function:: script_paths()\n"
    "\n"
    "   Return 2 paths to blender scripts directories.\n"
    "\n"
    "   :return: (system, user) strings will be empty when not found.\n"
    "   :rtype: tuple[str, str]\n");
static PyObject *bpy_script_paths(PyObject * /*self*/)
{
  PyObject *ret = PyTuple_New(2);
  PyObject *item;

  std::optional<std::string> path = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, nullptr);
  item = PyC_UnicodeFromStdStr(path.value_or(""));
  BLI_assert(item != nullptr);
  PyTuple_SET_ITEM(ret, 0, item);
  path = BKE_appdir_folder_id(BLENDER_USER_SCRIPTS, nullptr);
  item = PyC_UnicodeFromStdStr(path.value_or(""));
  BLI_assert(item != nullptr);
  PyTuple_SET_ITEM(ret, 1, item);

  return ret;
}

static bool bpy_blend_foreach_path_cb(BPathForeachPathData *bpath_data,
                                      char * /*path_dst*/,
                                      size_t /*path_dst_maxncpy*/,
                                      const char *path_src)
{
  PyObject *py_list = static_cast<PyObject *>(bpath_data->user_data);
  PyList_APPEND(py_list, PyC_UnicodeFromBytes(path_src));
  return false; /* Never edits the path. */
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_blend_paths_doc,
    ".. function:: blend_paths(*, absolute=False, packed=False, local=False)\n"
    "\n"
    "   Returns a list of paths to external files referenced by the loaded .blend file.\n"
    "\n"
    "   :arg absolute: When true the paths returned are made absolute.\n"
    "   :type absolute: bool\n"
    "   :arg packed: When true skip file paths for packed data.\n"
    "   :type packed: bool\n"
    "   :arg local: When true skip linked library paths.\n"
    "   :type local: bool\n"
    "   :return: path list.\n"
    "   :rtype: list[str]\n");
static PyObject *bpy_blend_paths(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  eBPathForeachFlag flag = eBPathForeachFlag(0);
  PyObject *list;

  bool absolute = false;
  bool packed = false;
  bool local = false;

  static const char *_keywords[] = {"absolute", "packed", "local", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "|$" /* Optional keyword only arguments. */
      "O&" /* `absolute` */
      "O&" /* `packed` */
      "O&" /* `local` */
      ":blend_paths",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseBool,
                                        &absolute,
                                        PyC_ParseBool,
                                        &packed,
                                        PyC_ParseBool,
                                        &local))
  {
    return nullptr;
  }

  if (absolute) {
    flag |= BKE_BPATH_FOREACH_PATH_ABSOLUTE;
  }
  if (!packed) {
    flag |= BKE_BPATH_FOREACH_PATH_SKIP_PACKED;
  }
  if (local) {
    flag |= BKE_BPATH_FOREACH_PATH_SKIP_LINKED;
  }

  list = PyList_New(0);

  BPathForeachPathData path_data{};
  path_data.bmain = G_MAIN;
  path_data.callback_function = bpy_blend_foreach_path_cb;
  path_data.flag = flag;
  path_data.user_data = list;
  BKE_bpath_foreach_path_main(&path_data);

  return list;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_flip_name_doc,
    ".. function:: flip_name(name, *, strip_digits=False)\n"
    "\n"
    "   Flip a name between left/right sides, useful for \n"
    "   mirroring bone names.\n"
    "\n"
    "   :arg name: Bone name to flip.\n"
    "   :type name: str\n"
    "   :arg strip_digits: Whether to remove ``.###`` suffix.\n"
    "   :type strip_digits: bool\n"
    "   :return: The flipped name.\n"
    "   :rtype: str\n");
static PyObject *bpy_flip_name(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  const char *name_src = nullptr;
  Py_ssize_t name_src_len;
  bool strip_digits = false;

  static const char *_keywords[] = {"", "strip_digits", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "s#" /* `name` */
      "|$" /* Optional, keyword only arguments. */
      "O&" /* `strip_digits` */
      ":flip_name",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &name_src, &name_src_len, PyC_ParseBool, &strip_digits))
  {
    return nullptr;
  }

  /* Worst case we gain one extra byte (besides null-terminator) by changing
   * "Left" to "Right", because only the first appearance of "Left" gets replaced. */
  const size_t size = name_src_len + 2;
  char *name_dst = static_cast<char *>(PyMem_MALLOC(size));
  const size_t name_dst_len = BLI_string_flip_side_name(name_dst, name_src, strip_digits, size);

  PyObject *result = PyUnicode_FromStringAndSize(name_dst, name_dst_len);

  PyMem_FREE(name_dst);

  return result;
}

/* `bpy_user_resource_doc`, Now in `bpy/utils/__init__.py`. */
static PyObject *bpy_user_resource(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  const PyC_StringEnumItems type_items[] = {
      {BLENDER_USER_DATAFILES, "DATAFILES"},
      {BLENDER_USER_CONFIG, "CONFIG"},
      {BLENDER_USER_SCRIPTS, "SCRIPTS"},
      {BLENDER_USER_EXTENSIONS, "EXTENSIONS"},
      {0, nullptr},
  };
  PyC_StringEnum type = {type_items};
  PyC_UnicodeAsBytesAndSize_Data subdir_data = {nullptr};

  static const char *_keywords[] = {"type", "path", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O&" /* `type` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `path` */
      ":user_resource",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseStringEnum,
                                        &type,
                                        PyC_ParseUnicodeAsBytesAndSize,
                                        &subdir_data))
  {
    return nullptr;
  }

  /* same logic as BKE_appdir_folder_id_create(),
   * but best leave it up to the script author to create */
  const std::optional<std::string> path = BKE_appdir_folder_id_user_notest(type.value_found,
                                                                           subdir_data.value);
  Py_XDECREF(subdir_data.value_coerce);

  return PyC_UnicodeFromStdStr(path.value_or(""));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_system_resource_doc,
    ".. function:: system_resource(type, *, path=\"\")\n"
    "\n"
    "   Return a system resource path.\n"
    "\n"
    "   :arg type: string in ['DATAFILES', 'SCRIPTS', 'EXTENSIONS', 'PYTHON'].\n"
    "   :type type: str\n"
    "   :arg path: Optional subdirectory.\n"
    "   :type path: str | bytes\n");
static PyObject *bpy_system_resource(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  const PyC_StringEnumItems type_items[] = {
      {BLENDER_SYSTEM_DATAFILES, "DATAFILES"},
      {BLENDER_SYSTEM_SCRIPTS, "SCRIPTS"},
      {BLENDER_SYSTEM_EXTENSIONS, "EXTENSIONS"},
      {BLENDER_SYSTEM_PYTHON, "PYTHON"},
      {0, nullptr},
  };
  PyC_StringEnum type = {type_items};

  PyC_UnicodeAsBytesAndSize_Data subdir_data = {nullptr};

  static const char *_keywords[] = {"type", "path", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O&" /* `type` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `path` */
      ":system_resource",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseStringEnum,
                                        &type,
                                        PyC_ParseUnicodeAsBytesAndSize,
                                        &subdir_data))
  {
    return nullptr;
  }

  std::optional<std::string> path = BKE_appdir_folder_id(type.value_found, subdir_data.value);
  Py_XDECREF(subdir_data.value_coerce);

  return PyC_UnicodeFromStdStr(path.value_or(""));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_resource_path_doc,
    ".. function:: resource_path(type, *, major=bpy.app.version[0], minor=bpy.app.version[1])\n"
    "\n"
    "   Return the base path for storing system files.\n"
    "\n"
    "   :arg type: string in ['USER', 'LOCAL', 'SYSTEM'].\n"
    "   :type type: str\n"
    "   :arg major: major version, defaults to current.\n"
    "   :type major: int\n"
    "   :arg minor: minor version, defaults to current.\n"
    "   :type minor: str\n"
    "   :return: the resource path (not necessarily existing).\n"
    "   :rtype: str\n");
static PyObject *bpy_resource_path(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  const PyC_StringEnumItems type_items[] = {
      {BLENDER_RESOURCE_PATH_USER, "USER"},
      {BLENDER_RESOURCE_PATH_LOCAL, "LOCAL"},
      {BLENDER_RESOURCE_PATH_SYSTEM, "SYSTEM"},
      {0, nullptr},
  };
  PyC_StringEnum type = {type_items};

  int major = BLENDER_VERSION / 100, minor = BLENDER_VERSION % 100;

  static const char *_keywords[] = {"type", "major", "minor", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O&" /* `type` */
      "|$" /* Optional keyword only arguments. */
      "i"  /* `major` */
      "i"  /* `minor` */
      ":resource_path",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, PyC_ParseStringEnum, &type, &major, &minor))
  {
    return nullptr;
  }

  const std::optional<std::string> path = BKE_appdir_resource_path_id_with_version(
      type.value_found, false, (major * 100) + minor);

  return PyC_UnicodeFromStdStr(path.value_or(""));
}

/* This is only exposed for tests, see: `tests/python/bl_pyapi_bpy_driver_secure_eval.py`. */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_driver_secure_code_test_doc,
    ".. function:: _driver_secure_code_test(code, *, namespace=None, verbose=False)\n"
    "\n"
    "   Test if the script should be considered trusted.\n"
    "\n"
    "   :arg code: The code to test.\n"
    "   :type code: code\n"
    "   :arg namespace: The namespace of values which are allowed.\n"
    "   :type namespace: dict[str, Any]\n"
    "   :arg verbose: Print the reason for considering insecure to the ``stderr``.\n"
    "   :type verbose: bool\n"
    "   :return: True when the script is considered trusted.\n"
    "   :rtype: bool\n");
static PyObject *bpy_driver_secure_code_test(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  PyObject *py_code;
  PyObject *py_namespace = nullptr;
  const bool verbose = false;
  static const char *_keywords[] = {"code", "namespace", "verbose", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O!" /* `expression` */
      "|$" /* Optional keyword only arguments. */
      "O!" /* `namespace` */
      "O&" /* `verbose` */
      ":driver_secure_code_test",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        &PyCode_Type,
                                        &py_code,
                                        &PyDict_Type,
                                        &py_namespace,
                                        PyC_ParseBool,
                                        &verbose))
  {
    return nullptr;
  }
  return PyBool_FromLong(BPY_driver_secure_bytecode_test(py_code, py_namespace, verbose));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_escape_identifier_doc,
    ".. function:: escape_identifier(string)\n"
    "\n"
    "   Simple string escaping function used for animation paths.\n"
    "\n"
    "   :arg string: text\n"
    "   :type string: str\n"
    "   :return: The escaped string.\n"
    "   :rtype: str\n");
static PyObject *bpy_escape_identifier(PyObject * /*self*/, PyObject *value)
{
  Py_ssize_t value_str_len;
  const char *value_str = PyUnicode_AsUTF8AndSize(value, &value_str_len);

  if (value_str == nullptr) {
    PyErr_SetString(PyExc_TypeError, "expected a string");
    return nullptr;
  }

  const size_t size = (value_str_len * 2) + 1;
  char *value_escape_str = static_cast<char *>(PyMem_MALLOC(size));
  const Py_ssize_t value_escape_str_len = BLI_str_escape(value_escape_str, value_str, size);

  PyObject *value_escape;
  if (value_escape_str_len == value_str_len) {
    Py_INCREF(value);
    value_escape = value;
  }
  else {
    value_escape = PyUnicode_FromStringAndSize(value_escape_str, value_escape_str_len);
  }

  PyMem_FREE(value_escape_str);

  return value_escape;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_unescape_identifier_doc,
    ".. function:: unescape_identifier(string)\n"
    "\n"
    "   Simple string un-escape function used for animation paths.\n"
    "   This performs the reverse of :func:`escape_identifier`.\n"
    "\n"
    "   :arg string: text\n"
    "   :type string: str\n"
    "   :return: The un-escaped string.\n"
    "   :rtype: str\n");
static PyObject *bpy_unescape_identifier(PyObject * /*self*/, PyObject *value)
{
  Py_ssize_t value_str_len;
  const char *value_str = PyUnicode_AsUTF8AndSize(value, &value_str_len);

  if (value_str == nullptr) {
    PyErr_SetString(PyExc_TypeError, "expected a string");
    return nullptr;
  }

  const size_t size = value_str_len + 1;
  char *value_unescape_str = static_cast<char *>(PyMem_MALLOC(size));
  const Py_ssize_t value_unescape_str_len = BLI_str_unescape(value_unescape_str, value_str, size);

  PyObject *value_unescape;
  if (value_unescape_str_len == value_str_len) {
    Py_INCREF(value);
    value_unescape = value;
  }
  else {
    value_unescape = PyUnicode_FromStringAndSize(value_unescape_str, value_unescape_str_len);
  }

  PyMem_FREE(value_unescape_str);

  return value_unescape;
}

extern "C" const char *buttons_context_dir[];
extern "C" const char *clip_context_dir[];
extern "C" const char *file_context_dir[];
extern "C" const char *image_context_dir[];
extern "C" const char *node_context_dir[];
extern "C" const char *screen_context_dir[];
extern "C" const char *sequencer_context_dir[];
extern "C" const char *text_context_dir[];
extern "C" const char *view3d_context_dir[];

/**
 * \note only exposed for generating documentation, see: `doc/python_api/sphinx_doc_gen.py`.
 */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_context_members_doc,
    ".. function:: context_members()\n"
    "\n"
    "   :return: A dict where the key is the context and the value is a tuple of it's members.\n"
    "   :rtype: dict[str, tuple[str]]\n");
static PyObject *bpy_context_members(PyObject * /*self*/)
{

  struct {
    const char *name;
    const char **dir;
  } context_members_all[] = {
      {"buttons", buttons_context_dir},
      {"clip", clip_context_dir},
      {"file", file_context_dir},
      {"image", image_context_dir},
      {"node", node_context_dir},
      {"screen", screen_context_dir},
      {"sequencer", sequencer_context_dir},
      {"text", text_context_dir},
      {"view3d", view3d_context_dir},
  };

  PyObject *result = _PyDict_NewPresized(ARRAY_SIZE(context_members_all));
  for (int context_index = 0; context_index < ARRAY_SIZE(context_members_all); context_index++) {
    const char *name = context_members_all[context_index].name;
    const char **dir = context_members_all[context_index].dir;
    int i;
    for (i = 0; dir[i]; i++) {
      /* Pass. */
    }
    PyObject *members = PyTuple_New(i);
    for (i = 0; dir[i]; i++) {
      PyTuple_SET_ITEM(members, i, PyUnicode_FromString(dir[i]));
    }
    PyDict_SetItemString(result, name, members);
    Py_DECREF(members);
  }
  BLI_assert(PyDict_GET_SIZE(result) == ARRAY_SIZE(context_members_all));

  return result;
}

/**
 * \note only exposed for generating documentation, see: `doc/python_api/sphinx_doc_gen.py`.
 */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_rna_enum_items_static_doc,
    ".. function:: rna_enum_items_static()\n"
    "\n"
    "   :return: A dict where the key the name of the enum, the value is a tuple of enum items.\n"
    "   :rtype: dict[str, tuple[:class:`bpy.types.EnumPropertyItem`]]\n");
static PyObject *bpy_rna_enum_items_static(PyObject * /*self*/)
{
#define DEF_ENUM(id) {STRINGIFY(id), id},
  struct {
    const char *id;
    const EnumPropertyItem *items;
  } enum_info[] = {
#include "RNA_enum_items.hh"
  };
  PyObject *result = _PyDict_NewPresized(ARRAY_SIZE(enum_info));
  for (int i = 0; i < ARRAY_SIZE(enum_info); i++) {
    /* Include all items (including headings & separators), can be shown in documentation. */
    const EnumPropertyItem *items = enum_info[i].items;
    const int items_count = RNA_enum_items_count(items);
    PyObject *value = PyTuple_New(items_count);
    for (int item_index = 0; item_index < items_count; item_index++) {
      PointerRNA ptr = RNA_pointer_create_discrete(
          nullptr, &RNA_EnumPropertyItem, (void *)&items[item_index]);
      PyTuple_SET_ITEM(value, item_index, pyrna_struct_CreatePyObject(&ptr));
    }
    PyDict_SetItemString(result, enum_info[i].id, value);
    Py_DECREF(value);
  }
  return result;
}

/* This is only exposed for (Unix/Linux), see: #GHOST_ISystem::getSystemBackend for details. */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_ghost_backend_doc,
    ".. function:: _ghost_backend()\n"
    "\n"
    "   :return: An identifier for the GHOST back-end.\n"
    "   :rtype: str\n");
static PyObject *bpy_ghost_backend(PyObject * /*self*/)
{
  return PyUnicode_FromString(WM_ghost_backend());
}

/* NOTE(@ideasman42): This is a private function because the keys in the returned dictionary,
 * are not considered stable. Sometimes a function is temporarily only supported by one platform.
 * Once all platforms support the functionality there is no need for the flag
 * and it can be removed. This is at odds with a public API that has values which are
 * intended to be kept between releases.
 * If this were to be made public we would have to document that this is subject to change. */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_wm_capabilities_doc,
    ".. function:: _wm_capabilities()\n"
    "\n"
    "   :return: A dictionary of capabilities (string keys, boolean values).\n"
    "   :rtype: dict[str, bool]\n");
static PyObject *bpy_wm_capabilities(PyObject *self)
{
  PyObject *py_id_capabilities = PyUnicode_FromString("_wm_capabilities_");
  PyObject *result = nullptr;
  switch (PyObject_GetOptionalAttr(self, py_id_capabilities, &result)) {
    case 1: {
      BLI_assert(result != nullptr);
      break;
    }
    case 0: {
      result = PyDict_New();

      const eWM_CapabilitiesFlag flag = WM_capabilities_flag();

#define SetFlagItem(x) \
  PyDict_SetItemString(result, STRINGIFY(x), PyBool_FromLong((WM_CAPABILITY_##x) & flag));

      /* Only exposed flags which are used, by Blender's built-in scripts
       * since this is a private API. */

      SetFlagItem(TRACKPAD_PHYSICAL_DIRECTION);
      SetFlagItem(KEYBOARD_HYPER_KEY);

#undef SetFlagItem
      PyObject_SetAttr(self, py_id_capabilities, result);
      break;
    }
    default:
      /* Unlikely, but there may be an error, forward it. */
      BLI_assert(result == nullptr);
      break;
  }

  Py_DECREF(py_id_capabilities);
  return result;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef bpy_methods[] = {
    {"script_paths", (PyCFunction)bpy_script_paths, METH_NOARGS, bpy_script_paths_doc},
    {"blend_paths",
     (PyCFunction)bpy_blend_paths,
     METH_VARARGS | METH_KEYWORDS,
     bpy_blend_paths_doc},
    {"flip_name", (PyCFunction)bpy_flip_name, METH_VARARGS | METH_KEYWORDS, bpy_flip_name_doc},
    {"user_resource", (PyCFunction)bpy_user_resource, METH_VARARGS | METH_KEYWORDS, nullptr},
    {"system_resource",
     (PyCFunction)bpy_system_resource,
     METH_VARARGS | METH_KEYWORDS,
     bpy_system_resource_doc},
    {"resource_path",
     (PyCFunction)bpy_resource_path,
     METH_VARARGS | METH_KEYWORDS,
     bpy_resource_path_doc},
    {"escape_identifier", (PyCFunction)bpy_escape_identifier, METH_O, bpy_escape_identifier_doc},
    {"unescape_identifier",
     (PyCFunction)bpy_unescape_identifier,
     METH_O,
     bpy_unescape_identifier_doc},
    {"context_members", (PyCFunction)bpy_context_members, METH_NOARGS, bpy_context_members_doc},
    {"rna_enum_items_static",
     (PyCFunction)bpy_rna_enum_items_static,
     METH_NOARGS,
     bpy_rna_enum_items_static_doc},

    /* Private functions (not part of the public API and may be removed at any time). */
    {"_driver_secure_code_test",
     (PyCFunction)bpy_driver_secure_code_test,
     METH_VARARGS | METH_KEYWORDS,
     bpy_driver_secure_code_test_doc},
    {"_ghost_backend", (PyCFunction)bpy_ghost_backend, METH_NOARGS, bpy_ghost_backend_doc},
    {"_wm_capabilities", (PyCFunction)bpy_wm_capabilities, METH_NOARGS, bpy_wm_capabilities_doc},

    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyObject *bpy_import_test(const char *modname)
{
  PyObject *mod = PyImport_ImportModuleLevel(modname, nullptr, nullptr, nullptr, 0);

  if (mod) {
    Py_DECREF(mod);
  }
  else {
    PyErr_Print();
  }

  return mod;
}

void BPy_init_modules(bContext *C)
{
  PyObject *mod;

  /* Needs to be first since this dir is needed for future modules */
  const std::optional<std::string> modpath = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS,
                                                                  "modules");
  if (modpath.has_value()) {
    // printf("bpy: found module path '%s'.\n", modpath);
    PyObject *sys_path = PySys_GetObject("path"); /* borrow */
    PyObject *py_modpath = PyC_UnicodeFromStdStr(modpath.value());
    PyList_Insert(sys_path, 0, py_modpath); /* add first */
    Py_DECREF(py_modpath);
  }
  else {
    printf("bpy: couldn't find 'scripts/modules', blender probably won't start.\n");
  }
  /* stand alone utility modules not related to blender directly */
  IDProp_Init_Types(); /* not actually a submodule, just types */
  IDPropertyUIData_Init_Types();
#ifdef WITH_FREESTYLE
  Freestyle_Init();
#endif

  mod = PyModule_New("_bpy");

  /* add the module so we can import it */
  PyDict_SetItemString(PyImport_GetModuleDict(), "_bpy", mod);
  Py_DECREF(mod);

  /* Needs to be first so `_bpy_types` can run. */
  PyObject *bpy_types = BPY_rna_types();
  PyModule_AddObject(bpy_types, "GeometrySet", BPyInit_geometry_set_type());
  PyModule_AddObject(bpy_types, "InlineShaderNodes", BPyInit_inline_shader_nodes_type());
  PyModule_AddObject(mod, "types", bpy_types);

  /* Needs to be first so `_bpy_types` can run. */
  BPY_library_load_type_ready();

  BPY_rna_data_context_type_ready();

  BPY_rna_gizmo_module(mod);

  /* Important to internalizes `_bpy_types` before creating RNA instances. */
  {
    /* Set a dummy module so the `_bpy_types.py` can access `bpy.types.ID`
     * without a null pointer dereference when instancing types. */
    PyObject *bpy_types_dict_dummy = PyDict_New();
    BPY_rna_types_dict_set(bpy_types_dict_dummy);
    PyObject *bpy_types_module_py = bpy_import_test("_bpy_types");
    /* Something has gone wrong if this is ever populated. */
    BLI_assert(PyDict_GET_SIZE(bpy_types_dict_dummy) == 0);
    Py_DECREF(bpy_types_dict_dummy);

    PyObject *bpy_types_module_py_dict = PyModule_GetDict(bpy_types_module_py);
    BPY_rna_types_dict_set(bpy_types_module_py_dict);
  }
  PyModule_AddObject(mod, "data", BPY_rna_module());
  BPY_rna_types_finalize_external_types(bpy_types);

  PyModule_AddObject(mod, "props", BPY_rna_props());
  PyModule_AddObject(mod, "ops", BPY_operator_module());
  PyModule_AddObject(mod, "app", BPY_app_struct());
  PyModule_AddObject(mod, "_utils_units", BPY_utils_units());
  PyModule_AddObject(mod, "_utils_previews", BPY_utils_previews_module());
  PyModule_AddObject(mod, "msgbus", BPY_msgbus_module());

  PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Context, C);
  bpy_context_module = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ctx_ptr);
  PyModule_AddObject(mod, "context", (PyObject *)bpy_context_module);

  /* Register methods and property get/set for RNA types. */
  BPY_rna_types_extend_capi();

#define PYMODULE_ADD_METHOD(mod, meth) \
  PyModule_AddObject(mod, (meth)->ml_name, (PyObject *)PyCFunction_New(meth, mod))

  for (int i = 0; bpy_methods[i].ml_name; i++) {
    PyMethodDef *m = &bpy_methods[i];
    /* Currently there is no need to support these. */
    BLI_assert((m->ml_flags & (METH_CLASS | METH_STATIC)) == 0);
    PYMODULE_ADD_METHOD(mod, m);
  }

  /* Register functions (`bpy_rna.cc`). */
  PYMODULE_ADD_METHOD(mod, &meth_bpy_register_class);
  PYMODULE_ADD_METHOD(mod, &meth_bpy_unregister_class);

  PYMODULE_ADD_METHOD(mod, &meth_bpy_owner_id_get);
  PYMODULE_ADD_METHOD(mod, &meth_bpy_owner_id_set);

  /* Register command functions. */
  PYMODULE_ADD_METHOD(mod, &BPY_cli_command_register_def);
  PYMODULE_ADD_METHOD(mod, &BPY_cli_command_unregister_def);

#undef PYMODULE_ADD_METHOD

  /* add our own modules dir, this is a python package */
  bpy_package_py = bpy_import_test("bpy");
}
