/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.h"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

struct Depsgraph;
struct Main;

namespace blender::io::usd {

class USDHierarchyIterator;

struct USDExporterContext {
  Main *bmain;
  Depsgraph *depsgraph;
  const pxr::UsdStageRefPtr stage;
  const pxr::SdfPath usd_path;
  const USDHierarchyIterator *hierarchy_iterator;
  const USDExportParams &export_params;

  template<typename T> T usd_define_or_over(pxr::SdfPath path) const
  {
    return (export_params.export_as_overs) ? T(stage->OverridePrim(path)) : T::Define(stage, path);
  }
};

}  // namespace blender::io::usd
