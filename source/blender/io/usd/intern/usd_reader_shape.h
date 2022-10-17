/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Tangent Animation. All rights reserved. */
#pragma once

#include "usd.h"
#include "usd_reader_geom.h"
#include "usd_reader_xform.h"
#include <pxr/usd/usdGeom/gprim.h>

struct Mesh;

namespace blender::io::usd {

class USDShapeReader : public USDGeomReader {
 private:
  template<typename Adapter>
  void read_values(double motionSampleTime,
                   pxr::VtVec3fArray &positions,
                   pxr::VtIntArray &face_indices,
                   pxr::VtIntArray &face_counts);

 public:
  USDShapeReader(const pxr::UsdPrim &prim,
                 const USDImportParams &import_params,
                 const ImportSettings &settings);

  void create_object(Main *bmain, double motionSampleTime) override;
  void read_object_data(Main *bmain, double motionSampleTime) override;
  struct Mesh *read_mesh(struct Mesh *existing_mesh,
                         double motionSampleTime,
                         int read_flag,
                         const char **err_str) override;
  bool is_time_varying();

  virtual bool topology_changed(const Mesh * /* existing_mesh */, double /* motionSampleTime */) override
  {
    return false;
  };
};

}  // namespace blender::io::usd
