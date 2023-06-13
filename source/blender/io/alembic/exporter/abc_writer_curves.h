/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_abstract.h"
#include "abc_writer_mesh.h"

#include <Alembic/AbcGeom/OCurves.h>

namespace blender::io::alembic {

extern const std::string ABC_CURVE_RESOLUTION_U_PROPNAME;

class ABCCurveWriter : public ABCAbstractWriter {
 private:
  Alembic::AbcGeom::OCurves abc_curve_;
  Alembic::AbcGeom::OCurvesSchema abc_curve_schema_;

 public:
  explicit ABCCurveWriter(const ABCWriterConstructorArgs &args);

  virtual void create_alembic_objects(const HierarchyContext *context) override;
  virtual Alembic::Abc::OObject get_alembic_object() const override;
  Alembic::Abc::OCompoundProperty abc_prop_for_custom_props() override;

 protected:
  virtual void do_write(HierarchyContext &context) override;
};

class ABCCurveMeshWriter : public ABCGenericMeshWriter {
 public:
  ABCCurveMeshWriter(const ABCWriterConstructorArgs &args);

 protected:
  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
};

}  // namespace blender::io::alembic
