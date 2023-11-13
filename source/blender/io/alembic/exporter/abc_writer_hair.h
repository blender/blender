/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_abstract.h"
#include <Alembic/AbcGeom/OCurves.h>
#include <vector>

namespace blender::io::alembic {

class ABCHairWriter : public ABCAbstractWriter {
 private:
  Alembic::AbcGeom::OCurves abc_curves_;
  Alembic::AbcGeom::OCurvesSchema abc_curves_schema_;

  bool uv_warning_shown_;

 public:
  explicit ABCHairWriter(const ABCWriterConstructorArgs &args);

  virtual void create_alembic_objects(const HierarchyContext *context) override;
  virtual Alembic::Abc::OObject get_alembic_object() const override;

 protected:
  virtual void do_write(HierarchyContext &context) override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;
  Alembic::Abc::OCompoundProperty abc_prop_for_custom_props() override;

 private:
  void write_hair_sample(const HierarchyContext &context,
                         struct Mesh *mesh,
                         std::vector<Imath::V3f> &verts,
                         std::vector<Imath::V3f> &norm_values,
                         std::vector<Imath::V2f> &uv_values,
                         std::vector<int32_t> &hvertices);

  void write_hair_child_sample(const HierarchyContext &context,
                               struct Mesh *mesh,
                               std::vector<Imath::V3f> &verts,
                               std::vector<Imath::V3f> &norm_values,
                               std::vector<Imath::V2f> &uv_values,
                               std::vector<int32_t> &hvertices);
};

}  // namespace blender::io::alembic
