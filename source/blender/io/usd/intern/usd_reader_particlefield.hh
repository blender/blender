/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"
#include "usd_reader_geom.hh"

#include <pxr/usd/usdVol/particleField3DGaussianSplat.h>

namespace blender {
struct Main;
}

namespace blender::io::usd {

class USDParticleFieldReader : public USDGeomReader {
 private:
  pxr::UsdVolParticleField3DGaussianSplat gsplat_prim_;

 public:
  USDParticleFieldReader(const pxr::UsdPrim &prim,
                         const USDImportParams &import_params,
                         const ImportSettings &settings)
      : USDGeomReader(prim, import_params, settings), gsplat_prim_(prim)
  {
  }

  bool valid() const override
  {
    return bool(gsplat_prim_);
  }

  void create_object(Main *bmain) override;
  void read_object_data(Main *bmain, pxr::UsdTimeCode time) override;

  void read_geometry(bke::GeometrySet &geometry_set,
                     USDMeshReadParams params,
                     const char **r_err_str) override;

 private:
  bool is_animated() const;
};

}  // namespace blender::io::usd
