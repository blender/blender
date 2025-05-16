/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_mesh.hh"

struct Mesh;
struct Object;

namespace blender::io::usd {

class USDTextWriter : public USDGenericMeshWriter {
 public:
  USDTextWriter(const USDExporterContext &ctx);

 protected:
  Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
  void free_export_mesh(Mesh *mesh) override;
};

}  // namespace blender::io::usd
