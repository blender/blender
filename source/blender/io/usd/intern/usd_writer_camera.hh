/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.hh"

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
