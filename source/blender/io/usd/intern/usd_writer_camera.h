/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */
#pragma once

#include "usd_writer_abstract.h"

namespace blender::io::usd {

/* Writer for writing camera data to UsdGeomCamera. */
class USDCameraWriter : public USDAbstractWriter {
 public:
  USDCameraWriter(const USDExporterContext &ctx);

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual void do_write(HierarchyContext &context) override;
};

}  // namespace blender::io::usd
