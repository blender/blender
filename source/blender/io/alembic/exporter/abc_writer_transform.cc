/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_transform.h"
#include "abc_hierarchy_iterator.h"
#include "intern/abc_axis_conversion.h"
#include "intern/abc_util.h"

#include "BKE_object.hh"

#include "BLI_math_euler_types.hh"
#include "BLI_math_matrix.hh"

#include "DNA_object_types.h"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"io.alembic"};

namespace io::alembic {

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
  CLOG_DEBUG(&LOG, "exporting %s", args_.abc_path.c_str());
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
  /* The object matrix relative to the parent. */
  float4x4 parent_relative_matrix = context.parent_matrix_inv_world * context.matrix_world;

  /* After this, parent_relative_matrix uses Y=up. */
  copy_m44_axis_swap(parent_relative_matrix.ptr(), parent_relative_matrix.ptr(), ABC_YUP_FROM_ZUP);

  /* If the parent is a camera, undo its to-Maya rotation (see below). */
  bool is_root_object = context.export_parent == nullptr;
  if (!is_root_object && context.export_parent->type == OB_CAMERA) {
    float4x4 rot_mat = math::from_rotation<float4x4>(math::EulerXYZ(M_PI_2, 0.0f, 0.0f));
    parent_relative_matrix = rot_mat * parent_relative_matrix;
  }

  /* If the object is a camera, apply an extra rotation to Maya camera orientation. */
  if (context.object->type == OB_CAMERA) {
    float4x4 rot_mat = math::from_rotation<float4x4>(math::EulerXYZ(-M_PI_2, 0.0f, 0.0f));
    parent_relative_matrix = parent_relative_matrix * rot_mat;
  }

  /* Only apply scaling to root objects, parenting will propagate it. */
  if (is_root_object) {
    /* A float4 so we also scale translation */
    const float4 scale(args_.export_params->global_scale);
    parent_relative_matrix = math::scale(parent_relative_matrix, scale);

    parent_relative_matrix[3][3] /=
        args_.export_params->global_scale; /* Normalize the homogeneous component. */
  }

  XformSample xform_sample;
  xform_sample.setMatrix(convert_matrix_datatype(parent_relative_matrix.ptr()));
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

}  // namespace io::alembic
}  // namespace blender
