/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.hh"
#include "usd_reader_prim.hh"

/* For #UsdGeomXformable. */
#include <pxr/usd/usdGeom/xformable.h>

namespace blender {

struct Main;

namespace io::usd {

/**
 * A transformation matrix and a boolean indicating
 * whether the matrix is constant over time.
 */
using XformResult = std::tuple<pxr::GfMatrix4f, bool>;

class USDXformReader : public USDPrimReader {
 private:
  bool use_parent_xform_ = false;

 public:
  USDXformReader(const pxr::UsdPrim &prim,
                 const USDImportParams &import_params,
                 const ImportSettings &settings)
      : USDPrimReader(prim, import_params, settings)
  {
  }

  void create_object(Main *bmain) override;
  void read_object_data(Main *bmain, pxr::UsdTimeCode time) override;

  pxr::SdfPath object_prim_path() const override;

  void read_matrix(float4x4 &r_mat, pxr::UsdTimeCode time, float scale, bool *r_is_constant) const;

  bool use_parent_xform() const
  {
    return use_parent_xform_;
  }
  void set_use_parent_xform(bool flag)
  {
    use_parent_xform_ = flag;
  }

  bool prim_has_xform_ops() const;

  /**
   * Return true if the prim's transform ops start with `!resetXformStack!`, meaning the prim
   * doesn't inherit the transform of its parent and its local transform is its world transform.
   * Like #get_local_usd_xform, this checks the parent Xform when this reader is merged with it
   * (#use_parent_xform), since that is the prim the object represents.
   */
  bool resets_xform_stack() const;

  /**
   * Return true if the transform of an ancestor that this prim would otherwise inherit might be
   * time-varying. Ancestors above a prim that resets the transform stack are ignored.
   */
  bool ancestor_xform_might_be_time_varying() const;

 protected:
  /* Returns true if the contained USD prim is the root of a transform hierarchy. */
  virtual bool is_root_xform_prim() const;

  /**
   * Return the USD prim's local transformation.
   *
   * \param time: Time code for evaluating the transform.
   *
   * \return Optional tuple with the following elements:
   *         - The transform matrix.
   *         - A boolean flag indicating whether the matrix
   *           is constant over time.
   */
  virtual std::optional<XformResult> get_local_usd_xform(pxr::UsdTimeCode time) const;

 private:
  pxr::UsdGeomXformable get_xformable() const;
};

}  // namespace io::usd
}  // namespace blender
