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

/** Ensure classes and type converters necessary for invoking import and export hooks
 * are registered. */
void register_hook_converters();

/** Call the 'on_export' chaser function defined in the registered USDHook classes. */
void call_export_hooks(pxr::UsdStageRefPtr stage, Depsgraph *depsgraph, ReportList *reports);

/** Call the 'on_material_export' hook functions defined in the registered #USDHook classes. */
void call_material_export_hooks(pxr::UsdStageRefPtr stage,
                                Material *material,
                                pxr::UsdShadeMaterial &usd_material,
                                ReportList *reports);

/** Call the 'on_import' chaser function defined in the registered USDHook classes. */
void call_import_hooks(pxr::UsdStageRefPtr stage, ReportList *reports);

}  // namespace blender::io::usd
