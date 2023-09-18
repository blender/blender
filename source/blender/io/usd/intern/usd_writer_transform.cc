/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_transform.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/quaternion.h>
#include <pxr/usd/usdGeom/xform.h>

#include "BKE_object.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "DNA_layer_types.h"

namespace blender::io::usd {

static const float UNIT_M4[4][4] = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1},
};

/* Returns in r_mat tha unit scaling and axis rotation transforms
 * applied to root prims on export. */
void get_export_conversion_matrix(const USDExportParams &params, float r_mat[4][4])
{
  unit_m4(r_mat);

  if (params.convert_orientation) {
    float mrot[3][3];
    mat3_from_axis_conversion(
        USD_GLOBAL_FORWARD_Y, USD_GLOBAL_UP_Z, params.forward_axis, params.up_axis, mrot);
    transpose_m3(mrot);
    copy_m4_m3(r_mat, mrot);
  }

  if (params.convert_to_cm) {
    float scale_mat[4][4];
    scale_m4_fl(scale_mat, 100.0f);
    mul_m4_m4m4(r_mat, scale_mat, r_mat);
  }
}

USDTransformWriter::USDTransformWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

pxr::UsdGeomXformable USDTransformWriter::create_xformable() const
{
  pxr::UsdGeomXform xform;

  if (usd_export_context_.export_params.export_as_overs) {
    // Override existing prim on stage
    xform = pxr::UsdGeomXform(
        usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path));
  }
  else {
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
  }

  return xform;
}

bool USDTransformWriter::should_apply_root_xform(const HierarchyContext &context) const
{
  if (!(usd_export_context_.export_params.convert_orientation ||
        usd_export_context_.export_params.convert_to_cm)) {
    return false;
  }

  if (strlen(usd_export_context_.export_params.root_prim_path) != 0) {
    return false;
  }

  if (context.export_parent != nullptr) {
    return false;
  }

  if (usd_export_context_.export_params.use_instancing && context.is_prototype()) {
    /* This is an instancing prototype. */
    return false;
  }

  return true;
}

void USDTransformWriter::do_write(HierarchyContext &context)
{
  pxr::UsdGeomXformable xform = create_xformable();

  if (!xform) {
    printf("INTERNAL ERROR: USDTransformWriter: couldn't create xformable.\n");
    return;
  }

  if (usd_export_context_.export_params.export_transforms) {
    float parent_relative_matrix[4][4];  // The object matrix relative to the parent.

    // TODO(makowalski): This is inefficient checking for every transform and should be moved elsewhere.
    // TODO(makowalski): Use get_export_conversion_matrix() here, to avoid duplicating code.
    if (should_apply_root_xform(context)) {
      float matrix_world[4][4];
      copy_m4_m4(matrix_world, context.matrix_world);

      if (usd_export_context_.export_params.convert_orientation) {
        float mrot[3][3];
        float mat[4][4];
        mat3_from_axis_conversion(USD_GLOBAL_FORWARD_Y,
                                  USD_GLOBAL_UP_Z,
                                  usd_export_context_.export_params.forward_axis,
                                  usd_export_context_.export_params.up_axis,
                                  mrot);
        transpose_m3(mrot);
        copy_m4_m3(mat, mrot);
        mul_m4_m4m4(matrix_world, mat, context.matrix_world);
      }

      if (usd_export_context_.export_params.convert_to_cm) {
        float scale_mat[4][4];
        scale_m4_fl(scale_mat, 100.0f);
        mul_m4_m4m4(matrix_world, scale_mat, matrix_world);
      }

      mul_m4_m4m4(parent_relative_matrix, context.parent_matrix_inv_world, matrix_world);
    }
    else
      mul_m4_m4m4(parent_relative_matrix, context.parent_matrix_inv_world, context.matrix_world);

    // USD Xforms are by default set with an identity transform.
    // This check ensures transforms of non-identity are authored
    // preventing usd composition collisions up and down stream.
    if (usd_export_context_.export_params.export_identity_transforms ||
        !compare_m4m4(parent_relative_matrix, UNIT_M4, 0.000000001f)) {
      set_xform_ops(parent_relative_matrix, xform);
    }
  }

  if (usd_export_context_.export_params.export_custom_properties && context.object) {
    auto prim = xform.GetPrim();
    write_id_properties(prim, context.object->id, get_export_time_code());
  }

  if (usd_export_context_.export_params.use_instancing) {
    if (context.is_instance()) {
      mark_as_instance(context, xform.GetPrim());
      /* Explicitly set visibility, since the prototype might be invisible. */
      xform.GetVisibilityAttr().Set(pxr::UsdGeomTokens->inherited);
    }
    else if (context.is_prototype() && !context.export_parent) {
      /* TODO(makowalski): perhaps making prototypes invisible should be optional. */
      xform.GetVisibilityAttr().Set(pxr::UsdGeomTokens->invisible);
    }
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

  // TODO: This fails for a specific set of drivers and rig setups...
  // Setting 'context.animation_check_include_parent' to true fixed it...
  return BKE_object_moves_in_time(context.object, context.animation_check_include_parent);
}

void USDTransformWriter::set_xform_ops(float xf_matrix[4][4], pxr::UsdGeomXformable &xf)
{
  if (!xf) {
    return;
  }

  eUSDXformOpMode xfOpMode = usd_export_context_.export_params.xform_op_mode;

  if (xformOps_.empty()) {
    switch (xfOpMode) {
      case USD_XFORM_OP_SRT:
        xformOps_.push_back(xf.AddTranslateOp());
        xformOps_.push_back(xf.AddRotateXYZOp());
        xformOps_.push_back(xf.AddScaleOp());

        break;
      case USD_XFORM_OP_SOT:
        xformOps_.push_back(xf.AddTranslateOp());
        xformOps_.push_back(xf.AddOrientOp());
        xformOps_.push_back(xf.AddScaleOp());
        break;
      case USD_XFORM_OP_MAT:
        xformOps_.push_back(xf.AddTransformOp());
        break;
      default:
        printf("Warning: unknown XformOp type\n");
        xformOps_.push_back(xf.AddTransformOp());
        break;
    }
  }

  if (xformOps_.empty()) {
    /* Shouldn't happen. */
    return;
  }

  if (xformOps_.size() == 1) {
    xformOps_[0].Set(pxr::GfMatrix4d(xf_matrix), get_export_time_code());
  }
  else if (xformOps_.size() == 3) {

    float loc[3];
    float quat[4];
    float scale[3];

    mat4_decompose(loc, quat, scale, xf_matrix);

    if (xfOpMode == USD_XFORM_OP_SRT) {
      float rot[3];
      quat_to_eul(rot, quat);
      rot[0] *= 180.0 / M_PI;
      rot[1] *= 180.0 / M_PI;
      rot[2] *= 180.0 / M_PI;
      xformOps_[0].Set(pxr::GfVec3d(loc), get_export_time_code());
      xformOps_[1].Set(pxr::GfVec3f(rot), get_export_time_code());
      xformOps_[2].Set(pxr::GfVec3f(scale), get_export_time_code());
    }
    else if (xfOpMode == USD_XFORM_OP_SOT) {
      xformOps_[0].Set(pxr::GfVec3d(loc), get_export_time_code());
      xformOps_[1].Set(pxr::GfQuatf(quat[0], quat[1], quat[2], quat[3]), get_export_time_code());
      xformOps_[2].Set(pxr::GfVec3f(scale), get_export_time_code());
    }
  }
}

}  // namespace blender::io::usd
