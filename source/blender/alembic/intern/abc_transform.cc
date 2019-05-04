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

#include "abc_transform.h"

#include <OpenEXR/ImathBoxAlgo.h>

#include "abc_util.h"

extern "C" {
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_object.h"

#include "DEG_depsgraph_query.h"
}

using Alembic::Abc::ISampleSelector;
using Alembic::AbcGeom::OObject;
using Alembic::AbcGeom::OXform;

/* ************************************************************************** */

static bool has_parent_camera(Object *ob)
{
  if (!ob->parent) {
    return false;
  }

  Object *parent = ob->parent;

  if (parent->type == OB_CAMERA) {
    return true;
  }

  return has_parent_camera(parent);
}

/* ************************************************************************** */

AbcTransformWriter::AbcTransformWriter(Object *ob,
                                       const OObject &abc_parent,
                                       AbcTransformWriter *parent,
                                       unsigned int time_sampling,
                                       ExportSettings &settings)
    : AbcObjectWriter(ob, time_sampling, settings, parent), m_proxy_from(NULL)
{
  m_is_animated = hasAnimation(m_object);

  if (!m_is_animated) {
    time_sampling = 0;
  }

  m_xform = OXform(abc_parent, get_id_name(m_object), time_sampling);
  m_schema = m_xform.getSchema();

  /* Blender objects can't have a parent without inheriting the transform. */
  m_inherits_xform = parent != NULL;
}

void AbcTransformWriter::do_write()
{
  Object *ob_eval = DEG_get_evaluated_object(m_settings.depsgraph, m_object);

  if (m_first_frame) {
    m_visibility = Alembic::AbcGeom::CreateVisibilityProperty(
        m_xform, m_xform.getSchema().getTimeSampling());
  }

  m_visibility.set(!(ob_eval->restrictflag & OB_RESTRICT_INSTANCE));

  if (!m_first_frame && !m_is_animated) {
    return;
  }

  float yup_mat[4][4];
  create_transform_matrix(
      ob_eval, yup_mat, m_inherits_xform ? ABC_MATRIX_LOCAL : ABC_MATRIX_WORLD, m_proxy_from);

  /* Only apply rotation to root camera, parenting will propagate it. */
  if (ob_eval->type == OB_CAMERA && (!m_inherits_xform || !has_parent_camera(ob_eval))) {
    float rot_mat[4][4];
    axis_angle_to_mat4_single(rot_mat, 'X', -M_PI_2);
    mul_m4_m4m4(yup_mat, yup_mat, rot_mat);
  }

  if (!ob_eval->parent || !m_inherits_xform) {
    /* Only apply scaling to root objects, parenting will propagate it. */
    float scale_mat[4][4];
    scale_m4_fl(scale_mat, m_settings.global_scale);
    scale_mat[3][3] = m_settings.global_scale; /* also scale translation */
    mul_m4_m4m4(yup_mat, yup_mat, scale_mat);
    yup_mat[3][3] /= m_settings.global_scale; /* normalise the homogeneous component */
  }

  m_matrix = convert_matrix(yup_mat);
  m_sample.setMatrix(m_matrix);
  m_sample.setInheritsXforms(m_inherits_xform);
  m_schema.set(m_sample);
}

Imath::Box3d AbcTransformWriter::bounds()
{
  Imath::Box3d bounds;

  for (int i = 0; i < m_children.size(); ++i) {
    Imath::Box3d box(m_children[i]->bounds());
    bounds.extendBy(box);
  }

  return Imath::transform(bounds, m_matrix);
}

bool AbcTransformWriter::hasAnimation(Object * /*ob*/) const
{
  /* TODO(kevin): implement this. */
  return true;
}

/* ************************************************************************** */

AbcEmptyReader::AbcEmptyReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  /* Empties have no data. It makes the import of Alembic files easier to
   * understand when we name the empty after its name in Alembic. */
  m_object_name = object.getName();

  Alembic::AbcGeom::IXform xform(object, Alembic::AbcGeom::kWrapExisting);
  m_schema = xform.getSchema();

  get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcEmptyReader::valid() const
{
  return m_schema.valid();
}

bool AbcEmptyReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **err_str) const
{
  if (!Alembic::AbcGeom::IXform::matches(alembic_header)) {
    *err_str =
        "Object type mismatch, Alembic object path pointed to XForm when importing, but not any "
        "more.";
    return false;
  }

  if (ob->type != OB_EMPTY) {
    *err_str = "Object type mismatch, Alembic object path points to XForm.";
    return false;
  }

  return true;
}

void AbcEmptyReader::readObjectData(Main *bmain, const ISampleSelector &UNUSED(sample_sel))
{
  m_object = BKE_object_add_only_object(bmain, OB_EMPTY, m_object_name.c_str());
  m_object->data = NULL;
}
