/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_abstract.h"

#include <Alembic/AbcGeom/OCamera.h>

namespace blender::io::alembic {

class ABCCameraWriter : public ABCAbstractWriter {
 private:
  Alembic::AbcGeom::OCamera abc_camera_;
  Alembic::AbcGeom::OCameraSchema abc_camera_schema_;

  Alembic::AbcGeom::OCompoundProperty abc_custom_data_container_;
  Alembic::AbcGeom::OFloatProperty abc_stereo_distance_;
  Alembic::AbcGeom::OFloatProperty abc_eye_separation_;

 public:
  explicit ABCCameraWriter(const ABCWriterConstructorArgs &args);

  virtual void create_alembic_objects(const HierarchyContext *context) override;
  virtual Alembic::Abc::OObject get_alembic_object() const override;

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual void do_write(HierarchyContext &context) override;
  Alembic::Abc::OCompoundProperty abc_prop_for_custom_props() override;
};

}  // namespace blender::io::alembic
