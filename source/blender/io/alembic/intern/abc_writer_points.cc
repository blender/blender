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

/** \file
 * \ingroup balembic
 */

#include "abc_writer_points.h"
#include "abc_util.h"
#include "abc_writer_mesh.h"
#include "abc_writer_transform.h"

#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_lattice.h"
#include "BKE_particle.h"

#include "BLI_math.h"

#include "DEG_depsgraph_query.h"

using Alembic::AbcGeom::kVertexScope;
using Alembic::AbcGeom::OPoints;
using Alembic::AbcGeom::OPointsSchema;

/* ************************************************************************** */

AbcPointsWriter::AbcPointsWriter(Object *ob,
                                 AbcTransformWriter *parent,
                                 uint32_t time_sampling,
                                 ExportSettings &settings,
                                 ParticleSystem *psys)
    : AbcObjectWriter(ob, time_sampling, settings, parent)
{
  m_psys = psys;

  OPoints points(parent->alembicXform(), psys->name, m_time_sampling);
  m_schema = points.getSchema();
}

void AbcPointsWriter::do_write()
{
  if (!m_psys) {
    return;
  }

  std::vector<Imath::V3f> points;
  std::vector<Imath::V3f> velocities;
  std::vector<float> widths;
  std::vector<uint64_t> ids;

  ParticleKey state;

  ParticleSimulationData sim;
  sim.depsgraph = m_settings.depsgraph;
  sim.scene = m_settings.scene;
  sim.ob = m_object;
  sim.psys = m_psys;

  m_psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

  uint64_t index = 0;
  for (int p = 0; p < m_psys->totpart; p++) {
    float pos[3], vel[3];

    if (m_psys->particles[p].flag & (PARS_NO_DISP | PARS_UNEXIST)) {
      continue;
    }

    state.time = DEG_get_ctime(m_settings.depsgraph);

    if (psys_get_particle_state(&sim, p, &state, 0) == 0) {
      continue;
    }

    /* location */
    mul_v3_m4v3(pos, m_object->imat, state.co);

    /* velocity */
    sub_v3_v3v3(vel, state.co, m_psys->particles[p].prev_state.co);

    /* Convert Z-up to Y-up. */
    points.push_back(Imath::V3f(pos[0], pos[2], -pos[1]));
    velocities.push_back(Imath::V3f(vel[0], vel[2], -vel[1]));
    widths.push_back(m_psys->particles[p].size);
    ids.push_back(index++);
  }

  if (m_psys->lattice_deform_data) {
    BKE_lattice_deform_data_destroy(m_psys->lattice_deform_data);
    m_psys->lattice_deform_data = NULL;
  }

  Alembic::Abc::P3fArraySample psample(points);
  Alembic::Abc::UInt64ArraySample idsample(ids);
  Alembic::Abc::V3fArraySample vsample(velocities);
  Alembic::Abc::FloatArraySample wsample_array(widths);
  Alembic::AbcGeom::OFloatGeomParam::Sample wsample(wsample_array, kVertexScope);

  m_sample = OPointsSchema::Sample(psample, idsample, vsample, wsample);
  m_sample.setSelfBounds(bounds());

  m_schema.set(m_sample);
}
