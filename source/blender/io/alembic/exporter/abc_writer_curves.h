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

namespace blender::bke {
class CurvesGeometry;
}

namespace blender::io::alembic {

class ABCCurveWriter : public ABCAbstractWriter {
 private:
  Alembic::AbcGeom::OCurves abc_curve_;
  Alembic::AbcGeom::OCurvesSchema abc_curve_schema_;

  std::unique_ptr<AttributeParamMaps> attribute_maps_ = nullptr;

 public:
  explicit ABCCurveWriter(const ABCWriterConstructorArgs &args);

  void create_alembic_objects(const HierarchyContext *context) override;
  Alembic::Abc::OObject get_alembic_object() const override;
  Alembic::Abc::OCompoundProperty abc_prop_for_custom_props() override;

 protected:
  void do_write(HierarchyContext &context) override;

 private:
  void write_arb_geo_params(const bke::CurvesGeometry &curves,
                            const Object &object,
                            const size_t num_geom_samples);

  AttributeParamMaps &get_attribute_param_maps();
};

class ABCCurveMeshWriter : public ABCGenericMeshWriter {
 public:
  ABCCurveMeshWriter(const ABCWriterConstructorArgs &args);

 protected:
  Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
};

}  // namespace blender::io::alembic
