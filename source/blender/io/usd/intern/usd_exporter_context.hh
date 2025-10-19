/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/common.h>

#include <functional>

struct Depsgraph;
struct Main;
struct Image;
struct ImageUser;
struct Scene;

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
  std::string export_file_path;
  std::function<std::string(Main *, Scene *, Image *, ImageUser *)> export_image_fn;

  /** Optional callback for skel/shape-key path registration (used by USDPointInstancerWriter). */
  std::function<void(const Object *, const pxr::SdfPath &)> add_skel_mapping_fn;

  USDHierarchyIterator *hierarchy_iterator;
};

}  // namespace blender::io::usd
