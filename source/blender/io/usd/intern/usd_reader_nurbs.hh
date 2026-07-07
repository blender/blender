/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.hh"
#include "usd_reader_curve.hh"
#include "usd_reader_prim.hh"

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/nurbsCurves.h>

namespace blender {

struct Curves;

namespace bke {
class CurvesGeometry;
}  // namespace bke

namespace io::usd {

class USDNurbsReader : public USDCurvesReader {
 private:
  pxr::UsdGeomNurbsCurves curve_prim_;

 public:
  USDNurbsReader(const pxr::UsdPrim &prim,
                 const USDImportParams &import_params,
                 const ImportSettings &settings)
      : USDCurvesReader(prim, import_params, settings), curve_prim_(prim)
  {
  }

  bool valid() const override
  {
    return bool(curve_prim_);
  }

  void read_curve_sample(Curves *curves_id, pxr::UsdTimeCode time) override;
  bool is_animated() const override;
};

}  // namespace io::usd
}  // namespace blender
