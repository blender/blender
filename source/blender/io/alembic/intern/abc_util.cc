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
 */

/** \file
 * \ingroup balembic
 */

#include "abc_util.h"

#include "abc_axis_conversion.h"
#include "abc_reader_camera.h"
#include "abc_reader_curves.h"
#include "abc_reader_mesh.h"
#include "abc_reader_nurbs.h"
#include "abc_reader_points.h"
#include "abc_reader_transform.h"

#include <Alembic/AbcMaterial/IMaterial.h>

#include <algorithm>

#include "DNA_object_types.h"

#include "BLI_math_geom.h"

#include "PIL_time.h"

std::string get_id_name(const Object *const ob)
{
  if (!ob) {
    return "";
  }

  return get_id_name(&ob->id);
}

std::string get_id_name(const ID *const id)
{
  return get_valid_abc_name(id->name + 2);
}

std::string get_valid_abc_name(const char *name)
{
  std::string name_string(name);
  std::replace(name_string.begin(), name_string.end(), ' ', '_');
  std::replace(name_string.begin(), name_string.end(), '.', '_');
  std::replace(name_string.begin(), name_string.end(), ':', '_');
  return name_string;
}

/**
 * \brief get_object_dag_path_name returns the name under which the object
 *  will be exported in the Alembic file. It is of the form
 *  "[../grandparent/]parent/object" if dupli_parent is NULL, or
 *  "dupli_parent/[../grandparent/]parent/object" otherwise.
 * \param ob:
 * \param dupli_parent:
 * \return
 */
std::string get_object_dag_path_name(const Object *const ob, Object *dupli_parent)
{
  std::string name = get_id_name(ob);

  Object *p = ob->parent;

  while (p) {
    name = get_id_name(p) + "/" + name;
    p = p->parent;
  }

  if (dupli_parent && (ob != dupli_parent)) {
    name = get_id_name(dupli_parent) + "/" + name;
  }

  return name;
}

Imath::M44d convert_matrix_datatype(float mat[4][4])
{
  Imath::M44d m;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      m[i][j] = mat[i][j];
    }
  }

  return m;
}

void convert_matrix_datatype(const Imath::M44d &xform, float r_mat[4][4])
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      r_mat[i][j] = static_cast<float>(xform[i][j]);
    }
  }
}

void split(const std::string &s, const char delim, std::vector<std::string> &tokens)
{
  tokens.clear();

  std::stringstream ss(s);
  std::string item;

  while (std::getline(ss, item, delim)) {
    if (!item.empty()) {
      tokens.push_back(item);
    }
  }
}

bool has_property(const Alembic::Abc::ICompoundProperty &prop, const std::string &name)
{
  if (!prop.valid()) {
    return false;
  }

  return prop.getPropertyHeader(name) != NULL;
}

typedef std::pair<Alembic::AbcCoreAbstract::index_t, float> index_time_pair_t;

float get_weight_and_index(float time,
                           const Alembic::AbcCoreAbstract::TimeSamplingPtr &time_sampling,
                           int samples_number,
                           Alembic::AbcGeom::index_t &i0,
                           Alembic::AbcGeom::index_t &i1)
{
  samples_number = std::max(samples_number, 1);

  index_time_pair_t t0 = time_sampling->getFloorIndex(time, samples_number);
  i0 = i1 = t0.first;

  if (samples_number == 1 || (fabs(time - t0.second) < 0.0001f)) {
    return 0.0f;
  }

  index_time_pair_t t1 = time_sampling->getCeilIndex(time, samples_number);
  i1 = t1.first;

  if (i0 == i1) {
    return 0.0f;
  }

  const float bias = (time - t0.second) / (t1.second - t0.second);

  if (fabs(1.0f - bias) < 0.0001f) {
    i0 = i1;
    return 0.0f;
  }

  return bias;
}

//#define USE_NURBS

AbcObjectReader *create_reader(const Alembic::AbcGeom::IObject &object, ImportSettings &settings)
{
  AbcObjectReader *reader = NULL;

  const Alembic::AbcGeom::MetaData &md = object.getMetaData();

  if (Alembic::AbcGeom::IXform::matches(md)) {
    reader = new AbcEmptyReader(object, settings);
  }
  else if (Alembic::AbcGeom::IPolyMesh::matches(md)) {
    reader = new AbcMeshReader(object, settings);
  }
  else if (Alembic::AbcGeom::ISubD::matches(md)) {
    reader = new AbcSubDReader(object, settings);
  }
  else if (Alembic::AbcGeom::INuPatch::matches(md)) {
#ifdef USE_NURBS
    /* TODO(kevin): importing cyclic NURBS from other software crashes
     * at the moment. This is due to the fact that NURBS in other
     * software have duplicated points which causes buffer overflows in
     * Blender. Need to figure out exactly how these points are
     * duplicated, in all cases (cyclic U, cyclic V, and cyclic UV).
     * Until this is fixed, disabling NURBS reading. */
    reader = new AbcNurbsReader(child, settings);
#endif
  }
  else if (Alembic::AbcGeom::ICamera::matches(md)) {
    reader = new AbcCameraReader(object, settings);
  }
  else if (Alembic::AbcGeom::IPoints::matches(md)) {
    reader = new AbcPointsReader(object, settings);
  }
  else if (Alembic::AbcMaterial::IMaterial::matches(md)) {
    /* Pass for now. */
  }
  else if (Alembic::AbcGeom::ILight::matches(md)) {
    /* Pass for now. */
  }
  else if (Alembic::AbcGeom::IFaceSet::matches(md)) {
    /* Pass, those are handled in the mesh reader. */
  }
  else if (Alembic::AbcGeom::ICurves::matches(md)) {
    reader = new AbcCurveReader(object, settings);
  }
  else {
    std::cerr << "Alembic: unknown how to handle objects of schema '" << md.get("schemaObjTitle")
              << "', skipping object '" << object.getFullName() << "'" << std::endl;
  }

  return reader;
}

/* ********************** */

ScopeTimer::ScopeTimer(const char *message)
    : m_message(message), m_start(PIL_check_seconds_timer())
{
}

ScopeTimer::~ScopeTimer()
{
  fprintf(stderr, "%s: %fs\n", m_message, PIL_check_seconds_timer() - m_start);
}

/* ********************** */

std::string SimpleLogger::str() const
{
  return m_stream.str();
}

void SimpleLogger::clear()
{
  m_stream.clear();
  m_stream.str("");
}

std::ostringstream &SimpleLogger::stream()
{
  return m_stream;
}

std::ostream &operator<<(std::ostream &os, const SimpleLogger &logger)
{
  os << logger.str();
  return os;
}
