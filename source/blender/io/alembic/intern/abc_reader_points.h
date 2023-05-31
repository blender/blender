/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_customdata.h"
#include "abc_reader_object.h"

namespace blender::io::alembic {

class AbcPointsReader final : public AbcObjectReader {
  Alembic::AbcGeom::IPointsSchema m_schema;
  Alembic::AbcGeom::IPointsSchema::Sample m_sample;

 public:
  AbcPointsReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

  bool valid() const override;
  bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
                           const Object *const ob,
                           const char **err_str) const override;

  void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel) override;

  struct Mesh *read_mesh(struct Mesh *existing_mesh,
                         const Alembic::Abc::ISampleSelector &sample_sel,
                         int read_flag,
                         const char *velocity_name,
                         float velocity_scale,
                         const char **err_str) override;
};

void read_points_sample(const Alembic::AbcGeom::IPointsSchema &schema,
                        const Alembic::AbcGeom::ISampleSelector &selector,
                        CDStreamConfig &config);

}  // namespace blender::io::alembic
