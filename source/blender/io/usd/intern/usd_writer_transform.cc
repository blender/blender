/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_transform.hh"
#include "usd_hierarchy_iterator.hh"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/usd/usdGeom/xform.h>

#include "BKE_object.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace blender::io::usd {

USDTransformWriter::USDTransformWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

pxr::UsdGeomXformable USDTransformWriter::create_xformable() const
{
  pxr::UsdGeomXform xform;

  // If prim exists, cast to UsdGeomXform (Solves merge transform and shape issue for animated
  // exports)
  pxr::UsdPrim existing_prim = usd_export_context_.stage->GetPrimAtPath(
      usd_export_context_.usd_path);
  if (existing_prim.IsValid() && existing_prim.IsA<pxr::UsdGeomXform>()) {
    xform = pxr::UsdGeomXform(existing_prim);
  }
  else {
    xform = pxr::UsdGeomXform::Define(usd_export_context_.stage, usd_export_context_.usd_path);
  }

  return xform;
}

bool USDTransformWriter::should_apply_root_xform(const HierarchyContext &context) const
{
  if (!usd_export_context_.export_params.convert_orientation) {
    return false;
  }

  if (BLI_strnlen(usd_export_context_.export_params.root_prim_path, 1024) != 0) {
    return false;
  }

  if (context.export_parent != nullptr) {
    return false;
  }

  return true;
}

void USDTransformWriter::do_write(HierarchyContext &context)
{
  constexpr float UNIT_M4[4][4] = {
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
  };

  pxr::UsdGeomXformable xform = create_xformable();

  if (!xform) {
    CLOG_ERROR(&LOG, "USDTransformWriter: couldn't create xformable");
    return;
  }

  float parent_relative_matrix[4][4];  // The object matrix relative to the parent.

  if (should_apply_root_xform(context)) {
    float matrix_world[4][4];
    copy_m4_m4(matrix_world, context.matrix_world);

    if (usd_export_context_.export_params.convert_orientation) {
      float mrot[3][3];
      float mat[4][4];
      mat3_from_axis_conversion(IO_AXIS_Y,
                                IO_AXIS_Z,
                                usd_export_context_.export_params.forward_axis,
                                usd_export_context_.export_params.up_axis,
                                mrot);
      transpose_m3(mrot);
      copy_m4_m3(mat, mrot);
      mul_m4_m4m4(matrix_world, mat, context.matrix_world);
    }

    mul_m4_m4m4(parent_relative_matrix, context.parent_matrix_inv_world, matrix_world);
  }
  else {
    mul_m4_m4m4(parent_relative_matrix, context.parent_matrix_inv_world, context.matrix_world);
  }

  /* USD Xforms are by default set with an identity transform; only write if necessary. */
  if (!compare_m4m4(parent_relative_matrix, UNIT_M4, 0.000000001f)) {
    set_xform_ops(parent_relative_matrix, xform);
  }

  if (context.object) {
    auto prim = xform.GetPrim();
    write_id_properties(prim, context.object->id, get_export_time_code());
  }
}

bool USDTransformWriter::check_is_animated(const HierarchyContext &context) const
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

void USDTransformWriter::set_xform_ops(float xf_matrix[4][4], pxr::UsdGeomXformable &xf)
{
  if (!xf) {
    return;
  }

  eUSDXformOpMode xfOpMode = usd_export_context_.export_params.xform_op_mode;
  blender::Vector<pxr::UsdGeomXformOp> xformOps;

  switch (xfOpMode) {
    case USD_XFORM_OP_TRS:
      xformOps.append(xf.AddTranslateOp());
      xformOps.append(xf.AddRotateXYZOp());
      xformOps.append(xf.AddScaleOp());

      break;
    case USD_XFORM_OP_TOS:
      xformOps.append(xf.AddTranslateOp());
      xformOps.append(xf.AddOrientOp());
      xformOps.append(xf.AddScaleOp());
      break;
    case USD_XFORM_OP_MAT:
      xformOps.append(xf.AddTransformOp());
      break;
    default:
      CLOG_WARN(&LOG, "Warning: unknown XformOp type\n");
      xformOps.append(xf.AddTransformOp());
      break;
  }

  if (xformOps.is_empty()) {
    /* Shouldn't happen. */
    return;
  }

  pxr::UsdTimeCode time_code = get_export_time_code();

  if (xformOps.size() == 1) {
    pxr::GfMatrix4d mat_val(xf_matrix);
    usd_value_writer_.SetAttribute(xformOps[0].GetAttr(), mat_val, time_code);
  }
  else if (xformOps.size() == 3) {

    float loc[3];
    float quat[4];
    float scale[3];

    mat4_decompose(loc, quat, scale, xf_matrix);

    if (xfOpMode == USD_XFORM_OP_TRS) {
      float rot[3];
      quat_to_eul(rot, quat);
      rot[0] *= 180.0 / M_PI;
      rot[1] *= 180.0 / M_PI;
      rot[2] *= 180.0 / M_PI;

      pxr::GfVec3d loc_val(loc);
      usd_value_writer_.SetAttribute(xformOps[0].GetAttr(), loc_val, time_code);

      pxr::GfVec3f rot_val(rot);
      usd_value_writer_.SetAttribute(xformOps[1].GetAttr(), rot_val, time_code);

      pxr::GfVec3f scale_val(scale);
      usd_value_writer_.SetAttribute(xformOps[2].GetAttr(), scale_val, time_code);
    }
    else if (xfOpMode == USD_XFORM_OP_TOS) {
      pxr::GfVec3d loc_val(loc);
      usd_value_writer_.SetAttribute(xformOps[0].GetAttr(), loc_val, time_code);

      pxr::GfQuatf quat_val(quat[0], quat[1], quat[2], quat[3]);
      usd_value_writer_.SetAttribute(xformOps[1].GetAttr(), quat_val, time_code);

      pxr::GfVec3f scale_val(scale);
      usd_value_writer_.SetAttribute(xformOps[2].GetAttr(), scale_val, time_code);
    }
  }
}

}  // namespace blender::io::usd
