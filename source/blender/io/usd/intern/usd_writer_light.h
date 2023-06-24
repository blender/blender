/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.h"

namespace blender::io::usd {

class USDLightWriter : public USDAbstractWriter {
 public:
  USDLightWriter(const USDExporterContext &ctx);

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual void do_write(HierarchyContext &context) override;
};

}  // namespace blender::io::usd
