/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.hh"

#include <pxr/usd/usdGeom/points.h>

struct Main;
struct PointCloud;

namespace blender::bke {
class AttributeIter;
}  // namespace blender::bke

namespace blender::io::usd {

/* Writer for USD points. */
class USDPointsWriter final : public USDAbstractWriter {
 public:
  USDPointsWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}
  ~USDPointsWriter() final = default;

 protected:
  void do_write(HierarchyContext &context) override;

 private:
  void write_generic_data(const bke::AttributeIter &attr,
                          const pxr::UsdGeomPoints &usd_points,
                          pxr::UsdTimeCode timecode);

  void write_custom_data(const PointCloud *points,
                         const pxr::UsdGeomPoints &usd_points,
                         pxr::UsdTimeCode timecode);

  void write_velocities(const PointCloud *points,
                        const pxr::UsdGeomPoints &usd_points,
                        pxr::UsdTimeCode timecode);
};

}  // namespace blender::io::usd
