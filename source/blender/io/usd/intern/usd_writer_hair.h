/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */
#pragma once

#include "usd_writer_abstract.h"

#include <pxr/usd/usdGeom/basisCurves.h>

namespace blender::io::usd {

/* Writer for writing hair particle data as USD curves. */
class USDHairWriter : public USDAbstractWriter {
 public:
  USDHairWriter(const USDExporterContext &ctx);

 protected:
  virtual void do_write(HierarchyContext &context) override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;
  void assign_material(const HierarchyContext &context, pxr::UsdGeomBasisCurves usd_curve);
};

}  // namespace blender::io::usd
