/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_abstract.h"

#include <memory>

#include <Alembic/AbcGeom/OXform.h>

namespace blender::io::alembic {

class ABCTransformWriter : public ABCAbstractWriter {
 private:
  Alembic::AbcGeom::OXform abc_xform_;
  Alembic::AbcGeom::OXformSchema abc_xform_schema_;

 public:
  explicit ABCTransformWriter(const ABCWriterConstructorArgs &args);
  virtual void create_alembic_objects(const HierarchyContext *context) override;

 protected:
  virtual void do_write(HierarchyContext &context) override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;
  virtual Alembic::Abc::OObject get_alembic_object() const override;
  const IDProperty *get_id_properties(const HierarchyContext &context) const override;
  Alembic::Abc::OCompoundProperty abc_prop_for_custom_props() override;
};

}  // namespace blender::io::alembic
