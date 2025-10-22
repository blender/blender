/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/usd/usd/common.h>
#include <pxr/usd/usdShade/material.h>

struct Depsgraph;
struct Material;
struct ReportList;

namespace blender::io::usd {

struct USDExportParams;
class USDHierarchyIterator;
struct USDImportParams;
class USDStageReader;

/** Ensure classes and type converters necessary for invoking import and export hooks
 * are registered. */
void register_hook_converters();

/** Call the 'on_export' chaser function defined in the registered #USDHook classes. */
void call_export_hooks(Depsgraph *depsgraph,
                       const USDHierarchyIterator *iter,
                       ReportList *reports);

/** Call the 'on_material_export' hook functions defined in the registered #USDHook classes. */
void call_material_export_hooks(pxr::UsdStageRefPtr stage,
                                Material *material,
                                const pxr::UsdShadeMaterial &usd_material,
                                const USDExportParams &export_params,
                                ReportList *reports);

/** Call the 'on_import' chaser function defined in the registered USDHook classes. */
void call_import_hooks(USDStageReader *archive, ReportList *reports);

/** Returns true if there is a registered #USDHook class that can convert the given material. */
bool have_material_import_hook(pxr::UsdStageRefPtr stage,
                               const pxr::UsdShadeMaterial &usd_material,
                               const USDImportParams &import_params,
                               ReportList *reports);

/** Call the 'on_material_import' hook functions defined in the registered #USDHook classes.
 * Returns true if any of the hooks were successful, false otherwise. */
bool call_material_import_hooks(pxr::UsdStageRefPtr stage,
                                Material *material,
                                const pxr::UsdShadeMaterial &usd_material,
                                const USDImportParams &import_params,
                                ReportList *reports);

}  // namespace blender::io::usd
