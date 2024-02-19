/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd.hh"

#include "usd_hook.hh"

#include <boost/python/call_method.hpp>
#include <boost/python/class.hpp>
#include <boost/python/import.hpp>
#include <boost/python/return_value_policy.hpp>
#include <boost/python/to_python_converter.hpp>

#include "BLI_utildefines.h"

#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"
#include "RNA_types.hh"
#include "bpy_rna.h"

#include <list>
#include <memory>

using namespace boost;

namespace blender::io::usd {

using USDHookList = std::list<std::unique_ptr<USDHook>>;

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

void USD_unregister_hook(USDHook *hook)
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
struct USDSceneExportContext {

  USDSceneExportContext() : depsgraph_ptr({}) {}

  USDSceneExportContext(pxr::UsdStageRefPtr in_stage, Depsgraph *depsgraph) : stage(in_stage)
  {
    depsgraph_ptr = RNA_pointer_create(nullptr, &RNA_Depsgraph, depsgraph);
  }

  pxr::UsdStageRefPtr get_stage()
  {
    return stage;
  }

  const PointerRNA &get_depsgraph() const
  {
    return depsgraph_ptr;
  }

  pxr::UsdStageRefPtr stage;
  PointerRNA depsgraph_ptr;
};

/* Encapsulate arguments for scene import. */
struct USDSceneImportContext {

  USDSceneImportContext() {}

  USDSceneImportContext(pxr::UsdStageRefPtr in_stage) : stage(in_stage) {}

  pxr::UsdStageRefPtr get_stage()
  {
    return stage;
  }

  pxr::UsdStageRefPtr stage;
};

/* Encapsulate arguments for material export. */
struct USDMaterialExportContext {
  USDMaterialExportContext() {}

  USDMaterialExportContext(pxr::UsdStageRefPtr in_stage) : stage(in_stage) {}

  pxr::UsdStageRefPtr get_stage()
  {
    return stage;
  }

  pxr::UsdStageRefPtr stage;
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
  python::class_<USDSceneExportContext>("USDSceneExportContext")
      .def("get_stage", &USDSceneExportContext::get_stage)
      .def("get_depsgraph",
           &USDSceneExportContext::get_depsgraph,
           python::return_value_policy<python::return_by_value>());

  python::class_<USDMaterialExportContext>("USDMaterialExportContext")
      .def("get_stage", &USDMaterialExportContext::get_stage);

  python::class_<USDSceneImportContext>("USDSceneImportContext")
      .def("get_stage", &USDSceneImportContext::get_stage);

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
              "An exception occurred invoking USD hook '%s'.  Please see the console for details",
              hook->name);
}

/* Abstract base class to facilitate calling a function with a given
 * signature defined by the registered USDHook classes.  Subclasses
 * override virtual methods to specify the hook function name and to
 * call the hook with the required arguments.
 */
class USDHookInvoker {
 public:
  /* Attempt to call the function, if defined by the registered hooks. */
  void call() const
  {
    if (hook_list().empty()) {
      return;
    }

    PyGILState_STATE gilstate = PyGILState_Ensure();

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

    PyGILState_Release(gilstate);
  }

 protected:
  /* Override to specify the name of the function to be called. */
  virtual const char *function_name() const = 0;
  /* Override to call the function of the given object with the
   * required arguments, e.g.,
   *
   * python::call_method<void>(hook_obj, function_name(), arg1, arg2); */
  virtual void call_hook(PyObject *hook_obj) const = 0;

  /* Reports list provided when constructing the subclass, used by #call() to store reports. */
  ReportList *reports_;
};

class OnExportInvoker : public USDHookInvoker {
 private:
  USDSceneExportContext hook_context_;

 public:
  OnExportInvoker(pxr::UsdStageRefPtr stage, Depsgraph *depsgraph, ReportList *reports)
      : hook_context_(stage, depsgraph)
  {
    reports_ = reports;
  }

 protected:
  const char *function_name() const override
  {
    return "on_export";
  }

  void call_hook(PyObject *hook_obj) const override
  {
    python::call_method<bool>(hook_obj, function_name(), hook_context_);
  }
};

class OnMaterialExportInvoker : public USDHookInvoker {
 private:
  USDMaterialExportContext hook_context_;
  pxr::UsdShadeMaterial usd_material_;
  PointerRNA material_ptr_;

 public:
  OnMaterialExportInvoker(pxr::UsdStageRefPtr stage,
                          Material *material,
                          pxr::UsdShadeMaterial &usd_material,
                          ReportList *reports)
      : hook_context_(stage), usd_material_(usd_material)
  {
    material_ptr_ = RNA_pointer_create(nullptr, &RNA_Material, material);
    reports_ = reports;
  }

 protected:
  const char *function_name() const override
  {
    return "on_material_export";
  }

  void call_hook(PyObject *hook_obj) const override
  {
    python::call_method<bool>(
        hook_obj, function_name(), hook_context_, material_ptr_, usd_material_);
  }
};

class OnImportInvoker : public USDHookInvoker {
 private:
  USDSceneImportContext hook_context_;

 public:
  OnImportInvoker(pxr::UsdStageRefPtr stage, ReportList *reports) : hook_context_(stage)
  {
    reports_ = reports;
  }

 protected:
  const char *function_name() const override
  {
    return "on_import";
  }

  void call_hook(PyObject *hook_obj) const override
  {
    python::call_method<bool>(hook_obj, function_name(), hook_context_);
  }
};

void call_export_hooks(pxr::UsdStageRefPtr stage, Depsgraph *depsgraph, ReportList *reports)
{
  if (hook_list().empty()) {
    return;
  }

  OnExportInvoker on_export(stage, depsgraph, reports);
  on_export.call();
}

void call_material_export_hooks(pxr::UsdStageRefPtr stage,
                                Material *material,
                                pxr::UsdShadeMaterial &usd_material,
                                ReportList *reports)
{
  if (hook_list().empty()) {
    return;
  }

  OnMaterialExportInvoker on_material_export(stage, material, usd_material, reports);
  on_material_export.call();
}

void call_import_hooks(pxr::UsdStageRefPtr stage, ReportList *reports)
{
  if (hook_list().empty()) {
    return;
  }

  OnImportInvoker on_import(stage, reports);
  on_import.call();
}

}  // namespace blender::io::usd
