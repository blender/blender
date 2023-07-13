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
 *
 */

#ifdef WITH_PYTHON

#  include "usd_umm.h"
#  include "usd.h"
#  include "usd_asset_utils.h"
#  include "usd_exporter_context.h"

#include <boost/python/call.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/import.hpp>
#include <boost/python/list.hpp>

#  include <pxr/usd/sdf/copyUtils.h>

#  include <iostream>

#  include "DNA_material_types.h"
#  include "WM_api.h"

#  include "Python.h"

// The following is additional example code for invoking Python and
// a Blender Python operator from C++:

//#include "BPY_extern_python.h"
//#include "BPY_extern_run.h"

// const char *foo[] = { "bpy", 0 };
// BPY_run_string_eval(C, nullptr, "print('hi!!')");
// BPY_run_string_eval(C, foo, "bpy.ops.universalmaterialmap.instance_to_data_converter()");
// BPY_run_string_eval(C, nullptr, "print('test')");

static PyObject *g_umm_module = nullptr;
static const char *k_umm_module_name = "umm";
static const char *k_export_mtl_func_name = "export_material";
static const char *k_import_mtl_func_name = "import_material";

using namespace blender::io::usd;

using namespace boost;

static void print_obj(PyObject *obj)
{
  if (!obj) {
    return;
  }

  PyObject *str = PyObject_Str(obj);
  if (str && PyUnicode_Check(str)) {
    std::cout << PyUnicode_AsUTF8(str) << std::endl;
    Py_DECREF(str);
  }
}

/* A no-op callback used when impoting textures is turned off. */
static PyObject *import_texture_noop(PyObject *self, PyObject *args)
{
  /* Return the input path unchanged. */
  const char *asset_path;
  if (!PyArg_ParseTuple(args, "s", &asset_path)) {
    return NULL;
  }
  return PyUnicode_FromString(asset_path);
}

static PyMethodDef import_texture_noop_method = {
    "import_texture_noop_cb",
    import_texture_noop,
    METH_VARARGS,
    "A no-op function that returns the input path "
    "argument unchanged, used when texture importing "
    "is turned off."};

static PyObject *import_texture(PyObject *self, PyObject *args)
{
  const char *asset_path = "";
  if (!PyArg_ParseTuple(args, "s", &asset_path)) {
    return NULL;
  }

  if (!should_import_asset(asset_path)) {
    return PyUnicode_FromString(asset_path);
  }

  if (!self) {
    return NULL;
  }

  if (!PyTuple_Check(self)) {
    return NULL;
  }

  if (PyTuple_Size(self) < 2) {
    return NULL;
  }

  PyObject *tex_dir_item = PyTuple_GetItem(self, 0);
  if (!(tex_dir_item && PyUnicode_Check(tex_dir_item))) {
    return NULL;
  }
  const char *tex_dir = PyUnicode_AsUTF8(tex_dir_item);

  PyObject *name_collision_mode_item = PyTuple_GetItem(self, 1);
  if (!(name_collision_mode_item && PyLong_Check(name_collision_mode_item))) {
    return NULL;
  }

  eUSDTexNameCollisionMode name_collision_mode = static_cast<eUSDTexNameCollisionMode>(
      PyLong_AsLong(name_collision_mode_item));

  std::string import_path = import_asset(asset_path, tex_dir, name_collision_mode);

  if (!import_path.empty()) {
    return PyUnicode_FromString(import_path.c_str());
  }

  return PyUnicode_FromString(asset_path);
}

static PyMethodDef import_texture_method = {
    "import_texture",
    import_texture,
    METH_VARARGS,
    "If the given texture asset path is a URI or is "
    "relative to a USDZ arhive, attempt to copy the "
    "texture to the local file system and return the "
    "asset's local path. The source path will be "
    "returned unchanged if it's alreay a local "
    "file or if it could not be copied to a local "
    "destination. The function may return None if "
    "there was a Python error."};


static PyObject *create_import_texture_cb(const USDImportParams &import_params)
{
  if (import_params.import_textures_mode == USD_TEX_IMPORT_NONE) {
    /* Importing textures is turned off, so return a no-op function. */
    return PyCFunction_New(&import_texture_noop_method, NULL);
  }

  /* Create the first 'self' argument for the 'import_textures'
   * function, which is a tuple storing the texture import
   * parameters that will be needed to copy the texture. */

  const char *textures_dir = import_params.import_textures_mode == USD_TEX_IMPORT_PACK ?
                                 temp_textures_dir() :
                                 import_params.import_textures_dir;

  const eUSDTexNameCollisionMode name_collision_mode = import_params.import_textures_mode ==
                                                               USD_TEX_IMPORT_PACK ?
                                                           USD_TEX_NAME_COLLISION_OVERWRITE :
                                                           import_params.tex_name_collision_mode;

  PyObject *import_texture_self = PyTuple_New(2);
  PyObject *tex_dir_item = PyUnicode_FromString(textures_dir);
  PyTuple_SetItem(import_texture_self, 0, tex_dir_item);
  PyObject *collision_mode_item = PyLong_FromLong(static_cast<long>(name_collision_mode));
  PyTuple_SetItem(import_texture_self, 1, collision_mode_item);

  PyObject *new_func = PyCFunction_New(&import_texture_method, import_texture_self);
  Py_DECREF(import_texture_self);

  return new_func;
}

namespace {

enum eUMMNotification {
  UMM_NOTIFICATION_NONE = 0,
  UMM_NOTIFICATION_SUCCESS,
  UMM_NOTIFICATION_FAILURE,
  UMM_NOTIFICATION_BYPASS
};

}  // anonymous namespace

/* Parse the dictionary returned by UMM for an error notification
 * and message.  Report the message in the Blender UI and return
 * the notification enum.  */
static eUMMNotification report_notification(const python::dict &result_dict)
{
  if (result_dict.is_none()) {
    return UMM_NOTIFICATION_NONE;
  }

  if (result_dict.has_key("warnings")) {
    python::extract<python::list> get_list(result_dict["warnings"]);

    if (get_list.check()) {
      python::list warnings = get_list();
      for (int i = 0; i < python::len(warnings); ++i) {
        python::extract<std::string> get_str(warnings[i]);
        if (!get_str.check()) {
          continue;
        }
        std::string warning_str = get_str();
        if (!warning_str.empty()) {
          WM_reportf(RPT_WARNING, "%s", warning_str.c_str());
        }
      }
    }
    else {
      WM_reportf(RPT_WARNING, "%s: 'warnings' value is not a list", __func__);
    }
  }

  std::string notification_str;

  if (result_dict.has_key("umm_notification")) {
    python::extract<std::string> notification_get(result_dict["umm_notification"]);
    if (notification_get.check()) {
      notification_str = notification_get();
    }
    else {
      WM_reportf(RPT_WARNING, "%s: 'umm_notification' value is not a string", __func__);
      return UMM_NOTIFICATION_NONE;
    }
  }

  if (notification_str.empty()) {
    WM_reportf(RPT_WARNING, "%s: Couldn't get 'umm_notification' string value", __func__);
    return UMM_NOTIFICATION_NONE;
  }

  if (notification_str == "success") {
    /* We don't report success, do nothing. */
    return UMM_NOTIFICATION_SUCCESS;
  }

  if (notification_str == "bypass") {
    /* This is a no-op, where no conversion was required.
     * For now, we don't report this. */
    return UMM_NOTIFICATION_BYPASS;
  }

  if (result_dict.has_key("message")) {
    python::extract<std::string> message_get(result_dict["message"]);

    if (!message_get.check()) {
      WM_reportf(RPT_WARNING, "%s: 'message' value is not a string", __func__);
      return UMM_NOTIFICATION_NONE;
    }
    std::string message = message_get();

    if (message.empty()) {
      WM_reportf(RPT_WARNING, "%s: Empty message string value", __func__);
      return UMM_NOTIFICATION_NONE;
    }

    if (notification_str == "unexpected_error") {
      WM_reportf(RPT_ERROR, "%s", message.c_str());
      return UMM_NOTIFICATION_FAILURE;
    }

    WM_reportf(
        RPT_WARNING, "%s: Unsupported notification type '%s'", __func__, notification_str.c_str());
  }

  return UMM_NOTIFICATION_NONE;
}

/* Be sure to call PyGILState_Ensure() before calling this function. */
static bool ensure_module_loaded(bool warn = true)
{

  if (!g_umm_module) {
    g_umm_module = PyImport_ImportModule(k_umm_module_name);
    if (!g_umm_module) {
      if (warn) {
        std::cout << "WARNING: couldn't load Python module " << k_umm_module_name << std::endl;
        if (PyErr_Occurred()) {
          PyErr_Print();
        }
      }
      PyErr_Clear();
    }
  }

  static bool usd_modules_imported = false;

  if (g_umm_module && !usd_modules_imported) {
    usd_modules_imported = true;
    python::import("pxr.Usd");
    python::import("pxr.UsdShade");
  }

  return g_umm_module != nullptr;
}

namespace blender::io::usd {

bool umm_module_loaded()
{
  PyGILState_STATE gilstate = PyGILState_Ensure();

  bool loaded = ensure_module_loaded(false /* warn */);

  PyGILState_Release(gilstate);

  return loaded;
}

bool umm_import_material(const USDImportParams &import_params,
                         Material *mtl,
                         const pxr::UsdShadeMaterial &usd_material,
                         const std::string &render_context)
{
  if (!(mtl && usd_material)) {
    return false;
  }

  PyGILState_STATE gilstate = PyGILState_Ensure();

  if (!ensure_module_loaded()) {
    PyGILState_Release(gilstate);
    return false;
  }

  bool success = true;

  try {

    const char *func_name = k_import_mtl_func_name;

    if (!PyObject_HasAttrString(g_umm_module, func_name)) {
      WM_reportf(
          RPT_ERROR, "%s: module %s has no attribute %s", __func__, k_umm_module_name, func_name);
      return false;
    }

    PyObject *func = PyObject_GetAttrString(g_umm_module, func_name);

    if (!func) {
      WM_reportf(RPT_ERROR,
                 "%s: couldn't get %s module attribute %s",
                 __func__,
                 k_umm_module_name,
                 func_name);
      PyGILState_Release(gilstate);
      return false;
    }

    pxr::UsdStageWeakPtr stage = usd_material.GetPrim().GetStage();

    if (!stage) {
      WM_reportf(RPT_ERROR, "%s:  Couldn't get stage pointer from material", __func__);
      PyGILState_Release(gilstate);
      return false;
    }

    python::dict args_dict;
    args_dict["instance_name"] = std::string(mtl->id.name + 2);
    args_dict["render_context"] = render_context;
    args_dict["mtl_path"] = usd_material.GetPath().GetAsString();
    args_dict["stage"] = stage;

    PyObject *import_tex_cb_arg = create_import_texture_cb(import_params);
    args_dict["import_texture_cb"] = python::object(python::handle<>(import_tex_cb_arg));

    python::dict result = python::call<python::dict>(func, args_dict);

    success = report_notification(result) == UMM_NOTIFICATION_SUCCESS;
  }
  catch (...) {
    if (PyErr_Occurred()) {
      PyErr_Print();
    }
    success = false;
  }

  PyGILState_Release(gilstate);

  return success;
}

bool umm_export_material(const USDExporterContext &usd_export_context,
                         const Material *mtl,
                         const pxr::UsdShadeMaterial &usd_material,
                         const std::string &render_context)
{
  if (!(usd_material && mtl)) {
    return false;
  }

  PyGILState_STATE gilstate = PyGILState_Ensure();

  if (!ensure_module_loaded()) {
    PyGILState_Release(gilstate);
    return false;
  }

  bool success = true;

  try {
    const char *func_name = k_export_mtl_func_name;

    if (!PyObject_HasAttrString(g_umm_module, func_name)) {
      WM_reportf(
          RPT_ERROR, "%s: module %s has no attribute %s", __func__, k_umm_module_name, func_name);
      PyGILState_Release(gilstate);
      return false;
    }

    PyObject *func = PyObject_GetAttrString(g_umm_module, func_name);

    if (!func) {
      WM_reportf(RPT_ERROR,
                 "%s: couldn't get %s module attribute %s",
                 __func__,
                 k_umm_module_name,
                 func_name);
      PyGILState_Release(gilstate);
      return false;
    }

    pxr::UsdStageWeakPtr stage = usd_material.GetPrim().GetStage();

    if (!stage) {
      WM_reportf(RPT_ERROR, "%s:  Couldn't get stage pointer from material", __func__);
      PyGILState_Release(gilstate);
      return false;
    }

    python::dict args_dict;

    args_dict["instance_name"] = std::string(mtl->id.name + 2);
    args_dict["render_context"] = render_context;
    args_dict["mtl_path"] = usd_material.GetPath().GetAsString();
    args_dict["stage"] = stage;

    std::string usd_path = stage->GetRootLayer()->GetRealPath();
    args_dict["usd_path"] = usd_path;

    python::dict result = python::call<python::dict>(func, args_dict);

    success = report_notification(result) == UMM_NOTIFICATION_SUCCESS;
  }
  catch (...) {
    if (PyErr_Occurred()) {
      PyErr_Print();
    }
    success = false;
  }

  PyGILState_Release(gilstate);

  return success;
}

}  // Namespace blender::io::usd

#endif  // ifdef WITH_PYTHON
