/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.h"

#include <string>
#include <unordered_map>
#include <vector>

struct Object;
struct Bone;

namespace blender::io::usd {

class USDArmatureWriter : public USDAbstractWriter {
 public:
  USDArmatureWriter(const USDExporterContext &ctx);

 protected:
  virtual void do_write(HierarchyContext &context) override;

  virtual bool check_is_animated(const HierarchyContext &context) const override;

 private:
  std::unordered_map<const char *, const Bone *> deform_map_;
};

}  // namespace blender::io::usd
