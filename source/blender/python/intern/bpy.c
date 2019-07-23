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
 * \ingroup pythonintern
 *
 * This file defines the '_bpy' module which is used by python's 'bpy' package
 * to access C defined builtin functions.
 * A script writer should never directly access this module.
 */

#include <Python.h>

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_appdir.h"
#include "BKE_global.h" /* XXX, G_MAIN only */
#include "BKE_blender_version.h"
#include "BKE_bpath.h"

#include "RNA_types.h"
#include "RNA_access.h"

#include "bpy.h"
#include "bpy_capi_utils.h"
#include "bpy_rna.h"
#include "bpy_app.h"
#include "bpy_rna_id_collection.h"
#include "bpy_rna_gizmo.h"
#include "bpy_props.h"
#include "bpy_library.h"
#include "bpy_operator.h"
#include "bpy_utils_previews.h"
#include "bpy_utils_units.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

/* external util modules */
#include "../generic/idprop_py_api.h"
#include "bpy_msgbus.h"

#ifdef WITH_FREESTYLE
#  include "BPy_Freestyle.h"
#endif

PyObject *bpy_package_py = NULL;

PyDoc_STRVAR(bpy_script_paths_doc,
             ".. function:: script_paths()\n"
             "\n"
             "   Return 2 paths to blender scripts directories.\n"
             "\n"
             "   :return: (system, user) strings will be empty when not found.\n"
             "   :rtype: tuple of strings\n");
static PyObject *bpy_script_paths(PyObject *UNUSED(self))
{
  PyObject *ret = PyTuple_New(2);
  PyObject *item;
  const char *path;

  path = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, NULL);
  item = PyC_UnicodeFromByte(path ? path : "");
  BLI_assert(item != NULL);
  PyTuple_SET_ITEM(ret, 0, item);
  path = BKE_appdir_folder_id(BLENDER_USER_SCRIPTS, NULL);
  item = PyC_UnicodeFromByte(path ? path : "");
  BLI_assert(item != NULL);
  PyTuple_SET_ITEM(ret, 1, item);

  return ret;
}

static bool bpy_blend_paths_visit_cb(void *userdata, char *UNUSED(path_dst), const char *path_src)
{
  PyList_APPEND((PyObject *)userdata, PyC_UnicodeFromByte(path_src));
  return false; /* never edits the path */
}

PyDoc_STRVAR(bpy_blend_paths_doc,
             ".. function:: blend_paths(absolute=False, packed=False, local=False)\n"
             "\n"
             "   Returns a list of paths to external files referenced by the loaded .blend file.\n"
             "\n"
             "   :arg absolute: When true the paths returned are made absolute.\n"
             "   :type absolute: boolean\n"
             "   :arg packed: When true skip file paths for packed data.\n"
             "   :type packed: boolean\n"
             "   :arg local: When true skip linked library paths.\n"
             "   :type local: boolean\n"
             "   :return: path list.\n"
             "   :rtype: list of strings\n");
static PyObject *bpy_blend_paths(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  int flag = 0;
  PyObject *list;

  bool absolute = false;
  bool packed = false;
  bool local = false;

  static const char *_keywords[] = {"absolute", "packed", "local", NULL};
  static _PyArg_Parser _parser = {"|O&O&O&:blend_paths", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseBool,
                                        &absolute,
                                        PyC_ParseBool,
                                        &packed,
                                        PyC_ParseBool,
                                        &local)) {
    return NULL;
  }

  if (absolute) {
    flag |= BKE_BPATH_TRAVERSE_ABS;
  }
  if (!packed) {
    flag |= BKE_BPATH_TRAVERSE_SKIP_PACKED;
  }
  if (local) {
    flag |= BKE_BPATH_TRAVERSE_SKIP_LIBRARY;
  }

  list = PyList_New(0);

  BKE_bpath_traverse_main(G_MAIN, bpy_blend_paths_visit_cb, flag, (void *)list);

  return list;
}

// PyDoc_STRVAR(bpy_user_resource_doc[] = // now in bpy/utils.py
static PyObject *bpy_user_resource(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  const char *type;
  const char *subdir = NULL;
  int folder_id;

  const char *path;

  static const char *_keywords[] = {"type", "subdir", NULL};
  static _PyArg_Parser _parser = {"s|s:user_resource", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &type, &subdir)) {
    return NULL;
  }

  /* stupid string compare */
  if (STREQ(type, "DATAFILES")) {
    folder_id = BLENDER_USER_DATAFILES;
  }
  else if (STREQ(type, "CONFIG")) {
    folder_id = BLENDER_USER_CONFIG;
  }
  else if (STREQ(type, "SCRIPTS")) {
    folder_id = BLENDER_USER_SCRIPTS;
  }
  else if (STREQ(type, "AUTOSAVE")) {
    folder_id = BLENDER_USER_AUTOSAVE;
  }
  else {
    PyErr_SetString(PyExc_ValueError, "invalid resource argument");
    return NULL;
  }

  /* same logic as BKE_appdir_folder_id_create(),
   * but best leave it up to the script author to create */
  path = BKE_appdir_folder_id_user_notest(folder_id, subdir);

  return PyC_UnicodeFromByte(path ? path : "");
}

PyDoc_STRVAR(bpy_system_resource_doc,
             ".. function:: system_resource(type, path=\"\")\n"
             "\n"
             "   Return a system resource path.\n"
             "\n"
             "   :arg type: string in ['DATAFILES', 'SCRIPTS', 'PYTHON'].\n"
             "   :type type: string\n"
             "   :arg path: Optional subdirectory.\n"
             "   :type path: string\n");
static PyObject *bpy_system_resource(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  const char *type;
  const char *subdir = NULL;
  int folder_id;

  const char *path;

  static const char *_keywords[] = {"type", "path", NULL};
  static _PyArg_Parser _parser = {"s|s:system_resource", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &type, &subdir)) {
    return NULL;
  }

  /* stupid string compare */
  if (STREQ(type, "DATAFILES")) {
    folder_id = BLENDER_SYSTEM_DATAFILES;
  }
  else if (STREQ(type, "SCRIPTS")) {
    folder_id = BLENDER_SYSTEM_SCRIPTS;
  }
  else if (STREQ(type, "PYTHON")) {
    folder_id = BLENDER_SYSTEM_PYTHON;
  }
  else {
    PyErr_SetString(PyExc_ValueError, "invalid resource argument");
    return NULL;
  }

  path = BKE_appdir_folder_id(folder_id, subdir);

  return PyC_UnicodeFromByte(path ? path : "");
}

PyDoc_STRVAR(
    bpy_resource_path_doc,
    ".. function:: resource_path(type, major=bpy.app.version[0], minor=bpy.app.version[1])\n"
    "\n"
    "   Return the base path for storing system files.\n"
    "\n"
    "   :arg type: string in ['USER', 'LOCAL', 'SYSTEM'].\n"
    "   :type type: string\n"
    "   :arg major: major version, defaults to current.\n"
    "   :type major: int\n"
    "   :arg minor: minor version, defaults to current.\n"
    "   :type minor: string\n"
    "   :return: the resource path (not necessarily existing).\n"
    "   :rtype: string\n");
static PyObject *bpy_resource_path(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  const char *type;
  int major = BLENDER_VERSION / 100, minor = BLENDER_VERSION % 100;
  int folder_id;
  const char *path;

  static const char *_keywords[] = {"type", "major", "minor", NULL};
  static _PyArg_Parser _parser = {"s|ii:resource_path", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &type, &major, &minor)) {
    return NULL;
  }

  /* stupid string compare */
  if (STREQ(type, "USER")) {
    folder_id = BLENDER_RESOURCE_PATH_USER;
  }
  else if (STREQ(type, "LOCAL")) {
    folder_id = BLENDER_RESOURCE_PATH_LOCAL;
  }
  else if (STREQ(type, "SYSTEM")) {
    folder_id = BLENDER_RESOURCE_PATH_SYSTEM;
  }
  else {
    PyErr_SetString(PyExc_ValueError, "invalid resource argument");
    return NULL;
  }

  path = BKE_appdir_folder_id_version(folder_id, (major * 100) + minor, false);

  return PyC_UnicodeFromByte(path ? path : "");
}

PyDoc_STRVAR(bpy_escape_identifier_doc,
             ".. function:: escape_identifier(string)\n"
             "\n"
             "   Simple string escaping function used for animation paths.\n"
             "\n"
             "   :arg string: text\n"
             "   :type string: string\n"
             "   :return: The escaped string.\n"
             "   :rtype: string\n");
static PyObject *bpy_escape_identifier(PyObject *UNUSED(self), PyObject *value)
{
  const char *value_str;
  Py_ssize_t value_str_len;

  char *value_escape_str;
  Py_ssize_t value_escape_str_len;
  PyObject *value_escape;
  size_t size;

  value_str = _PyUnicode_AsStringAndSize(value, &value_str_len);

  if (value_str == NULL) {
    PyErr_SetString(PyExc_TypeError, "expected a string");
    return NULL;
  }

  size = (value_str_len * 2) + 1;
  value_escape_str = PyMem_MALLOC(size);
  value_escape_str_len = BLI_strescape(value_escape_str, value_str, size);

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

static PyMethodDef meth_bpy_script_paths = {
    "script_paths",
    (PyCFunction)bpy_script_paths,
    METH_NOARGS,
    bpy_script_paths_doc,
};
static PyMethodDef meth_bpy_blend_paths = {
    "blend_paths",
    (PyCFunction)bpy_blend_paths,
    METH_VARARGS | METH_KEYWORDS,
    bpy_blend_paths_doc,
};
static PyMethodDef meth_bpy_user_resource = {
    "user_resource",
    (PyCFunction)bpy_user_resource,
    METH_VARARGS | METH_KEYWORDS,
    NULL,
};
static PyMethodDef meth_bpy_system_resource = {
    "system_resource",
    (PyCFunction)bpy_system_resource,
    METH_VARARGS | METH_KEYWORDS,
    bpy_system_resource_doc,
};
static PyMethodDef meth_bpy_resource_path = {
    "resource_path",
    (PyCFunction)bpy_resource_path,
    METH_VARARGS | METH_KEYWORDS,
    bpy_resource_path_doc,
};
static PyMethodDef meth_bpy_escape_identifier = {
    "escape_identifier",
    (PyCFunction)bpy_escape_identifier,
    METH_O,
    bpy_escape_identifier_doc,
};

static PyObject *bpy_import_test(const char *modname)
{
  PyObject *mod = PyImport_ImportModuleLevel(modname, NULL, NULL, NULL, 0);
  if (mod) {
    Py_DECREF(mod);
  }
  else {
    PyErr_Print();
    PyErr_Clear();
  }

  return mod;
}

/******************************************************************************
 * Description: Creates the bpy module and adds it to sys.modules for importing
 ******************************************************************************/
void BPy_init_modules(void)
{
  PointerRNA ctx_ptr;
  PyObject *mod;

  /* Needs to be first since this dir is needed for future modules */
  const char *const modpath = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, "modules");
  if (modpath) {
    // printf("bpy: found module path '%s'.\n", modpath);
    PyObject *sys_path = PySys_GetObject("path"); /* borrow */
    PyObject *py_modpath = PyUnicode_FromString(modpath);
    PyList_Insert(sys_path, 0, py_modpath); /* add first */
    Py_DECREF(py_modpath);
  }
  else {
    printf("bpy: couldnt find 'scripts/modules', blender probably wont start.\n");
  }
  /* stand alone utility modules not related to blender directly */
  IDProp_Init_Types(); /* not actually a submodule, just types */
#ifdef WITH_FREESTYLE
  Freestyle_Init();
#endif

  mod = PyModule_New("_bpy");

  /* add the module so we can import it */
  PyDict_SetItemString(PyImport_GetModuleDict(), "_bpy", mod);
  Py_DECREF(mod);

  /* run first, initializes rna types */
  BPY_rna_init();

  /* needs to be first so bpy_types can run */
  PyModule_AddObject(mod, "types", BPY_rna_types());

  /* needs to be first so bpy_types can run */
  BPY_library_load_module(mod);
  BPY_library_write_module(mod);

  BPY_rna_id_collection_module(mod);

  BPY_rna_gizmo_module(mod);

  bpy_import_test("bpy_types");
  PyModule_AddObject(mod, "data", BPY_rna_module()); /* imports bpy_types by running this */
  bpy_import_test("bpy_types");
  PyModule_AddObject(mod, "props", BPY_rna_props());
  /* ops is now a python module that does the conversion from SOME_OT_foo -> some.foo */
  PyModule_AddObject(mod, "ops", BPY_operator_module());
  PyModule_AddObject(mod, "app", BPY_app_struct());
  PyModule_AddObject(mod, "_utils_units", BPY_utils_units());
  PyModule_AddObject(mod, "_utils_previews", BPY_utils_previews_module());
  PyModule_AddObject(mod, "msgbus", BPY_msgbus_module());

  /* bpy context */
  RNA_pointer_create(NULL, &RNA_Context, (void *)BPy_GetContext(), &ctx_ptr);
  bpy_context_module = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ctx_ptr);
  /* odd that this is needed, 1 ref on creation and another for the module
   * but without we get a crash on exit */
  Py_INCREF(bpy_context_module);

  PyModule_AddObject(mod, "context", (PyObject *)bpy_context_module);

  /* register bpy/rna classmethod callbacks */
  BPY_rna_register_cb();

  /* utility func's that have nowhere else to go */
  PyModule_AddObject(mod,
                     meth_bpy_script_paths.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_script_paths, NULL));
  PyModule_AddObject(
      mod, meth_bpy_blend_paths.ml_name, (PyObject *)PyCFunction_New(&meth_bpy_blend_paths, NULL));
  PyModule_AddObject(mod,
                     meth_bpy_user_resource.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_user_resource, NULL));
  PyModule_AddObject(mod,
                     meth_bpy_system_resource.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_system_resource, NULL));
  PyModule_AddObject(mod,
                     meth_bpy_resource_path.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_resource_path, NULL));
  PyModule_AddObject(mod,
                     meth_bpy_escape_identifier.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_escape_identifier, NULL));

  /* register funcs (bpy_rna.c) */
  PyModule_AddObject(mod,
                     meth_bpy_register_class.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_register_class, NULL));
  PyModule_AddObject(mod,
                     meth_bpy_unregister_class.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_unregister_class, NULL));

  PyModule_AddObject(mod,
                     meth_bpy_owner_id_get.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_owner_id_get, NULL));
  PyModule_AddObject(mod,
                     meth_bpy_owner_id_set.ml_name,
                     (PyObject *)PyCFunction_New(&meth_bpy_owner_id_set, NULL));

  /* add our own modules dir, this is a python package */
  bpy_package_py = bpy_import_test("bpy");
}
