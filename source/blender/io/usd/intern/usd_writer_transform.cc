/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_transform.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/base/gf/matrix4f.h>
#include <pxr/usd/usdGeom/xform.h>

#include "BKE_object.h"

#include "BLI_math_matrix.h"

#include "DNA_layer_types.h"

namespace blender::io::usd {

USDTransformWriter::USDTransformWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

void USDTransformWriter::do_write(HierarchyContext &context)
{
  float parent_relative_matrix[4][4]; /* The object matrix relative to the parent. */
  mul_m4_m4m4(parent_relative_matrix, context.parent_matrix_inv_world, context.matrix_world);

  /* Write the transform relative to the parent. */
  pxr::UsdGeomXform xform = pxr::UsdGeomXform::Define(usd_export_context_.stage,
                                                      usd_export_context_.usd_path);
  if (!xformOp_) {
    xformOp_ = xform.AddTransformOp();
  }
  xformOp_.Set(pxr::GfMatrix4d(parent_relative_matrix), get_export_time_code());
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

}  // namespace blender::io::usd
