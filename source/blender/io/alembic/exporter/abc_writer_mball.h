/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_mesh.h"

namespace blender::io::alembic {

class ABCMetaballWriter : public ABCGenericMeshWriter {
 public:
  explicit ABCMetaballWriter(const ABCWriterConstructorArgs &args);

 protected:
  Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
  void free_export_mesh(Mesh *mesh) override;
  bool is_supported(const HierarchyContext *context) const override;
  bool check_is_animated(const HierarchyContext &context) const override;
  bool export_as_subdivision_surface(Object *ob_eval) const override;

 private:
  bool is_basis_ball(Scene *scene, Object *ob) const;
};

}  // namespace blender::io::alembic
