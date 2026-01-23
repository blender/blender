/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.hh"

#include "BLI_math_matrix_types.hh"
#include "BLI_vector.hh"

#include <pxr/usd/usdGeom/xformable.h>

namespace blender::io::usd {

class USDTransformWriter : public USDAbstractWriter {
 private:
  Vector<pxr::UsdGeomXformOp> xformOps_;

 public:
  USDTransformWriter(const USDExporterContext &ctx);

 protected:
  void do_write(HierarchyContext &context) override;
  bool check_is_animated(const HierarchyContext &context) const override;
  bool should_apply_root_xform(const HierarchyContext &context) const;
  void set_xform_ops(const float4x4 &parent_relative_matrix, const pxr::UsdGeomXformable &xf);

  /* Subclasses may override this to create prims other than UsdGeomXform. */
  virtual pxr::UsdGeomXformable create_xformable() const;
};

}  // namespace blender::io::usd
