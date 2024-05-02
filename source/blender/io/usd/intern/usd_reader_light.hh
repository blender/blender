/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"
#include "usd_reader_xform.hh"

#include <pxr/usd/usdGeom/xformCache.h>

struct Main;

namespace blender::io::usd {

class USDLightReader : public USDXformReader {
 private:
  float usd_world_scale_;

 public:
  USDLightReader(const pxr::UsdPrim &prim,
                 const USDImportParams &import_params,
                 const ImportSettings &settings,
                 pxr::UsdGeomXformCache *xf_cache = nullptr);

  void create_object(Main *bmain, double motionSampleTime) override;

  void read_object_data(Main *bmain, double motionSampleTime) override;
};

}  // namespace blender::io::usd
