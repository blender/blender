/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.hh"

#include <pxr/usd/usdGeom/points.h>

struct Main;
struct PointCloud;

namespace blender::bke {
struct AttributeMetaData;
}  // namespace blender::bke

namespace blender::io::usd {

/* Writer for USD points. */
class USDPointsWriter final : public USDAbstractWriter {
 public:
  USDPointsWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}
  ~USDPointsWriter() final = default;

 protected:
  virtual void do_write(HierarchyContext &context) override;

 private:
  void write_generic_data(const PointCloud *points,
                          const StringRef attribute_id,
                          const bke::AttributeMetaData &meta_data,
                          const pxr::UsdGeomPoints &usd_points,
                          pxr::UsdTimeCode timecode);

  void write_custom_data(const PointCloud *points,
                         const pxr::UsdGeomPoints &usd_points,
                         pxr::UsdTimeCode timecode);

  void write_velocities(const PointCloud *points,
                        const pxr::UsdGeomPoints &usd_points,
                        pxr::UsdTimeCode timecode);

  void set_extents(const pxr::UsdPrim &prim, pxr::UsdTimeCode timecode);
};

}  // namespace blender::io::usd
