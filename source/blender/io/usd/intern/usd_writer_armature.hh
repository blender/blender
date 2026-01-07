/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.hh"

#include "BLI_map.hh"

namespace blender {

struct Bone;

namespace io::usd {

class USDArmatureWriter : public USDAbstractWriter {
 public:
  USDArmatureWriter(const USDExporterContext &ctx);

 protected:
  void do_write(HierarchyContext &context) override;

  bool check_is_animated(const HierarchyContext &context) const override;

 private:
  Map<StringRef, const Bone *> deform_map_;
};

}  // namespace io::usd
}  // namespace blender
