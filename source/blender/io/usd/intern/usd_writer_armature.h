/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.h"

<<<<<<< HEAD
#include <string>
#include <unordered_map>
#include <vector>

struct Object;
struct Bone;
=======
#include "BLI_map.hh"

struct Bone;
struct Object;
>>>>>>> main

namespace blender::io::usd {

class USDArmatureWriter : public USDAbstractWriter {
 public:
  USDArmatureWriter(const USDExporterContext &ctx);

 protected:
  virtual void do_write(HierarchyContext &context) override;

  virtual bool check_is_animated(const HierarchyContext &context) const override;

 private:
<<<<<<< HEAD
  std::unordered_map<const char *, const Bone *> deform_map_;
=======
  Map<StringRef, const Bone *> deform_map_;
>>>>>>> main
};

}  // namespace blender::io::usd
