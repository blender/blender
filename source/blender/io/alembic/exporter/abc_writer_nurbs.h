/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_abstract.h"
#include "abc_writer_mesh.h"

#include <Alembic/AbcGeom/ONuPatch.h>

#include <vector>

namespace blender::io::alembic {

class ABCNurbsWriter : public ABCAbstractWriter {
 private:
  std::vector<Alembic::AbcGeom::ONuPatch> abc_nurbs_;
  std::vector<Alembic::AbcGeom::ONuPatchSchema> abc_nurbs_schemas_;

 public:
  explicit ABCNurbsWriter(const ABCWriterConstructorArgs &args);

  virtual void create_alembic_objects(const HierarchyContext *context) override;
  virtual Alembic::Abc::OObject get_alembic_object() const override;

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual void do_write(HierarchyContext &context) override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;
  Alembic::Abc::OCompoundProperty abc_prop_for_custom_props() override;
};

class ABCNurbsMeshWriter : public ABCGenericMeshWriter {
 public:
  explicit ABCNurbsMeshWriter(const ABCWriterConstructorArgs &args);

 protected:
  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
};

}  // namespace blender::io::alembic
