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
#  include "usd_writer_material.h"

#  include "DNA_material_types.h"

#  include <pxr/usd/ar/resolver.h>

#  include <iostream>
#  include <vector>

#  include "WM_api.h"

// The following is additional example code for invoking Python and
// a Blender Python operator from C++:

//#include "BPY_extern_python.h"
//#include "BPY_extern_run.h"

// const char *foo[] = { "bpy", 0 };
// BPY_run_string_eval(C, nullptr, "print('hi!!')");
// BPY_run_string_eval(C, foo, "bpy.ops.universalmaterialmap.instance_to_data_converter()");
// BPY_run_string_eval(C, nullptr, "print('test')");

namespace usdtokens {

// Render context names.
static const pxr::TfToken mdl("mdl", pxr::TfToken::Immortal);

}  // end namespace usdtokens

static PyObject *g_umm_module = nullptr;

static const char *k_umm_module_name = "omni.universalmaterialmap.blender.material";

using namespace blender::io::usd;

static std::string anchor_relative_path(pxr::UsdStagePtr stage, const std::string &asset_path)
{
  if (asset_path.empty() || asset_path.front() != '.') {
    return std::string();
  }

  // TODO(makowalski): avoid recomputing the USD path, if possible.
  pxr::SdfLayerHandle layer = stage->GetRootLayer();

  std::string stage_path = layer->GetRealPath();

  if (stage_path.empty()) {
    return asset_path;
  }

#if PXR_VERSION >= 2111
  return pxr::ArGetResolver().CreateIdentifier(asset_path, pxr::ArResolvedPath(stage_path));
#else
  return pxr::ArGetResolver().AnchorRelativePath(stage_path, asset_path);
#endif
}

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

namespace {

enum eUMMNotification {
  UMM_NOTIFICATION_NONE = 0,
  UMM_NOTIFICATION_SUCCESS,
  UMM_NOTIFICATION_FAILURE
};

}  // anonymous namespace

static eUMMNotification report_notification(PyObject *dict)
{
  if (!dict) {
    return UMM_NOTIFICATION_NONE;
  }

  if (!PyDict_Check(dict)) {
    return UMM_NOTIFICATION_NONE;
  }

  PyObject *notification_item = PyDict_GetItemString(dict, "umm_notification");

  if (!notification_item) {
    return UMM_NOTIFICATION_NONE;
  }

  PyObject *message_item = PyDict_GetItemString(dict, "message");

  if (!message_item) {
    return UMM_NOTIFICATION_NONE;
  }

  if (!PyUnicode_Check(notification_item)) {
    std::cerr << "WARNING: 'umm_notification' value is not a string" << std::endl;
    return UMM_NOTIFICATION_NONE;
  }

  const char *notification_str = PyUnicode_AsUTF8(notification_item);

  if (!notification_str) {
    std::cerr << "WARNING: couldn't get 'umm_notification' string value" << std::endl;
    return UMM_NOTIFICATION_NONE;
  }

  if (strcmp(notification_str, "success") == 0) {
    /* We don't report success, do nothing. */
    return UMM_NOTIFICATION_SUCCESS;
  }

  if (!PyUnicode_Check(message_item)) {
    std::cerr << "WARNING: 'message' value is not a string" << std::endl;
    return UMM_NOTIFICATION_NONE;
  }

  const char *message_str = PyUnicode_AsUTF8(message_item);

  if (!message_str) {
    std::cerr << "WARNING: couldn't get 'message' string value" << std::endl;
    return UMM_NOTIFICATION_NONE;
  }

  if (strlen(message_str) == 0) {
    std::cerr << "WARNING: empty 'message' string value" << std::endl;
    return UMM_NOTIFICATION_NONE;
  }

  if (strcmp(notification_str, "incomplete_process") == 0) {
    WM_reportf(RPT_WARNING, "%s", message_str);
    return UMM_NOTIFICATION_FAILURE;
  }

  if (strcmp(notification_str, "unexpected_error") == 0) {
    WM_reportf(RPT_ERROR, "%s", message_str);
    return UMM_NOTIFICATION_FAILURE;
  }

  std::cout << "WARNING: unknown notification type: " << notification_str << std::endl;

  return UMM_NOTIFICATION_NONE;
}

static bool is_none_value(PyObject *tup)
{
  if (!(tup && PyTuple_Check(tup) && PyTuple_Size(tup) > 1)) {
    return false;
  }

  PyObject *second = PyTuple_GetItem(tup, 1);

  return second == Py_None;
}

/* Sets the source asset and source asset subidenifier properties on the given shader
 * with values parsed from the given target_class string. */
static bool set_source_asset(pxr::UsdShadeShader &usd_shader, const std::string &target_class)
{
  if (!usd_shader || target_class.empty()) {
    return false;
  }

  // Split the target_class string on the '|' separator.
  size_t sep = target_class.find_last_of("|");
  if (sep == 0 || sep == std::string::npos) {
    std::cout << "Couldn't parse target_class string " << target_class << std::endl;
    return false;
  }

  std::string source_asset = target_class.substr(0, sep);
  usd_shader.SetSourceAsset(pxr::SdfAssetPath(source_asset), usdtokens::mdl);

  std::string source_asset_subidentifier = target_class.substr(sep + 1);

  if (!source_asset_subidentifier.empty()) {
    usd_shader.SetSourceAssetSubIdentifier(pxr::TfToken(source_asset_subidentifier),
                                           usdtokens::mdl);
  }

  return true;
}

static bool get_data_name(PyObject *tup, std::string &r_name)
{
  if (!(tup && PyTuple_Check(tup) && PyTuple_Size(tup) > 1)) {
    return false;
  }

  PyObject *first = PyTuple_GetItem(tup, 0);

  if (first && PyUnicode_Check(first)) {
    const char *name = PyUnicode_AsUTF8(first);
    if (name) {
      r_name = name;
      return true;
    }
  }

  return false;
}

static bool get_string_data(PyObject *tup, std::string &r_data)
{
  if (!(tup && PyTuple_Check(tup) && PyTuple_Size(tup) > 1)) {
    return false;
  }

  PyObject *second = PyTuple_GetItem(tup, 1);

  if (second && PyUnicode_Check(second)) {
    const char *data = PyUnicode_AsUTF8(second);
    if (data) {
      r_data = data;
      return true;
    }
  }

  return false;
}

//static bool get_float_data(PyObject *tup, float &r_data)
//{
//  if (!(tup && PyTuple_Check(tup) && PyTuple_Size(tup) > 1)) {
//    return false;
//  }
//
//  PyObject *second = PyTuple_GetItem(tup, 1);
//
//  if (second && PyFloat_Check(second)) {
//    r_data = static_cast<float>(PyFloat_AsDouble(second));
//    return true;
//  }
//
//  return false;
//}
//
//static bool get_float3_data(PyObject *tup, float r_data[3])
//{
//  if (!(tup && PyTuple_Check(tup) && PyTuple_Size(tup) > 1)) {
//    return false;
//  }
//
//  PyObject *second = PyTuple_GetItem(tup, 1);
//
//  if (second && PyTuple_Check(second) && PyTuple_Size(second) > 2) {
//    for (int i = 0; i < 3; ++i) {
//      PyObject *comp = PyTuple_GetItem(second, i);
//      if (comp && PyFloat_Check(comp)) {
//        r_data[i] = static_cast<float>(PyFloat_AsDouble(comp));
//      }
//      else {
//        return false;
//      }
//    }
//    return true;
//  }
//
//  return false;
//}
//
//static bool get_rgba_data(PyObject *tup, float r_data[4])
//{
//  if (!(tup && PyTuple_Check(tup) && PyTuple_Size(tup) > 1)) {
//    return false;
//  }
//
//  PyObject *second = PyTuple_GetItem(tup, 1);
//
//  if (!(second && PyTuple_Check(second))) {
//    return false;
//  }
//
//  Py_ssize_t size = PyTuple_Size(second);
//
//  if (size > 2) {
//    for (int i = 0; i < 3; ++i) {
//      PyObject *comp = PyTuple_GetItem(second, i);
//      if (comp && PyFloat_Check(comp)) {
//        r_data[i] = static_cast<float>(PyFloat_AsDouble(comp));
//      }
//      else {
//        return false;
//      }
//    }
//
//    if (size > 3) {
//      PyObject *alpha = PyTuple_GetItem(second, 3);
//      if (alpha && PyFloat_Check(alpha)) {
//        r_data[3] = static_cast<float>(PyFloat_AsDouble(alpha));
//      }
//      else {
//        return false;
//      }
//    }
//    else {
//      r_data[3] = 1.0;
//    }
//
//    return true;
//  }
//
//  return false;
//}

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

  return g_umm_module != nullptr;
}

//static void test_python()
//{
//  PyGILState_STATE gilstate = PyGILState_Ensure();
//
//  PyObject *mod = PyImport_ImportModule("omni.universalmaterialmap.core.converter.util");
//
//  if (mod) {
//    const char *func_name = "get_conversion_manifest";
//
//    if (PyObject_HasAttrString(mod, func_name)) {
//      if (PyObject *func = PyObject_GetAttrString(mod, func_name)) {
//        PyObject *ret = PyObject_CallObject(func, nullptr);
//        Py_DECREF(func);
//
//        if (ret) {
//          print_obj(ret);
//          Py_DECREF(ret);
//        }
//      }
//    }
//  }
//
//  PyGILState_Release(gilstate);
//}

static PyObject *get_shader_source_data(const USDImportParams &params,
                                        const pxr::UsdShadeShader &usd_shader)
{
  if (!usd_shader) {
    return nullptr;
  }

  std::vector<PyObject *> tuple_items;

  std::vector<pxr::UsdShadeInput> inputs = usd_shader.GetInputs();

  for (auto input : inputs) {

    if (!input) {
      continue;
    }

    PyObject *tup = nullptr;

    std::string name = input.GetBaseName().GetString();

    if (name.empty()) {
      continue;
    }

    pxr::UsdAttribute usd_attr;

    bool have_connected_source = false;

    if (input.HasConnectedSource()) {
      pxr::UsdShadeConnectableAPI source;
      pxr::TfToken source_name;
      pxr::UsdShadeAttributeType source_type;

      if (input.GetConnectedSource(&source, &source_name, &source_type)) {
        usd_attr = source.GetInput(source_name).GetAttr();
        have_connected_source = true;
      }
      else {
        std::cerr << "ERROR: couldn't get connected source for usd shader input "
          << input.GetPrim().GetPath() << " " << input.GetFullName() << std::endl;
      }
    }
    else {
      usd_attr = input.GetAttr();
    }

    if (!usd_attr) {
      std::cerr << "ERROR: couldn't get attribute for usd shader input " << input.GetPrim().GetPath()
                << " " << input.GetFullName() << std::endl;
      continue;
    }

    pxr::VtValue val;
    if (!usd_attr.Get(&val)) {
      std::cerr << "ERROR: couldn't get value for usd shader input " << input.GetPrim().GetPath()
                << " " << input.GetFullName() << std::endl;
      continue;
    }

    if (val.IsHolding<float>()) {
      double dval = val.UncheckedGet<float>();
      tup = Py_BuildValue("sd", name.c_str(), dval);
    }
    else if (val.IsHolding<int>()) {
      int ival = val.UncheckedGet<int>();
      tup = Py_BuildValue("si", name.c_str(), ival);
    }
    else if (val.IsHolding<bool>()) {
      int ival = val.UncheckedGet<bool>();
      tup = Py_BuildValue("si", name.c_str(), ival);
    }
    else if (val.IsHolding<pxr::SdfAssetPath>()) {
      pxr::SdfAssetPath asset_path = val.Get<pxr::SdfAssetPath>();

      std::string resolved_path = asset_path.GetResolvedPath();

      if (resolved_path.empty()) {
        /* If the path wasn't resolved, it could be because it's a UDIM path,
         * so try to use the asset path directly, anchoring it if it's a relative path. */
        resolved_path = anchor_relative_path(usd_shader.GetPrim().GetStage(),
                                             asset_path.GetAssetPath());
      }

      const bool import_textures = !resolved_path.empty() &&
                                   params.import_textures_mode != USD_TEX_IMPORT_NONE &&
                                   should_import_asset(resolved_path);

      if (import_textures) {
        /* If we are packing the imported textures, we first write them
         * to a temporary directory. */
        const char *textures_dir = params.import_textures_mode == USD_TEX_IMPORT_PACK ?
                                       temp_textures_dir() :
                                       params.import_textures_dir;

        const eUSDTexNameCollisionMode name_collision_mode = params.import_textures_mode ==
                                                                     USD_TEX_IMPORT_PACK ?
                                                                 USD_TEX_NAME_COLLISION_OVERWRITE :
                                                                 params.tex_name_collision_mode;

        resolved_path = import_asset(resolved_path.c_str(), textures_dir, name_collision_mode);
      }

      pxr::TfToken color_space_tok = usd_attr.GetColorSpace();

      if (color_space_tok.IsEmpty() && have_connected_source) {
        /* The connected asset input has no color space specified,
         * so we also check the shader's asset input for this data. */
        if (pxr::UsdAttribute input_attr = input.GetAttr()) {
          color_space_tok = input_attr.GetColorSpace();
        }
      }

      std::string color_space_str = !color_space_tok.IsEmpty() ? color_space_tok.GetString() :
                                                                 "sRGB";

      PyObject *tex_file_tup = Py_BuildValue("ss", resolved_path.c_str(), color_space_str.c_str());

      tup = Py_BuildValue("sN", name.c_str(), tex_file_tup);
    }
    else if (val.IsHolding<pxr::GfVec3f>()) {
      pxr::GfVec3f v3f = val.UncheckedGet<pxr::GfVec3f>();
      pxr::GfVec3d v3d(v3f);
      PyObject *v3_tup = Py_BuildValue("ddd", v3d[0], v3d[1], v3d[2]);
      if (v3_tup) {
        tup = Py_BuildValue("sN", name.c_str(), v3_tup);
      }
      else {
        std::cout << "Couldn't build v3f tuple for " << usd_shader.GetPath() << " input "
                  << input.GetFullName() << std::endl;
      }
    }
    else if (val.IsHolding<pxr::GfVec2f>()) {
      pxr::GfVec2f v2f = val.UncheckedGet<pxr::GfVec2f>();
      /*     std::cout << "Have v2f input " << v2f << " for "
             << usd_shader.GetPath() << " " << input.GetFullName() << std::endl;*/
      pxr::GfVec2d v2d(v2f);
      PyObject *v2_tup = Py_BuildValue("dd", v2d[0], v2d[1]);
      if (v2_tup) {
        tup = Py_BuildValue("sN", name.c_str(), v2_tup);
      }
      else {
        std::cout << "Couldn't build v2f tuple for " << usd_shader.GetPath() << " input "
                  << input.GetFullName() << std::endl;
      }
    }

    if (tup) {
      tuple_items.push_back(tup);
    }
  }

  PyObject *ret = PyTuple_New(tuple_items.size());

  if (!ret) {
    return nullptr;
  }

  for (int i = 0; i < tuple_items.size(); ++i) {
    if (PyTuple_SetItem(ret, i, tuple_items[i])) {
      std::cout << "error setting tuple item" << std::endl;
    }
  }

  return ret;
}

static bool import_material(const USDImportParams &params,
                            Material *mtl,
                            const pxr::UsdShadeShader &usd_shader,
                            const std::string &source_class)
{
  if (!(usd_shader && mtl)) {
    return false;
  }

  PyGILState_STATE gilstate = PyGILState_Ensure();

  if (!ensure_module_loaded()) {
    PyGILState_Release(gilstate);
    return false;
  }

  const char *func_name = "apply_data_to_instance";

  if (!PyObject_HasAttrString(g_umm_module, func_name)) {
    std::cerr << "WARNING: UMM module has no attribute " << func_name << std::endl;
    PyGILState_Release(gilstate);
    return false;
  }

  PyObject *func = PyObject_GetAttrString(g_umm_module, func_name);

  if (!func) {
    std::cerr << "WARNING: Couldn't get UMM module attribute " << func_name << std::endl;
    PyGILState_Release(gilstate);
    return false;
  }

  PyObject *source_data = get_shader_source_data(params, usd_shader);

  if (!source_data) {
    std::cout << "WARNING:  Couldn't get source data for shader " << usd_shader.GetPath()
              << std::endl;
    PyGILState_Release(gilstate);
    return false;
  }

  // std::cout << "source_data:\n";
  // print_obj(source_data);

  // Create the kwargs dictionary.
  PyObject *kwargs = PyDict_New();

  if (!kwargs) {
    std::cout << "WARNING:  Couldn't create kwargs dicsionary." << std::endl;
    Py_DECREF(source_data);
    PyGILState_Release(gilstate);
    return false;
  }

  PyObject *instance_name = PyUnicode_FromString(mtl->id.name + 2);
  PyDict_SetItemString(kwargs, "instance_name", instance_name);
  Py_DECREF(instance_name);

  PyObject *source_class_obj = PyUnicode_FromString(source_class.c_str());
  PyDict_SetItemString(kwargs, "source_class", source_class_obj);
  Py_DECREF(source_class_obj);

  PyObject *render_context = PyUnicode_FromString("Blender");
  PyDict_SetItemString(kwargs, "render_context", render_context);
  Py_DECREF(render_context);

  PyDict_SetItemString(kwargs, "source_data", source_data);
  Py_DECREF(source_data);

  std::cout << func_name << " arguments:\n";
  print_obj(kwargs);

  PyObject *empty_args = PyTuple_New(0);
  PyObject *ret = PyObject_Call(func, empty_args, kwargs);
  Py_DECREF(empty_args);
  Py_DECREF(func);

  bool success = ret != nullptr;

  if (ret) {
    std::cout << "result:\n";
    print_obj(ret);
    if (report_notification(ret) == UMM_NOTIFICATION_FAILURE) {
      /* The function returned a notification object
       * indicating a failure. */
      success = false;
    }
    Py_DECREF(ret);
  }

  Py_DECREF(kwargs);

  PyGILState_Release(gilstate);

  return success;
}

static void set_shader_properties(const USDExporterContext &usd_export_context,
                                  pxr::UsdShadeShader &usd_shader,
                                  PyObject *data_list)
{
  if (!(data_list && usd_shader)) {
    return;
  }

  if (!PyList_Check(data_list)) {
    return;
  }

  Py_ssize_t len = PyList_Size(data_list);

  for (Py_ssize_t i = 0; i < len; ++i) {
    PyObject *tup = PyList_GetItem(data_list, i);

    if (!tup) {
      continue;
    }

    std::string name;

    if (!get_data_name(tup, name) || name.empty()) {
      std::cout << "Couldn't get data name\n";
      continue;
    }

    if (is_none_value(tup)) {
      /* Receiving None values is not an error. */
      continue;
    }

    if (name == "umm_target_class") {
      std::string target_class;
      if (!get_string_data(tup, target_class) || target_class.empty()) {
        std::cout << "Couldn't get target class\n";
        continue;
      }
      set_source_asset(usd_shader, target_class);
    }
    else {
      if (!(PyTuple_Check(tup) && PyTuple_Size(tup) > 1)) {
        std::cout << "Unexpected data item type or size:\n";
        print_obj(tup);
        continue;
      }

      PyObject *second = PyTuple_GetItem(tup, 1);
      if (!second) {
        std::cout << "Couldn't get second tuple value:\n";
        print_obj(tup);
        continue;
      }

      if (PyFloat_Check(second)) {
        float fval = static_cast<float>(PyFloat_AsDouble(second));
        usd_shader.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float).Set(fval);
      }
      else if (PyBool_Check(second)) {
        bool bval = static_cast<bool>(PyLong_AsLong(second));
        usd_shader.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Bool).Set(bval);
      }
      else if (PyLong_Check(second)) {
        int ival = static_cast<int>(PyLong_AsLong(second));
        usd_shader.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Int).Set(ival);
      }
      else if (PyList_Check(second) && PyList_Size(second) == 2) {
        PyObject *item0 = PyList_GetItem(second, 0);
        PyObject *item1 = PyList_GetItem(second, 1);

        if (PyUnicode_Check(item0) && PyUnicode_Check(item1)) {
          const char *asset = PyUnicode_AsUTF8(item0);

          std::string asset_path = get_tex_image_asset_filepath(
              asset, usd_export_context.stage, usd_export_context.export_params);

          const char *color_space = PyUnicode_AsUTF8(item1);
          pxr::UsdShadeInput asset_input = usd_shader.CreateInput(pxr::TfToken(name),
                                                                  pxr::SdfValueTypeNames->Asset);
          asset_input.Set(pxr::SdfAssetPath(asset_path));
          asset_input.GetAttr().SetColorSpace(pxr::TfToken(color_space));
        }
        else if (PyFloat_Check(item0) && PyFloat_Check(item1)) {
          float f0 = static_cast<float>(PyFloat_AsDouble(item0));
          float f1 = static_cast<float>(PyFloat_AsDouble(item1));
          usd_shader.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float2)
              .Set(pxr::GfVec2f(f0, f1));
        }
      }
      else if (PyTuple_Check(second) && PyTuple_Size(second) == 3) {
        pxr::GfVec3f f3val;
        for (int i = 0; i < 3; ++i) {
          PyObject *comp = PyTuple_GetItem(second, i);
          if (comp && PyFloat_Check(comp)) {
            f3val[i] = static_cast<float>(PyFloat_AsDouble(comp));
          }
          else {
            std::cout << "Couldn't parse color3f " << name << std::endl;
          }
        }
        usd_shader.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Color3f).Set(f3val);
      }
      else {
        std::cout << "Can't handle value:\n";
        print_obj(second);
      }
    }
  }
}

namespace blender::io::usd {

bool umm_module_loaded()
{
  PyGILState_STATE gilstate = PyGILState_Ensure();

  bool loaded = ensure_module_loaded(false /* warn */);

  PyGILState_Release(gilstate);

  return loaded;
}

bool umm_import_mdl_material(const USDImportParams &params,
                             Material *mtl,
                             const pxr::UsdShadeMaterial &usd_material,
                             bool verbose,
                             bool *r_has_mdl)
{
  if (!(mtl && usd_material)) {
    return false;
  }

  /* Get the surface shader. */
  pxr::UsdShadeShader surf_shader = usd_material.ComputeSurfaceSource(usdtokens::mdl);

  if (surf_shader) {
    /* Check if we have an mdl source asset. */
    pxr::SdfAssetPath source_asset;
    if (!surf_shader.GetSourceAsset(&source_asset, usdtokens::mdl)) {
      if (verbose) {
        std::cout << "No mdl source asset for shader " << surf_shader.GetPath() << std::endl;
      }
      if (r_has_mdl) {
        *r_has_mdl = false;
      }
      return false;
    }
    pxr::TfToken source_asset_sub_identifier;
    if (!surf_shader.GetSourceAssetSubIdentifier(&source_asset_sub_identifier, usdtokens::mdl)) {
      if (verbose) {
        std::cout << "No mdl source asset sub identifier for shader " << surf_shader.GetPath()
                  << std::endl;
      }
      if (r_has_mdl) {
        *r_has_mdl = false;
      }
      return false;
    }

    if (r_has_mdl) {
      *r_has_mdl = true;
    }

    std::string path = source_asset.GetAssetPath();

    // Get the filename component of the path.
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
      path = path.substr(last_slash + 1);
    }

    std::string source_class = path + "|" + source_asset_sub_identifier.GetString();
    return import_material(params, mtl, surf_shader, source_class);
  }

  return false;
}

bool umm_export_material(const USDExporterContext &usd_export_context,
                         const Material *mtl,
                         pxr::UsdShadeShader &usd_shader,
                         const std::string &render_context)
{
  if (!(usd_shader && mtl)) {
    return false;
  }

  PyGILState_STATE gilstate = PyGILState_Ensure();

  if (!ensure_module_loaded()) {
    PyGILState_Release(gilstate);
    return false;
  }

  const char *func_name = "convert_instance_to_data";

  if (!PyObject_HasAttrString(g_umm_module, func_name)) {
    std::cerr << "WARNING: UMM module has no attribute " << func_name << std::endl;
    PyGILState_Release(gilstate);
    return false;
  }

  PyObject *func = PyObject_GetAttrString(g_umm_module, func_name);

  if (!func) {
    std::cerr << "WARNING: Couldn't get UMM module attribute " << func_name << std::endl;
    PyGILState_Release(gilstate);
    return false;
  }

  // Create the kwargs dictionary.
  PyObject *kwargs = PyDict_New();

  if (!kwargs) {
    std::cout << "WARNING:  Couldn't create kwargs dicsionary." << std::endl;
    PyGILState_Release(gilstate);
    return false;
  }

  PyObject *instance_name = PyUnicode_FromString(mtl->id.name + 2);
  PyDict_SetItemString(kwargs, "instance_name", instance_name);
  Py_DECREF(instance_name);

  PyObject *render_context_arg = PyUnicode_FromString(render_context.c_str());
  PyDict_SetItemString(kwargs, "render_context", render_context_arg);
  Py_DECREF(render_context_arg);

  std::cout << func_name << " arguments:\n";
  print_obj(kwargs);

  PyObject *empty_args = PyTuple_New(0);
  PyObject *ret = PyObject_Call(func, empty_args, kwargs);
  Py_DECREF(empty_args);
  Py_DECREF(func);

  bool success = ret != nullptr;

  if (ret) {
    std::cout << "result:\n";
    print_obj(ret);

    if (report_notification(ret) == UMM_NOTIFICATION_FAILURE) {
      /* The function returned a notification object
       * indicating a failure. */
      success = false;
    }
    else {
      set_shader_properties(usd_export_context, usd_shader, ret);
    }

    Py_DECREF(ret);
  }

  Py_DECREF(kwargs);

  PyGILState_Release(gilstate);

  return success;
}

}  // Namespace blender::io::usd

#endif  // ifdef WITH_PYTHON
