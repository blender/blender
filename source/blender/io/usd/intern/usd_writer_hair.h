/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.h"

namespace blender::io::usd {

/* Writer for writing hair particle data as USD curves. */
class USDHairWriter : public USDAbstractWriter {
 public:
  USDHairWriter(const USDExporterContext &ctx);

 protected:
  virtual void do_write(HierarchyContext &context) override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;
};

}  // namespace blender::io::usd
