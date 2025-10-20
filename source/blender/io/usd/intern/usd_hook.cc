/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_hook.hh"

#include "usd.hh"
#include "usd_asset_utils.hh"
#include "usd_hash_types.hh"
#include "usd_hierarchy_iterator.hh"
#include "usd_reader_prim.hh"
#include "usd_reader_stage.hh"
#include "usd_writer_material.hh"

#include "BLI_map.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_lib_id.hh"
#include "BKE_report.hh"

#include "DNA_material_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"
#include "bpy_rna.hh"

#include <list>
#include <memory>
#include <string>

#include <pxr/external/boost/python/call_method.hpp>
#include <pxr/external/boost/python/class.hpp>
#include <pxr/external/boost/python/dict.hpp>
#include <pxr/external/boost/python/import.hpp>
#include <pxr/external/boost/python/list.hpp>
#include <pxr/external/boost/python/ref.hpp>
#include <pxr/external/boost/python/return_value_policy.hpp>
#include <pxr/external/boost/python/to_python_converter.hpp>
#include <pxr/external/boost/python/tuple.hpp>

using namespace pxr::pxr_boost;

namespace blender::io::usd {

using USDHookList = std::list<std::unique_ptr<USDHook>>;
using ImportedPrimMap = Map<pxr::SdfPath, Vector<PointerRNA>>;

/* USD hook type declarations */
static USDHookList &hook_list()
{
  static USDHookList hooks{};
  return hooks;
}

void USD_register_hook(std::unique_ptr<USDHook> hook)
{
  if (USD_find_hook_name(hook->idname)) {
    /* The hook is already in the list. */
    return;
  }

  /* Add hook type to the list. */
  hook_list().push_back(std::move(hook));
}

void USD_unregister_hook(const USDHook *hook)
{
  hook_list().remove_if(
      [hook](const std::unique_ptr<USDHook> &item) { return item.get() == hook; });
}

USDHook *USD_find_hook_name(const char idname[])
{
  /* sanity checks */
  if (hook_list().empty() || (idname == nullptr) || (idname[0] == 0)) {
    return nullptr;
  }

  USDHookList::iterator hook_iter = std::find_if(
      hook_list().begin(), hook_list().end(), [idname](const std::unique_ptr<USDHook> &item) {
        return STREQ(item->idname, idname);
      });

  return (hook_iter == hook_list().end()) ? nullptr : hook_iter->get();
}

/* Convert PointerRNA to a PyObject*. */
struct PointerRNAToPython {

  /* We pass the argument by value because we need
   * to obtain a non-const pointer to it. */
  static PyObject *convert(PointerRNA ptr)
  {
    return pyrna_struct_CreatePyObject(&ptr);
  }
};

/* Encapsulate arguments for scene export. */
class USDSceneExportContext {
 private:
  pxr::UsdStageRefPtr stage_;
  PointerRNA depsgraph_ptr_;
  const USDHierarchyIterator *hierarchy_iterator_ = nullptr;

 public:
  USDSceneExportContext(const USDHierarchyIterator *iter, Depsgraph *depsgraph)
      : stage_(iter->get_stage()), hierarchy_iterator_(iter)
  {
    depsgraph_ptr_ = RNA_pointer_create_discrete(nullptr, &RNA_Depsgraph, depsgraph);
  }

  pxr::UsdStageRefPtr get_stage() const
  {
    return stage_;
  }

  const PointerRNA &get_depsgraph() const
  {
    return depsgraph_ptr_;
  }

  python::dict get_prim_map() const
  {
    python::dict result;

    const auto &exported_prim_map = hierarchy_iterator_->get_exported_prim_map();
    exported_prim_map.foreach_item([&](const pxr::SdfPath &path, const Vector<ID *> &ids) {
      python::list id_list;
      for (ID *id : ids) {
        if (id) {
          PointerRNA ptr_rna = RNA_id_pointer_create(id);
          id_list.append(ptr_rna);
        }
      }
      result[path] = id_list;
    });
    return result;
  }
};

/* Encapsulate arguments for scene import. */
class USDSceneImportContext {
 private:
  pxr::UsdStageRefPtr stage_;
  ImportedPrimMap prim_map_;
  python::dict *prim_map_dict_ = nullptr;

 public:
  USDSceneImportContext(pxr::UsdStageRefPtr in_stage, const ImportedPrimMap &in_prim_map)
      : stage_(in_stage), prim_map_(in_prim_map)
  {
  }

  void release()
  {
    delete prim_map_dict_;
  }

  pxr::UsdStageRefPtr get_stage() const
  {
    return stage_;
  }

  python::dict get_prim_map()
  {
    if (!prim_map_dict_) {
      prim_map_dict_ = new python::dict;

      prim_map_.foreach_item([&](const pxr::SdfPath &path, const Vector<PointerRNA> &ids) {
        if (!prim_map_dict_->has_key(path)) {
          (*prim_map_dict_)[path] = python::list();
        }

        python::list list = python::extract<python::list>((*prim_map_dict_)[path]);
        for (const auto &ptr_rna : ids) {
          list.append(ptr_rna);
        }
      });
    }

    return *prim_map_dict_;
  }
};

/* Encapsulate arguments for material export. */
class USDMaterialExportContext {
 private:
  pxr::UsdStageRefPtr stage_;
  USDExportParams params_ = {};
  ReportList *reports_ = nullptr;

 public:
  USDMaterialExportContext(pxr::UsdStageRefPtr stage,
                           const USDExportParams &params,
                           ReportList *reports)
      : stage_(stage), params_(params), reports_(reports)
  {
  }

  pxr::UsdStageRefPtr get_stage() const
  {
    return stage_;
  }

  /**
   * Returns the USD asset export path for the given texture image. The image will be copied
   * to the export directory if exporting textures is enabled in the export options.  The
   * function may return an empty string in case of an error.
   */
  std::string export_texture(python::object obj) const
  {
    ID *id;
    if (!pyrna_id_FromPyObject(obj.ptr(), &id)) {
      return "";
    }

    if (!id) {
      return "";
    }

    if (GS(id->name) != ID_IM) {
      return "";
    }

    Image *ima = reinterpret_cast<Image *>(id);

    std::string asset_path = get_tex_image_asset_filepath(ima, stage_, params_);

    if (params_.export_textures) {
      blender::io::usd::export_texture(ima, stage_, params_.overwrite_textures, reports_);
    }

    return asset_path;
  }
};

/* Encapsulate arguments for material import. */
class USDMaterialImportContext {
 private:
  pxr::UsdStageRefPtr stage_;
  USDImportParams params_ = {};
  ReportList *reports_ = nullptr;

 public:
  USDMaterialImportContext(pxr::UsdStageRefPtr stage,
                           const USDImportParams &params,
                           ReportList *reports)
      : stage_(stage), params_(params), reports_(reports)
  {
  }

  pxr::UsdStageRefPtr get_stage() const
  {
    return stage_;
  }

  /**
   * If the given texture asset path is a URI or is relative to a USDZ archive,
   * attempt to copy the texture to the local file system and returns a `tuple[str, bool]`,
   * containing the asset's local path and a boolean indicating whether the path references
   * a temporary file (in the case where imported textures should be packed).
   * The original asset path will be returned unchanged if it's already a local file
   * or if it could not be copied to a local destination.
   */
  python::tuple import_texture(const std::string &asset_path) const
  {
    if (!should_import_asset(asset_path)) {
      /* This path does not need to be imported, so return it unchanged. */
      return python::make_tuple(asset_path, false);
    }

    const char *textures_dir = params_.import_textures_mode == USD_TEX_IMPORT_PACK ?
                                   temp_textures_dir() :
                                   params_.import_textures_dir;

    const eUSDTexNameCollisionMode name_collision_mode = params_.import_textures_mode ==
                                                                 USD_TEX_IMPORT_PACK ?
                                                             USD_TEX_NAME_COLLISION_OVERWRITE :
                                                             params_.tex_name_collision_mode;

    std::string import_path = import_asset(
        asset_path, textures_dir, name_collision_mode, reports_);

    if (import_path == asset_path) {
      /* Path is unchanged. */
      return python::make_tuple(asset_path, false);
    }

    const bool is_temporary = params_.import_textures_mode == USD_TEX_IMPORT_PACK;
    return python::make_tuple(import_path, is_temporary);
  }
};

void register_hook_converters()
{
  static bool registered = false;

  /* No need to register if there are no hooks. */
  if (hook_list().empty()) {
    return;
  }

  if (registered) {
    return;
  }

  registered = true;

  PyGILState_STATE gilstate = PyGILState_Ensure();

  /* We must import these modules for the USD type converters to work. */
  python::import("pxr.Usd");
  python::import("pxr.UsdShade");

  /* Register converter from PoinerRNA to a PyObject*. */
  python::to_python_converter<PointerRNA, PointerRNAToPython>();

  /* Register context class converters. */
  python::class_<USDSceneExportContext>("USDSceneExportContext", python::no_init)
      .def("get_stage", &USDSceneExportContext::get_stage)
      .def("get_depsgraph",
           &USDSceneExportContext::get_depsgraph,
           python::return_value_policy<python::return_by_value>())
      .def("get_prim_map", &USDSceneExportContext::get_prim_map);

  python::class_<USDMaterialExportContext>("USDMaterialExportContext", python::no_init)
      .def("get_stage", &USDMaterialExportContext::get_stage)
      .def("export_texture", &USDMaterialExportContext::export_texture);

  python::class_<USDSceneImportContext>("USDSceneImportContext", python::no_init)
      .def("get_stage", &USDSceneImportContext::get_stage)
      .def("get_prim_map", &USDSceneImportContext::get_prim_map);

  python::class_<USDMaterialImportContext>("USDMaterialImportContext", python::no_init)
      .def("get_stage", &USDMaterialImportContext::get_stage)
      .def("import_texture", &USDMaterialImportContext::import_texture);

  PyGILState_Release(gilstate);
}

/* Retrieve and report the current Python error. */
static void handle_python_error(USDHook *hook, ReportList *reports)
{
  if (!PyErr_Occurred()) {
    return;
  }

  PyErr_Print();

  BKE_reportf(reports,
              RPT_ERROR,
              "An exception occurred invoking USD hook '%s'. Please see the console for details",
              hook->name);
}

/* Abstract base class to facilitate calling a function with a given
 * signature defined by the registered USDHook classes.  Subclasses
 * override virtual methods to specify the hook function name and to
 * call the hook with the required arguments.
 */
class USDHookInvoker {
 private:
  ReportList *reports_;

 public:
  explicit USDHookInvoker(ReportList *reports) : reports_(reports) {}
  virtual ~USDHookInvoker() = default;

  /* Attempt to call the function, if defined by the registered hooks. */
  void call()
  {
    if (hook_list().empty()) {
      return;
    }

    PyGILState_STATE gilstate = PyGILState_Ensure();
    init_in_gil();

    /* Iterate over the hooks and invoke the hook function, if it's defined. */
    USDHookList::const_iterator hook_iter = hook_list().begin();
    while (hook_iter != hook_list().end()) {

      /* XXX: Not sure if this is necessary:
       * Advance the iterator before invoking the callback, to guard
       * against the unlikely error where the hook is de-registered in
       * the callback. This would prevent a crash due to the iterator
       * getting invalidated. */
      USDHook *hook = hook_iter->get();
      ++hook_iter;

      if (!hook->rna_ext.data) {
        continue;
      }

      try {
        PyObject *hook_obj = static_cast<PyObject *>(hook->rna_ext.data);

        if (!PyObject_HasAttrString(hook_obj, function_name())) {
          continue;
        }

        call_hook(hook_obj);
      }
      catch (python::error_already_set const &) {
        handle_python_error(hook, reports_);
      }
      catch (...) {
        BKE_reportf(
            reports_, RPT_ERROR, "An exception occurred invoking USD hook '%s'", hook->name);
      }
    }

    release_in_gil();
    PyGILState_Release(gilstate);
  }

 protected:
  /* Override to specify the name of the function to be called. */
  virtual const char *function_name() const = 0;
  /* Override to call the function of the given object with the
   * required arguments, e.g.,
   *
   * python::call_method<void>(hook_obj, function_name(), arg1, arg2); */
  virtual void call_hook(PyObject *hook_obj) = 0;

  virtual void init_in_gil() {};
  virtual void release_in_gil() {};
};

class OnExportInvoker final : public USDHookInvoker {
 private:
  USDSceneExportContext hook_context_;

 public:
  OnExportInvoker(const USDHierarchyIterator *iter, Depsgraph *depsgraph, ReportList *reports)
      : USDHookInvoker(reports), hook_context_(iter, depsgraph)
  {
  }

 private:
  const char *function_name() const override
  {
    return "on_export";
  }

  void call_hook(PyObject *hook_obj) override
  {
    python::call_method<bool>(hook_obj, function_name(), python::ref(hook_context_));
  }
};

class OnMaterialExportInvoker final : public USDHookInvoker {
 private:
  USDMaterialExportContext hook_context_;
  pxr::UsdShadeMaterial usd_material_;
  PointerRNA material_ptr_;

 public:
  OnMaterialExportInvoker(pxr::UsdStageRefPtr stage,
                          Material *material,
                          const pxr::UsdShadeMaterial &usd_material,
                          const USDExportParams &export_params,
                          ReportList *reports)
      : USDHookInvoker(reports),
        hook_context_(stage, export_params, reports),
        usd_material_(usd_material)
  {
    material_ptr_ = RNA_pointer_create_discrete(nullptr, &RNA_Material, material);
  }

 private:
  const char *function_name() const override
  {
    return "on_material_export";
  }

  void call_hook(PyObject *hook_obj) override
  {
    python::call_method<bool>(
        hook_obj, function_name(), python::ref(hook_context_), material_ptr_, usd_material_);
  }
};

class OnImportInvoker final : public USDHookInvoker {
 private:
  USDSceneImportContext hook_context_;

 public:
  OnImportInvoker(pxr::UsdStageRefPtr stage, const ImportedPrimMap &prim_map, ReportList *reports)
      : USDHookInvoker(reports), hook_context_(stage, prim_map)
  {
  }

 private:
  const char *function_name() const override
  {
    return "on_import";
  }

  void call_hook(PyObject *hook_obj) override
  {
    python::call_method<bool>(hook_obj, function_name(), python::ref(hook_context_));
  }

  void release_in_gil() override
  {
    hook_context_.release();
  }
};

class MaterialImportPollInvoker final : public USDHookInvoker {
 private:
  USDMaterialImportContext hook_context_;
  pxr::UsdShadeMaterial usd_material_;
  bool result_ = false;

 public:
  MaterialImportPollInvoker(pxr::UsdStageRefPtr stage,
                            const pxr::UsdShadeMaterial &usd_material,
                            const USDImportParams &import_params,
                            ReportList *reports)
      : USDHookInvoker(reports),
        hook_context_(stage, import_params, reports),
        usd_material_(usd_material)
  {
  }

  bool result() const
  {
    return result_;
  }

 private:
  const char *function_name() const override
  {
    return "material_import_poll";
  }

  void call_hook(PyObject *hook_obj) override
  {
    /* If we already know that one of the registered hook classes can import the material
     * because it returned true in a previous invocation of the callback, we skip the call. */
    if (!result_) {
      result_ = python::call_method<bool>(
          hook_obj, function_name(), python::ref(hook_context_), usd_material_);
    }
  }
};

class OnMaterialImportInvoker final : public USDHookInvoker {
 private:
  USDMaterialImportContext hook_context_;
  pxr::UsdShadeMaterial usd_material_;
  PointerRNA material_ptr_;
  bool result_ = false;

 public:
  OnMaterialImportInvoker(pxr::UsdStageRefPtr stage,
                          Material *material,
                          const pxr::UsdShadeMaterial &usd_material,
                          const USDImportParams &import_params,
                          ReportList *reports)
      : USDHookInvoker(reports),
        hook_context_(stage, import_params, reports),
        usd_material_(usd_material)
  {
    material_ptr_ = RNA_pointer_create_discrete(nullptr, &RNA_Material, material);
  }

  bool result() const
  {
    return result_;
  }

 private:
  const char *function_name() const override
  {
    return "on_material_import";
  }

  void call_hook(PyObject *hook_obj) override
  {
    result_ |= python::call_method<bool>(
        hook_obj, function_name(), python::ref(hook_context_), material_ptr_, usd_material_);
  }
};

void call_export_hooks(Depsgraph *depsgraph, const USDHierarchyIterator *iter, ReportList *reports)
{
  if (hook_list().empty()) {
    return;
  }

  OnExportInvoker on_export(iter, depsgraph, reports);
  on_export.call();
}

void call_material_export_hooks(pxr::UsdStageRefPtr stage,
                                Material *material,
                                const pxr::UsdShadeMaterial &usd_material,
                                const USDExportParams &export_params,
                                ReportList *reports)
{
  if (hook_list().empty()) {
    return;
  }

  OnMaterialExportInvoker on_material_export(
      stage, material, usd_material, export_params, reports);
  on_material_export.call();
}

void call_import_hooks(USDStageReader *archive, ReportList *reports)
{
  if (hook_list().empty()) {
    return;
  }

  const Vector<USDPrimReader *> &readers = archive->readers();
  const ImportSettings &settings = archive->settings();
  ImportedPrimMap prim_map;

  /* Resize based on the typical scenario where there will be both Object and Data entries
   * in the map in addition to each material. */
  prim_map.reserve((readers.size() * 2) + settings.usd_path_to_mat.size());

  for (const USDPrimReader *reader : readers) {
    if (!reader) {
      continue;
    }

    Object *ob = reader->object();

    prim_map.lookup_or_add_default(reader->object_prim_path())
        .append(RNA_id_pointer_create(&ob->id));
    if (ob->data) {
      prim_map.lookup_or_add_default(reader->data_prim_path())
          .append(RNA_id_pointer_create(static_cast<ID *>(ob->data)));
    }
  }

  settings.usd_path_to_mat.foreach_item([&prim_map](const pxr::SdfPath &path, Material *mat) {
    prim_map.lookup_or_add_default(path).append(RNA_id_pointer_create(&mat->id));
  });

  OnImportInvoker on_import(archive->stage(), prim_map, reports);
  on_import.call();
}

bool have_material_import_hook(pxr::UsdStageRefPtr stage,
                               const pxr::UsdShadeMaterial &usd_material,
                               const USDImportParams &import_params,
                               ReportList *reports)
{
  if (hook_list().empty()) {
    return false;
  }

  MaterialImportPollInvoker poll(stage, usd_material, import_params, reports);
  poll.call();

  return poll.result();
}

bool call_material_import_hooks(pxr::UsdStageRefPtr stage,
                                Material *material,
                                const pxr::UsdShadeMaterial &usd_material,
                                const USDImportParams &import_params,
                                ReportList *reports)
{
  if (hook_list().empty()) {
    return false;
  }

  OnMaterialImportInvoker on_material_import(
      stage, material, usd_material, import_params, reports);
  on_material_import.call();
  return on_material_import.result();
}

}  // namespace blender::io::usd
