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

#include "abc_reader_points.h"
#include "abc_reader_mesh.h"
#include "abc_reader_transform.h"
#include "abc_util.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::N3fArraySamplePtr;
using Alembic::AbcGeom::P3fArraySamplePtr;

using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::IN3fArrayProperty;
using Alembic::AbcGeom::IPoints;
using Alembic::AbcGeom::IPointsSchema;
using Alembic::AbcGeom::ISampleSelector;

namespace blender {
namespace io {
namespace alembic {

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
    *err_str =
        "Object type mismatch, Alembic object path pointed to Points when importing, but not any "
        "more.";
    return false;
  }

  if (ob->type != OB_MESH) {
    *err_str = "Object type mismatch, Alembic object path points to Points.";
    return false;
  }

  return true;
}

void AbcPointsReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());
  Mesh *read_mesh = this->read_mesh(mesh, sample_sel, 0, NULL);

  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, m_object, &CD_MASK_MESH, true);
  }

  if (m_settings->validate_meshes) {
    BKE_mesh_validate(mesh, false, false);
  }

  m_object = BKE_object_add_only_object(bmain, OB_MESH, m_object_name.c_str());
  m_object->data = mesh;

  if (has_animations(m_schema, m_settings)) {
    addCacheModifier();
  }
}

void read_points_sample(const IPointsSchema &schema,
                        const ISampleSelector &selector,
                        CDStreamConfig &config)
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

  read_mverts(config.mvert, positions, vnormals);
}

struct Mesh *AbcPointsReader::read_mesh(struct Mesh *existing_mesh,
                                        const ISampleSelector &sample_sel,
                                        int /*read_flag*/,
                                        const char **err_str)
{
  IPointsSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    *err_str = "Error reading points sample; more detail on the console";
    printf("Alembic: error reading points sample for '%s/%s' at time %f: %s\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    return existing_mesh;
  }

  const P3fArraySamplePtr &positions = sample.getPositions();

  Mesh *new_mesh = NULL;

  if (existing_mesh->totvert != positions->size()) {
    new_mesh = BKE_mesh_new_nomain(positions->size(), 0, 0, 0, 0);
  }

  CDStreamConfig config = get_config(new_mesh ? new_mesh : existing_mesh);
  read_points_sample(m_schema, sample_sel, config);

  return new_mesh ? new_mesh : existing_mesh;
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
