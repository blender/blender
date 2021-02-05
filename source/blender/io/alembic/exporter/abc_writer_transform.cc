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
#include "abc_hierarchy_iterator.h"
#include "intern/abc_axis_conversion.h"
#include "intern/abc_util.h"

#include "BKE_object.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "DNA_layer_types.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

namespace blender::io::alembic {

using Alembic::Abc::OObject;
using Alembic::AbcGeom::OXform;
using Alembic::AbcGeom::OXformSchema;
using Alembic::AbcGeom::XformSample;

ABCTransformWriter::ABCTransformWriter(const ABCWriterConstructorArgs &args)
    : ABCAbstractWriter(args)
{
  timesample_index_ = args_.abc_archive->time_sampling_index_transforms();
}

void ABCTransformWriter::create_alembic_objects(const HierarchyContext * /*context*/)
{
  CLOG_INFO(&LOG, 2, "exporting %s", args_.abc_path.c_str());
  abc_xform_ = OXform(args_.abc_parent, args_.abc_name, timesample_index_);
  abc_xform_schema_ = abc_xform_.getSchema();
}

Alembic::Abc::OCompoundProperty ABCTransformWriter::abc_prop_for_custom_props()
{
  return abc_schema_prop_for_custom_props<OXformSchema>(abc_xform_schema_);
}

const IDProperty *ABCTransformWriter::get_id_properties(const HierarchyContext &context) const
{
  const Object *object = context.object;
  return object->id.properties;
}

void ABCTransformWriter::do_write(HierarchyContext &context)
{
  float parent_relative_matrix[4][4]; /* The object matrix relative to the parent. */
  mul_m4_m4m4(parent_relative_matrix, context.parent_matrix_inv_world, context.matrix_world);

  /* After this, parent_relative_matrix uses Y=up. */
  copy_m44_axis_swap(parent_relative_matrix, parent_relative_matrix, ABC_YUP_FROM_ZUP);

  /* If the parent is a camera, undo its to-Maya rotation (see below). */
  bool is_root_object = context.export_parent == nullptr;
  if (!is_root_object && context.export_parent->type == OB_CAMERA) {
    float rot_mat[4][4];
    axis_angle_to_mat4_single(rot_mat, 'X', M_PI_2);
    mul_m4_m4m4(parent_relative_matrix, rot_mat, parent_relative_matrix);
  }

  /* If the object is a camera, apply an extra rotation to Maya camera orientation. */
  if (context.object->type == OB_CAMERA) {
    float rot_mat[4][4];
    axis_angle_to_mat4_single(rot_mat, 'X', -M_PI_2);
    mul_m4_m4m4(parent_relative_matrix, parent_relative_matrix, rot_mat);
  }

  if (is_root_object) {
    /* Only apply scaling to root objects, parenting will propagate it. */
    float scale_mat[4][4];
    scale_m4_fl(scale_mat, args_.export_params->global_scale);
    scale_mat[3][3] = args_.export_params->global_scale; /* also scale translation */
    mul_m4_m4m4(parent_relative_matrix, parent_relative_matrix, scale_mat);
    parent_relative_matrix[3][3] /=
        args_.export_params->global_scale; /* Normalize the homogeneous component. */
  }

  XformSample xform_sample;
  xform_sample.setMatrix(convert_matrix_datatype(parent_relative_matrix));
  xform_sample.setInheritsXforms(true);
  abc_xform_schema_.set(xform_sample);

  write_visibility(context);
}

OObject ABCTransformWriter::get_alembic_object() const
{
  return abc_xform_;
}

bool ABCTransformWriter::check_is_animated(const HierarchyContext &context) const
{
  if (context.duplicator != nullptr) {
    /* This object is being duplicated, so could be emitted by a particle system and thus
     * influenced by forces. TODO(Sybren): Make this more strict. Probably better to get from the
     * depsgraph whether this object instance has a time source. */
    return true;
  }
  if (check_has_physics(context)) {
    return true;
  }
  return BKE_object_moves_in_time(context.object, context.animation_check_include_parent);
}

}  // namespace blender::io::alembic
