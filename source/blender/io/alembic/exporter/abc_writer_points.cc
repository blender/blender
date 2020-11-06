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

#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_lattice.h"
#include "BKE_particle.h"

#include "BLI_math.h"

#include "DEG_depsgraph_query.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

namespace blender::io::alembic {

using Alembic::AbcGeom::kVertexScope;
using Alembic::AbcGeom::OPoints;
using Alembic::AbcGeom::OPointsSchema;

ABCPointsWriter::ABCPointsWriter(const ABCWriterConstructorArgs &args) : ABCAbstractWriter(args)
{
}

void ABCPointsWriter::create_alembic_objects(const HierarchyContext * /*context*/)
{
  CLOG_INFO(&LOG, 2, "exporting OPoints %s", args_.abc_path.c_str());
  abc_points_ = OPoints(args_.abc_parent, args_.abc_name, timesample_index_);
  abc_points_schema_ = abc_points_.getSchema();
}

Alembic::Abc::OObject ABCPointsWriter::get_alembic_object() const
{
  return abc_points_;
}

Alembic::Abc::OCompoundProperty ABCPointsWriter::abc_prop_for_custom_props()
{
  return abc_schema_prop_for_custom_props(abc_points_schema_);
}

bool ABCPointsWriter::is_supported(const HierarchyContext *context) const
{
  return ELEM(context->particle_system->part->type,
              PART_EMITTER,
              PART_FLUID_FLIP,
              PART_FLUID_SPRAY,
              PART_FLUID_BUBBLE,
              PART_FLUID_FOAM,
              PART_FLUID_TRACER,
              PART_FLUID_SPRAYFOAM,
              PART_FLUID_SPRAYBUBBLE,
              PART_FLUID_FOAMBUBBLE,
              PART_FLUID_SPRAYFOAMBUBBLE);
}

bool ABCPointsWriter::check_is_animated(const HierarchyContext & /*context*/) const
{
  /* We assume that particles are always animated. */
  return true;
}

void ABCPointsWriter::do_write(HierarchyContext &context)
{
  BLI_assert(context.particle_system != nullptr);

  std::vector<Imath::V3f> points;
  std::vector<Imath::V3f> velocities;
  std::vector<float> widths;
  std::vector<uint64_t> ids;

  ParticleSystem *psys = context.particle_system;
  ParticleKey state;
  ParticleSimulationData sim;
  sim.depsgraph = args_.depsgraph;
  sim.scene = DEG_get_evaluated_scene(args_.depsgraph);
  sim.ob = context.object;
  sim.psys = psys;

  psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

  uint64_t index = 0;
  for (int p = 0; p < psys->totpart; p++) {
    float pos[3], vel[3];

    if (psys->particles[p].flag & (PARS_NO_DISP | PARS_UNEXIST)) {
      continue;
    }

    state.time = DEG_get_ctime(args_.depsgraph);
    if (psys_get_particle_state(&sim, p, &state, 0) == 0) {
      continue;
    }

    /* location */
    mul_v3_m4v3(pos, context.object->imat, state.co);

    /* velocity */
    sub_v3_v3v3(vel, state.co, psys->particles[p].prev_state.co);

    /* Convert Z-up to Y-up. */
    points.emplace_back(pos[0], pos[2], -pos[1]);
    velocities.emplace_back(vel[0], vel[2], -vel[1]);
    widths.push_back(psys->particles[p].size);
    ids.push_back(index++);
  }

  if (psys->lattice_deform_data) {
    BKE_lattice_deform_data_destroy(psys->lattice_deform_data);
    psys->lattice_deform_data = nullptr;
  }

  Alembic::Abc::P3fArraySample psample(points);
  Alembic::Abc::UInt64ArraySample idsample(ids);
  Alembic::Abc::V3fArraySample vsample(velocities);
  Alembic::Abc::FloatArraySample wsample_array(widths);
  Alembic::AbcGeom::OFloatGeomParam::Sample wsample(wsample_array, kVertexScope);

  OPointsSchema::Sample sample(psample, idsample, vsample, wsample);
  update_bounding_box(context.object);
  sample.setSelfBounds(bounding_box_);
  abc_points_schema_.set(sample);
}

}  // namespace blender::io::alembic
