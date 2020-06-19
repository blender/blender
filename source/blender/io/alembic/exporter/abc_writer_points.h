/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_object.h"
#include "intern/abc_customdata.h"

struct ParticleSystem;

/* ************************************************************************** */

namespace blender {
namespace io {
namespace alembic {

class AbcPointsWriter : public AbcObjectWriter {
  Alembic::AbcGeom::OPointsSchema m_schema;
  Alembic::AbcGeom::OPointsSchema::Sample m_sample;
  ParticleSystem *m_psys;

 public:
  AbcPointsWriter(Object *ob,
                  AbcTransformWriter *parent,
                  uint32_t time_sampling,
                  ExportSettings &settings,
                  ParticleSystem *psys);

  void do_write();
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
