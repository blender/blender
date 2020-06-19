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

#include "abc_reader_object.h"
#include "abc_axis_conversion.h"
#include "abc_util.h"

#include "DNA_cachefile_types.h"
#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_space_types.h" /* for FILE_MAX */

#include "BKE_constraint.h"
#include "BKE_lib_id.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IXform;
using Alembic::AbcGeom::IXformSchema;

namespace blender {
namespace io {
namespace alembic {

AbcObjectReader::AbcObjectReader(const IObject &object, ImportSettings &settings)
    : m_name(""),
      m_object_name(""),
      m_data_name(""),
      m_object(NULL),
      m_iobject(object),
      m_settings(&settings),
      m_min_time(std::numeric_limits<chrono_t>::max()),
      m_max_time(std::numeric_limits<chrono_t>::min()),
      m_refcount(0),
      parent_reader(NULL)
{
  m_name = object.getFullName();
  std::vector<std::string> parts;
  split(m_name, '/', parts);

  if (parts.size() >= 2) {
    m_object_name = parts[parts.size() - 2];
    m_data_name = parts[parts.size() - 1];
  }
  else {
    m_object_name = m_data_name = parts[parts.size() - 1];
  }

  determine_inherits_xform();
}

/* Determine whether we can inherit our parent's XForm */
void AbcObjectReader::determine_inherits_xform()
{
  m_inherits_xform = false;

  IXform ixform = xform();
  if (!ixform) {
    return;
  }

  const IXformSchema &schema(ixform.getSchema());
  if (!schema.valid()) {
    std::cerr << "Alembic object " << ixform.getFullName() << " has an invalid schema."
              << std::endl;
    return;
  }

  m_inherits_xform = schema.getInheritsXforms();

  IObject ixform_parent = ixform.getParent();
  if (!ixform_parent.getParent()) {
    /* The archive top object certainly is not a transform itself, so handle
     * it as "no parent". */
    m_inherits_xform = false;
  }
  else {
    m_inherits_xform = ixform_parent && m_inherits_xform;
  }
}

AbcObjectReader::~AbcObjectReader()
{
}

const IObject &AbcObjectReader::iobject() const
{
  return m_iobject;
}

Object *AbcObjectReader::object() const
{
  return m_object;
}

void AbcObjectReader::object(Object *ob)
{
  m_object = ob;
}

static Imath::M44d blend_matrices(const Imath::M44d &m0, const Imath::M44d &m1, const float weight)
{
  float mat0[4][4], mat1[4][4], ret[4][4];

  /* Cannot use Imath::M44d::getValue() since this returns a pointer to
   * doubles and interp_m4_m4m4 expects pointers to floats. So need to convert
   * the matrices manually.
   */

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      mat0[i][j] = static_cast<float>(m0[i][j]);
    }
  }

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      mat1[i][j] = static_cast<float>(m1[i][j]);
    }
  }

  interp_m4_m4m4(ret, mat0, mat1, weight);

  Imath::M44d m;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      m[i][j] = ret[i][j];
    }
  }

  return m;
}

Imath::M44d get_matrix(const IXformSchema &schema, const float time)
{
  Alembic::AbcGeom::index_t i0, i1;
  Alembic::AbcGeom::XformSample s0, s1;

  const float weight = get_weight_and_index(
      time, schema.getTimeSampling(), schema.getNumSamples(), i0, i1);

  schema.get(s0, Alembic::AbcGeom::ISampleSelector(i0));

  if (i0 != i1) {
    schema.get(s1, Alembic::AbcGeom::ISampleSelector(i1));
    return blend_matrices(s0.getMatrix(), s1.getMatrix(), weight);
  }

  return s0.getMatrix();
}

struct Mesh *AbcObjectReader::read_mesh(struct Mesh *existing_mesh,
                                        const Alembic::Abc::ISampleSelector &UNUSED(sample_sel),
                                        int UNUSED(read_flag),
                                        const char **UNUSED(err_str))
{
  return existing_mesh;
}

bool AbcObjectReader::topology_changed(Mesh * /*existing_mesh*/,
                                       const Alembic::Abc::ISampleSelector & /*sample_sel*/)
{
  /* The default implementation of read_mesh() just returns the original mesh, so never changes the
   * topology. */
  return false;
}

void AbcObjectReader::setupObjectTransform(const float time)
{
  bool is_constant = false;
  float transform_from_alembic[4][4];

  /* If the parent is a camera, apply the inverse rotation to make up for the from-Maya rotation.
   * This assumes that the parent object also was imported from Alembic. */
  if (m_object->parent != nullptr && m_object->parent->type == OB_CAMERA) {
    axis_angle_to_mat4_single(m_object->parentinv, 'X', -M_PI_2);
  }

  this->read_matrix(transform_from_alembic, time, m_settings->scale, is_constant);

  /* Apply the matrix to the object. */
  BKE_object_apply_mat4(m_object, transform_from_alembic, true, false);
  BKE_object_to_mat4(m_object, m_object->obmat);

  if (!is_constant) {
    bConstraint *con = BKE_constraint_add_for_object(
        m_object, NULL, CONSTRAINT_TYPE_TRANSFORM_CACHE);
    bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);
    BLI_strncpy(data->object_path, m_iobject.getFullName().c_str(), FILE_MAX);

    data->cache_file = m_settings->cache_file;
    id_us_plus(&data->cache_file->id);
  }
}

Alembic::AbcGeom::IXform AbcObjectReader::xform()
{
  /* Check that we have an empty object (locator, bone head/tail...).  */
  if (IXform::matches(m_iobject.getMetaData())) {
    try {
      return IXform(m_iobject, Alembic::AbcGeom::kWrapExisting);
    }
    catch (Alembic::Util::Exception &ex) {
      printf("Alembic: error reading object transform for '%s': %s\n",
             m_iobject.getFullName().c_str(),
             ex.what());
      return IXform();
    }
  }

  /* Check that we have an object with actual data, in which case the
   * parent Alembic object should contain the transform. */
  IObject abc_parent = m_iobject.getParent();

  /* The archive's top object can be recognised by not having a parent. */
  if (abc_parent.getParent() && IXform::matches(abc_parent.getMetaData())) {
    try {
      return IXform(abc_parent, Alembic::AbcGeom::kWrapExisting);
    }
    catch (Alembic::Util::Exception &ex) {
      printf("Alembic: error reading object transform for '%s': %s\n",
             abc_parent.getFullName().c_str(),
             ex.what());
      return IXform();
    }
  }

  /* This can happen in certain cases. For example, MeshLab exports
   * point clouds without parent XForm. */
  return IXform();
}

void AbcObjectReader::read_matrix(float r_mat[4][4] /* local matrix */,
                                  const float time,
                                  const float scale,
                                  bool &is_constant)
{
  IXform ixform = xform();
  if (!ixform) {
    unit_m4(r_mat);
    is_constant = true;
    return;
  }

  const IXformSchema &schema(ixform.getSchema());
  if (!schema.valid()) {
    std::cerr << "Alembic object " << ixform.getFullName() << " has an invalid schema."
              << std::endl;
    return;
  }

  const Imath::M44d matrix = get_matrix(schema, time);
  convert_matrix_datatype(matrix, r_mat);
  copy_m44_axis_swap(r_mat, r_mat, ABC_ZUP_FROM_YUP);

  /* Convert from Maya to Blender camera orientation. Children of this camera
   * will have the opposite transform as their Parent Inverse matrix.
   * See AbcObjectReader::setupObjectTransform(). */
  if (m_object->type == OB_CAMERA) {
    float camera_rotation[4][4];
    axis_angle_to_mat4_single(camera_rotation, 'X', M_PI_2);
    mul_m4_m4m4(r_mat, r_mat, camera_rotation);
  }

  if (!m_inherits_xform) {
    /* Only apply scaling to root objects, parenting will propagate it. */
    float scale_mat[4][4];
    scale_m4_fl(scale_mat, scale);
    mul_m4_m4m4(r_mat, scale_mat, r_mat);
  }

  is_constant = schema.isConstant();
}

void AbcObjectReader::addCacheModifier()
{
  ModifierData *md = BKE_modifier_new(eModifierType_MeshSequenceCache);
  BLI_addtail(&m_object->modifiers, md);

  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  mcmd->cache_file = m_settings->cache_file;
  id_us_plus(&mcmd->cache_file->id);

  BLI_strncpy(mcmd->object_path, m_iobject.getFullName().c_str(), FILE_MAX);
}

chrono_t AbcObjectReader::minTime() const
{
  return m_min_time;
}

chrono_t AbcObjectReader::maxTime() const
{
  return m_max_time;
}

int AbcObjectReader::refcount() const
{
  return m_refcount;
}

void AbcObjectReader::incref()
{
  m_refcount++;
}

void AbcObjectReader::decref()
{
  m_refcount--;
  BLI_assert(m_refcount >= 0);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
