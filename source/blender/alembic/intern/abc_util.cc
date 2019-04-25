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

#include "abc_camera.h"
#include "abc_curves.h"
#include "abc_mesh.h"
#include "abc_nurbs.h"
#include "abc_points.h"
#include "abc_transform.h"

#include <Alembic/AbcMaterial/IMaterial.h>

#include <algorithm>

extern "C" {
#include "DNA_object_types.h"
#include "DNA_layer_types.h"

#include "BLI_math.h"

#include "PIL_time.h"
}

std::string get_id_name(const Object *const ob)
{
  if (!ob) {
    return "";
  }

  return get_id_name(&ob->id);
}

std::string get_id_name(const ID *const id)
{
  std::string name(id->name + 2);
  std::replace(name.begin(), name.end(), ' ', '_');
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), ':', '_');

  return name;
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

Imath::M44d convert_matrix(float mat[4][4])
{
  Imath::M44d m;

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      m[i][j] = mat[i][j];
    }
  }

  return m;
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

void create_swapped_rotation_matrix(float rot_x_mat[3][3],
                                    float rot_y_mat[3][3],
                                    float rot_z_mat[3][3],
                                    const float euler[3],
                                    AbcAxisSwapMode mode)
{
  const float rx = euler[0];
  float ry;
  float rz;

  /* Apply transformation */
  switch (mode) {
    case ABC_ZUP_FROM_YUP:
      ry = -euler[2];
      rz = euler[1];
      break;
    case ABC_YUP_FROM_ZUP:
      ry = euler[2];
      rz = -euler[1];
      break;
    default:
      ry = 0.0f;
      rz = 0.0f;
      BLI_assert(false);
      break;
  }

  unit_m3(rot_x_mat);
  unit_m3(rot_y_mat);
  unit_m3(rot_z_mat);

  rot_x_mat[1][1] = cos(rx);
  rot_x_mat[2][1] = -sin(rx);
  rot_x_mat[1][2] = sin(rx);
  rot_x_mat[2][2] = cos(rx);

  rot_y_mat[2][2] = cos(ry);
  rot_y_mat[0][2] = -sin(ry);
  rot_y_mat[2][0] = sin(ry);
  rot_y_mat[0][0] = cos(ry);

  rot_z_mat[0][0] = cos(rz);
  rot_z_mat[1][0] = -sin(rz);
  rot_z_mat[0][1] = sin(rz);
  rot_z_mat[1][1] = cos(rz);
}

/* Convert matrix from Z=up to Y=up or vice versa. Use yup_mat = zup_mat for in-place conversion. */
void copy_m44_axis_swap(float dst_mat[4][4], float src_mat[4][4], AbcAxisSwapMode mode)
{
  float dst_rot[3][3], src_rot[3][3], dst_scale_mat[4][4];
  float rot_x_mat[3][3], rot_y_mat[3][3], rot_z_mat[3][3];
  float src_trans[3], dst_scale[3], src_scale[3], euler[3];

  zero_v3(src_trans);
  zero_v3(dst_scale);
  zero_v3(src_scale);
  zero_v3(euler);
  unit_m3(src_rot);
  unit_m3(dst_rot);
  unit_m4(dst_scale_mat);

  /* TODO(Sybren): This code assumes there is no sheer component and no
   * homogeneous scaling component, which is not always true when writing
   * non-hierarchical (e.g. flat) objects (e.g. when parent has non-uniform
   * scale and the child rotates). This is currently not taken into account
   * when axis-swapping. */

  /* Extract translation, rotation, and scale form matrix. */
  mat4_to_loc_rot_size(src_trans, src_rot, src_scale, src_mat);

  /* Get euler angles from rotation matrix. */
  mat3_to_eulO(euler, ROT_MODE_XZY, src_rot);

  /* Create X, Y, Z rotation matrices from euler angles. */
  create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, mode);

  /* Concatenate rotation matrices. */
  mul_m3_m3m3(dst_rot, dst_rot, rot_z_mat);
  mul_m3_m3m3(dst_rot, dst_rot, rot_y_mat);
  mul_m3_m3m3(dst_rot, dst_rot, rot_x_mat);

  mat3_to_eulO(euler, ROT_MODE_XZY, dst_rot);

  /* Start construction of dst_mat from rotation matrix */
  unit_m4(dst_mat);
  copy_m4_m3(dst_mat, dst_rot);

  /* Apply translation */
  switch (mode) {
    case ABC_ZUP_FROM_YUP:
      copy_zup_from_yup(dst_mat[3], src_trans);
      break;
    case ABC_YUP_FROM_ZUP:
      copy_yup_from_zup(dst_mat[3], src_trans);
      break;
    default:
      BLI_assert(false);
  }

  /* Apply scale matrix. Swaps y and z, but does not
   * negate like translation does. */
  dst_scale[0] = src_scale[0];
  dst_scale[1] = src_scale[2];
  dst_scale[2] = src_scale[1];

  size_to_mat4(dst_scale_mat, dst_scale);
  mul_m4_m4m4(dst_mat, dst_mat, dst_scale_mat);
}

void convert_matrix(const Imath::M44d &xform, Object *ob, float r_mat[4][4])
{
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      r_mat[i][j] = static_cast<float>(xform[i][j]);
    }
  }

  if (ob->type == OB_CAMERA) {
    float cam_to_yup[4][4];
    axis_angle_to_mat4_single(cam_to_yup, 'X', M_PI_2);
    mul_m4_m4m4(r_mat, r_mat, cam_to_yup);
  }

  copy_m44_axis_swap(r_mat, r_mat, ABC_ZUP_FROM_YUP);
}

/* Recompute transform matrix of object in new coordinate system
 * (from Z-Up to Y-Up). */
void create_transform_matrix(Object *obj,
                             float r_yup_mat[4][4],
                             AbcMatrixMode mode,
                             Object *proxy_from)
{
  float zup_mat[4][4];

  /* get local or world matrix. */
  if (mode == ABC_MATRIX_LOCAL && obj->parent) {
    /* Note that this produces another matrix than the local matrix, due to
     * constraints and modifiers as well as the obj->parentinv matrix. */
    invert_m4_m4(obj->parent->imat, obj->parent->obmat);
    mul_m4_m4m4(zup_mat, obj->parent->imat, obj->obmat);
  }
  else {
    copy_m4_m4(zup_mat, obj->obmat);
  }

  if (proxy_from) {
    mul_m4_m4m4(zup_mat, proxy_from->obmat, zup_mat);
  }

  copy_m44_axis_swap(r_yup_mat, zup_mat, ABC_YUP_FROM_ZUP);
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
