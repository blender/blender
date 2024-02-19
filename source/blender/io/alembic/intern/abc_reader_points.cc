/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_points.h"
#include "abc_reader_mesh.h"
#include "abc_reader_transform.h"
#include "abc_util.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLT_translation.h"

#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"

using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::N3fArraySamplePtr;
using Alembic::AbcGeom::P3fArraySamplePtr;

using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::IN3fArrayProperty;
using Alembic::AbcGeom::IPoints;
using Alembic::AbcGeom::IPointsSchema;
using Alembic::AbcGeom::ISampleSelector;

namespace blender::io::alembic {

AbcPointsReader::AbcPointsReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  IPoints ipoints(m_iobject, kWrapExisting);
  m_schema = ipoints.getSchema();
  get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcPointsReader::valid() const
{
  return m_schema.valid();
}

bool AbcPointsReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **err_str) const
{
  if (!Alembic::AbcGeom::IPoints::matches(alembic_header)) {
    *err_str = RPT_(
        "Object type mismatch, Alembic object path pointed to Points when importing, but not any "
        "more");
    return false;
  }

  if (ob->type != OB_MESH) {
    *err_str = RPT_("Object type mismatch, Alembic object path points to Points");
    return false;
  }

  return true;
}

void AbcPointsReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());
  Mesh *read_mesh = this->read_mesh(mesh, sample_sel, 0, "", 0.0f, nullptr);

  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, m_object);
  }

  if (m_settings->validate_meshes) {
    BKE_mesh_validate(mesh, false, false);
  }

  m_object = BKE_object_add_only_object(bmain, OB_MESH, m_object_name.c_str());
  m_object->data = mesh;

  if (m_settings->always_add_cache_reader || has_animations(m_schema, m_settings)) {
    addCacheModifier();
  }
}

void read_points_sample(const IPointsSchema &schema,
                        const ISampleSelector &selector,
                        CDStreamConfig &config,
                        ImportSettings *settings)
{
  Alembic::AbcGeom::IPointsSchema::Sample sample = schema.getValue(selector);

  const P3fArraySamplePtr &positions = sample.getPositions();

  ICompoundProperty prop = schema.getArbGeomParams();
  N3fArraySamplePtr vnormals;

  if (has_property(prop, "N")) {
    const Alembic::Util::uint32_t itime = static_cast<Alembic::Util::uint32_t>(
        selector.getRequestedTime());
    const IN3fArrayProperty &normals_prop = IN3fArrayProperty(prop, "N", itime);

    if (normals_prop) {
      vnormals = normals_prop.getValue(selector);
    }
  }

  read_mverts(*config.mesh, positions, vnormals);

  if (!settings->velocity_name.empty() && settings->velocity_scale != 0.0f) {
    V3fArraySamplePtr velocities = get_velocity_prop(schema, selector, settings->velocity_name);
    if (velocities) {
      read_velocity(velocities, config, settings->velocity_scale);
    }
  }
}

Mesh *AbcPointsReader::read_mesh(Mesh *existing_mesh,
                                 const ISampleSelector &sample_sel,
                                 int /*read_flag*/,
                                 const char *velocity_name,
                                 const float velocity_scale,
                                 const char **err_str)
{
  IPointsSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    *err_str = RPT_("Error reading points sample; more detail on the console");
    printf("Alembic: error reading points sample for '%s/%s' at time %f: %s\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    return existing_mesh;
  }

  const P3fArraySamplePtr &positions = sample.getPositions();

  Mesh *new_mesh = nullptr;

  if (existing_mesh->verts_num != positions->size()) {
    new_mesh = BKE_mesh_new_nomain(positions->size(), 0, 0, 0);
  }

  ImportSettings settings;
  settings.velocity_name = velocity_name;
  settings.velocity_scale = velocity_scale;

  Mesh *mesh_to_export = new_mesh ? new_mesh : existing_mesh;
  CDStreamConfig config = get_config(mesh_to_export);
  read_points_sample(m_schema, sample_sel, config, &settings);

  return mesh_to_export;
}

}  // namespace blender::io::alembic
