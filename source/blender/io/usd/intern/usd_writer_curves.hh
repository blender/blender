/* SPDX-FileCopyrightText: 2022 Blender Authors. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.hh"

#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/curves.h>
#include <pxr/usd/usdGeom/nurbsCurves.h>

namespace blender::bke {
class AttributeIDRef;
struct AttributeMetaData;
class CurvesGeometry;
}  // namespace blender::bke

namespace blender::io::usd {

/* Writer for writing Curves data as USD curves. */
class USDCurvesWriter final : public USDAbstractWriter {
 public:
  USDCurvesWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}
  ~USDCurvesWriter() final = default;

 protected:
  virtual void do_write(HierarchyContext &context) override;
  void assign_materials(const HierarchyContext &context, const pxr::UsdGeomCurves &usd_curves);

 private:
  int8_t first_frame_curve_type = -1;
  pxr::UsdGeomBasisCurves DefineUsdGeomBasisCurves(pxr::VtValue curve_basis,
                                                   bool cyclic,
                                                   bool cubic) const;

  void set_writer_attributes(pxr::UsdGeomCurves &usd_curves,
                             const pxr::VtArray<pxr::GfVec3f> &verts,
                             const pxr::VtIntArray &control_point_counts,
                             const pxr::VtArray<float> &widths,
                             const pxr::UsdTimeCode timecode,
                             const pxr::TfToken interpolation);

  void set_writer_attributes_for_nurbs(const pxr::UsdGeomNurbsCurves &usd_nurbs_curves,
                                       const pxr::VtArray<double> &knots,
                                       const pxr::VtArray<int> &orders,
                                       const pxr::UsdTimeCode timecode);

  void write_generic_data(const bke::CurvesGeometry &curves,
                          const bke::AttributeIDRef &attribute_id,
                          const bke::AttributeMetaData &meta_data,
                          const pxr::UsdGeomCurves &usd_curves);

  void write_uv_data(const bke::CurvesGeometry &curves,
                     const bke::AttributeIDRef &attribute_id,
                     const pxr::UsdGeomCurves &usd_curves);

  void write_custom_data(const blender::bke::CurvesGeometry &curves,
                         pxr::UsdGeomCurves &usd_curves);
};

}  // namespace blender::io::usd
