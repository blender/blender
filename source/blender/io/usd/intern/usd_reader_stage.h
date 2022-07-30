/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Tangent Animation and. NVIDIA Corporation. All rights reserved. */
#pragma once

struct Main;

#include "usd.h"
#include "usd_reader_prim.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/imageable.h>

#include <vector>

struct ImportSettings;

namespace blender::io::usd {

typedef std::map<pxr::SdfPath, std::vector<USDPrimReader *>> ProtoReaderMap;

class USDStageReader {

 protected:
  pxr::UsdStageRefPtr stage_;
  USDImportParams params_;
  ImportSettings settings_;

  std::vector<USDPrimReader *> readers_;

 public:
  USDStageReader(pxr::UsdStageRefPtr stage,
                 const USDImportParams &params,
                 const ImportSettings &settings);

  ~USDStageReader();

  USDPrimReader *create_reader_if_allowed(const pxr::UsdPrim &prim);

  USDPrimReader *create_reader(const pxr::UsdPrim &prim);

  void collect_readers(struct Main *bmain);

  bool valid() const;

  pxr::UsdStageRefPtr stage()
  {
    return stage_;
  }
  const USDImportParams &params() const
  {
    return params_;
  }

  const ImportSettings &settings() const
  {
    return settings_;
  }

  void clear_readers();

  const std::vector<USDPrimReader *> &readers() const
  {
    return readers_;
  };

  void sort_readers();

 private:
  USDPrimReader *collect_readers(Main *bmain, const pxr::UsdPrim &prim);

  /**
   * Returns true if the given prim should be included in the
   * traversal based on the import options and the prim's visibility
   * attribute.  Note that the prim will be trivially included
   * if it has no visibility attribute or if the visibility
   * is inherited.
   */
  bool include_by_visibility(const pxr::UsdGeomImageable &imageable) const;

  /**
   * Returns true if the given prim should be included in the
   * traversal based on the import options and the prim's purpose
   * attribute. E.g., return false (to exclude the prim) if the prim
   * represents guide geometry and the 'Import Guide' option is
   * toggled off.
   */
  bool include_by_purpose(const pxr::UsdGeomImageable &imageable) const;
};

};  // namespace blender::io::usd
