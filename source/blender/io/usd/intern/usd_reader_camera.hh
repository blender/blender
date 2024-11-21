/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.hh"
#include "usd_reader_xform.hh"

#include <pxr/usd/usdGeom/camera.h>

namespace blender::io::usd {

class USDCameraReader : public USDXformReader {
 private:
  pxr::UsdGeomCamera cam_prim_;

 public:
  USDCameraReader(const pxr::UsdPrim &prim,
                  const USDImportParams &import_params,
                  const ImportSettings &settings)
      : USDXformReader(prim, import_params, settings), cam_prim_(prim)
  {
  }

  bool valid() const override
  {
    return bool(cam_prim_);
  }

  void create_object(Main *bmain, double motionSampleTime) override;
  void read_object_data(Main *bmain, double motionSampleTime) override;
};

}  // namespace blender::io::usd
