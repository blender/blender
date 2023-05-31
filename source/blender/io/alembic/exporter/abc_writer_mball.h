/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
  virtual void free_export_mesh(Mesh *mesh) override;
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;
  virtual bool export_as_subdivision_surface(Object *ob_eval) const override;

 private:
  bool is_basis_ball(Scene *scene, Object *ob) const;
};

}  // namespace blender::io::alembic
