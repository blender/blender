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

#include "abc_writer_transform.h"
#include "abc_axis_conversion.h"

#include <OpenEXR/ImathBoxAlgo.h>

#include "DNA_object_types.h"

#include "BLI_math.h"

#include "DEG_depsgraph_query.h"

using Alembic::AbcGeom::OObject;
using Alembic::AbcGeom::OXform;

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

  m_visibility.set(!(ob_eval->restrictflag & OB_RESTRICT_VIEWPORT));

  if (!m_first_frame && !m_is_animated) {
    return;
  }

  float yup_mat[4][4];
  create_transform_matrix(
      ob_eval, yup_mat, m_inherits_xform ? ABC_MATRIX_LOCAL : ABC_MATRIX_WORLD, m_proxy_from);

  /* If the parent is a camera, undo its to-Maya rotation (see below). */
  bool is_root_object = !m_inherits_xform || ob_eval->parent == nullptr;
  if (!is_root_object && ob_eval->parent->type == OB_CAMERA) {
    float rot_mat[4][4];
    axis_angle_to_mat4_single(rot_mat, 'X', M_PI_2);
    mul_m4_m4m4(yup_mat, rot_mat, yup_mat);
  }

  /* If the object is a camera, apply an extra rotation to Maya camera orientation. */
  if (ob_eval->type == OB_CAMERA) {
    float rot_mat[4][4];
    axis_angle_to_mat4_single(rot_mat, 'X', -M_PI_2);
    mul_m4_m4m4(yup_mat, yup_mat, rot_mat);
  }

  if (is_root_object) {
    /* Only apply scaling to root objects, parenting will propagate it. */
    float scale_mat[4][4];
    scale_m4_fl(scale_mat, m_settings.global_scale);
    scale_mat[3][3] = m_settings.global_scale; /* also scale translation */
    mul_m4_m4m4(yup_mat, yup_mat, scale_mat);
    yup_mat[3][3] /= m_settings.global_scale; /* normalise the homogeneous component */
  }

  m_matrix = convert_matrix_datatype(yup_mat);
  m_sample.setMatrix(m_matrix);

  /* Always export as "inherits transform", as this is the only way in which Blender works. The
   * above code has already taken care of writing the correct matrix so that this option is not
   * necessary. However, certain packages (for example the USD Alembic exporter) are incompatible
   * with non-inheriting transforms and will completely ignore the transform if that is used. */
  m_sample.setInheritsXforms(true);
  m_schema.set(m_sample);
}

Imath::Box3d AbcTransformWriter::bounds()
{
  Imath::Box3d bounds;

  for (int i = 0; i < m_children.size(); i++) {
    Imath::Box3d box(m_children[i]->bounds());
    bounds.extendBy(box);
  }

  return Imath::transform(bounds, m_matrix);
}

bool AbcTransformWriter::hasAnimation(Object * /*ob*/) const
{
  return true;
}
