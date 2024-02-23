/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <functional>

struct Depsgraph;
struct Main;

namespace blender::io::usd {

class USDHierarchyIterator;

struct USDExporterContext {
  Main *bmain;
  Depsgraph *depsgraph;
  const pxr::UsdStageRefPtr stage;
  const pxr::SdfPath usd_path;
  /**
   * Wrap a function which returns the current time code
   * for export.  This is necessary since the context
   * may be used for exporting an animation over a sequence
   * of frames.
   */
  std::function<pxr::UsdTimeCode()> get_time_code;
  const USDExportParams &export_params;

  template<typename T> T usd_define_or_over(pxr::SdfPath path) const
  {
    return (export_params.export_as_overs) ? T(stage->OverridePrim(path)) : T::Define(stage, path);
  }

  std::string export_file_path;
};

}  // namespace blender::io::usd
