/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Adapted from the Blender Alembic importer implementation,
 * Copyright (C) 2016 Kévin Dietrich.
 *
 * Modifications Copyright (C) 2021 Tangent Animation.
 * All rights reserved.
 */
#pragma once

#include "usd.h"
#include "usd_reader_geom.h"

#include "pxr/usd/usdGeom/basisCurves.h"

struct Curve;

namespace blender::io::usd {

class USDCurvesReader : public USDGeomReader {
 protected:
  pxr::UsdGeomBasisCurves curve_prim_;
  Curve *curve_;

 public:
  USDCurvesReader(const pxr::UsdPrim &prim,
                  const USDImportParams &import_params,
                  const ImportSettings &settings)
      : USDGeomReader(prim, import_params, settings), curve_prim_(prim), curve_(nullptr)
  {
  }

  bool valid() const override
  {
    return static_cast<bool>(curve_prim_);
  }

  void create_object(Main *bmain, double motionSampleTime) override;
  void read_object_data(Main *bmain, double motionSampleTime) override;

  void read_curve_sample(Curve *cu, double motionSampleTime);

  Mesh *read_mesh(struct Mesh *existing_mesh,
                  double motionSampleTime,
                  int read_flag,
                  const char **err_str) override;
};

}  // namespace blender::io::usd
