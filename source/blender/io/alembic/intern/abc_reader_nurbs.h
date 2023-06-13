/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_reader_object.h"

namespace blender::io::alembic {

class AbcNurbsReader final : public AbcObjectReader {
  std::vector<std::pair<Alembic::AbcGeom::INuPatchSchema, Alembic::Abc::IObject>> m_schemas;

 public:
  AbcNurbsReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

  bool valid() const override;

  bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
                           const Object *const ob,
                           const char **err_str) const override;

  void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel) override;

 private:
  void getNurbsPatches(const Alembic::Abc::IObject &obj);
};

}  // namespace blender::io::alembic
