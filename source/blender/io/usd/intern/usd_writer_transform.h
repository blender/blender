/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.h"

#include <pxr/usd/usdGeom/xform.h>

namespace blender::io::usd {

class USDTransformWriter : public USDAbstractWriter {
 private:
  pxr::UsdGeomXformOp xformOp_;

 public:
  USDTransformWriter(const USDExporterContext &ctx);

 protected:
  void do_write(HierarchyContext &context) override;
  bool check_is_animated(const HierarchyContext &context) const override;
};

}  // namespace blender::io::usd
