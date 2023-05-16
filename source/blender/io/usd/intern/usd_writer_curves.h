/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */
#pragma once

#include <memory>

#include "DNA_curves_types.h"
#include "usd_writer_abstract.h"

#include <pxr/usd/usdGeom/basisCurves.h>

namespace blender::io::usd {

/* Writer for writing Curves data as USD curves. */
class USDCurvesWriter : public USDAbstractWriter {
 public:
  USDCurvesWriter(const USDExporterContext &ctx);
  ~USDCurvesWriter();

 protected:
  virtual void do_write(HierarchyContext &context) override;
  void assign_materials(const HierarchyContext &context, pxr::UsdGeomCurves usd_curve);

 private:
  int8_t first_frame_curve_type = -1;
  pxr::UsdGeomCurves DefineUsdGeomBasisCurves(pxr::VtValue curve_basis, bool cyclic, bool cubic);

  void set_writer_attributes(pxr::UsdGeomCurves &usd_curves,
                             const pxr::VtArray<pxr::GfVec3f> verts,
                             const pxr::VtIntArray control_point_counts,
                             const pxr::VtArray<float> widths,
                             const pxr::UsdTimeCode timecode,
                             const pxr::TfToken interpolation);

  void set_writer_attributes_for_nurbs(const pxr::UsdGeomCurves usd_curves,
                                       const pxr::VtArray<double> knots,
                                       const pxr::VtArray<int> orders,
                                       const pxr::UsdTimeCode timecode);
};

}  // namespace blender::io::usd
