/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_points.h"

#include <numeric>

#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_math_matrix_c.hh"
#include "BLI_math_vector_c.hh"

#include "BKE_particle.h"
#include "BKE_pointcloud.hh"

#include "DEG_depsgraph_query.hh"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"io.alembic"};

namespace io::alembic {

using Alembic::AbcGeom::kVertexScope;
using Alembic::AbcGeom::OPoints;
using Alembic::AbcGeom::OPointsSchema;

ABCPointsWriter::ABCPointsWriter(const ABCWriterConstructorArgs &args) : ABCAbstractWriter(args) {}

void ABCPointsWriter::create_alembic_objects(const HierarchyContext * /*context*/)
{
  CLOG_DEBUG(&LOG, "exporting OPoints %s", args_.abc_path.c_str());
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

  psys_sim_data_init(&sim);

  uint64_t index = 0;
  for (int p = 0; p < psys->totpart; p++) {
    float pos[3], vel[3];

    if (psys->particles[p].flag & (PARS_NO_DISP | PARS_UNEXIST)) {
      continue;
    }

    state.time = DEG_get_ctime(args_.depsgraph);
    if (psys_get_particle_state(&sim, p, &state, false) == 0) {
      continue;
    }

    /* location */
    mul_v3_m4v3(pos, context.object->world_to_object().ptr(), state.co);

    /* velocity */
    sub_v3_v3v3(vel, state.co, psys->particles[p].prev_state.co);

    /* Convert Z-up to Y-up. */
    points.emplace_back(pos[0], pos[2], -pos[1]);
    velocities.emplace_back(vel[0], vel[2], -vel[1]);
    widths.push_back(psys->particles[p].size);
    ids.push_back(index++);
  }

  psys_sim_data_free(&sim);

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

ABCPointCloudWriter::ABCPointCloudWriter(const ABCWriterConstructorArgs &args)
    : ABCAbstractWriter(args)
{
}

void ABCPointCloudWriter::create_alembic_objects(const HierarchyContext * /*context*/)
{
  CLOG_DEBUG(&LOG, "exporting OPoints %s", args_.abc_path.c_str());
  abc_points_ = OPoints(args_.abc_parent, args_.abc_name, timesample_index_);
  abc_points_schema_ = abc_points_.getSchema();
}

Alembic::Abc::OObject ABCPointCloudWriter::get_alembic_object() const
{
  return abc_points_;
}

Alembic::Abc::OCompoundProperty ABCPointCloudWriter::abc_prop_for_custom_props()
{
  return abc_schema_prop_for_custom_props(abc_points_schema_);
}

void ABCPointCloudWriter::do_write(HierarchyContext &context)
{
  const PointCloud *pointcloud = id_cast<const PointCloud *>(context.object->data);

  OPointsSchema::Sample sample;

  std::vector<Imath::V3f> points;
  get_positions(pointcloud->positions(), points);
  sample.setPositions(points);

  std::vector<uint64_t> ids;
  ids.resize(pointcloud->totpoint);
  const VArraySpan point_ids = *pointcloud->attributes().lookup<int>("id", bke::AttrDomain::Point);
  if (point_ids.is_empty()) {
    /* Fill ids from 0 to size - 1 if we do not have any as Alembic requires a sample to have ids.
     */
    std::iota(ids.begin(), ids.end(), 0);
  }
  else {
    for (const int i : point_ids.index_range()) {
      ids[i] = uint64_t(point_ids[i]);
    }
  }
  sample.setIds(ids);

  VArray<float> radii = pointcloud->radius();
  std::vector<float> widths;
  if (!radii.is_empty()) {
    widths.resize(radii.size());

    /* TODO(kevindietrich): if the radius is stored as a single value, export it as such on the
     * Uniform scope. */
    for (const int i : radii.index_range()) {
      widths[i] = radii[i] * 2.0f;
    }

    Alembic::Abc::FloatArraySample wsample_array(widths);
    Alembic::AbcGeom::OFloatGeomParam::Sample wsample(wsample_array, kVertexScope);
    sample.setWidths(wsample);
  }

  std::vector<Imath::V3f> velocities;
  if (get_velocities(pointcloud->attributes(), velocities)) {
    sample.setVelocities(velocities);
  }

  update_bounding_box(context.object);
  sample.setSelfBounds(bounding_box_);
  abc_points_schema_.set(sample);
}

}  // namespace io::alembic
}  // namespace blender
