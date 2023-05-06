/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */
#pragma once

#include "usd_writer_abstract.h"

#include <pxr/usd/usdGeom/xform.h>

#include <vector>

namespace blender::io::usd {

void get_export_conversion_matrix(const USDExportParams &params, float r_mat[4][4]);

class USDTransformWriter : public USDAbstractWriter {
 private:
  std::vector<pxr::UsdGeomXformOp> xformOps_;

 public:
  USDTransformWriter(const USDExporterContext &ctx);

 protected:
  void do_write(HierarchyContext &context) override;
  bool check_is_animated(const HierarchyContext &context) const override;

  void set_xform_ops(float parent_relative_matrix[4][4], pxr::UsdGeomXformable &xf);

  /* Return true if the given context is the root of a protoype. */
  bool is_proto_root(const HierarchyContext &context) const;

  /* Subclasses may override this to create prims other than UsdGeomXform. */
  virtual pxr::UsdGeomXformable create_xformable() const;

  bool should_apply_root_xform(const HierarchyContext &context) const;
};

}  // namespace blender::io::usd
