/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.h"
#include "usd_reader_prim.hh"

namespace blender::io::usd {

/**
 * A transformation matrix and a boolean indicating
 * whether the matrix is constant over time.
 */
using XformResult = std::tuple<pxr::GfMatrix4f, bool>;

class USDXformReader : public USDPrimReader {
 private:
  bool use_parent_xform_;

  /* Indicates if the created object is the root of a
   * transform hierarchy. */
  bool is_root_xform_;

 public:
  USDXformReader(const pxr::UsdPrim &prim,
                 const USDImportParams &import_params,
                 const ImportSettings &settings)
      : USDPrimReader(prim, import_params, settings),
        use_parent_xform_(false),
        is_root_xform_(is_root_xform_prim())
  {
  }

  void create_object(Main *bmain, double motionSampleTime) override;
  void read_object_data(Main *bmain, double motionSampleTime) override;

  void read_matrix(float r_mat[4][4], float time, float scale, bool *r_is_constant);

  bool use_parent_xform() const
  {
    return use_parent_xform_;
  }
  void set_use_parent_xform(bool flag)
  {
    use_parent_xform_ = flag;
    is_root_xform_ = is_root_xform_prim();
  }

  bool prim_has_xform_ops() const;

 protected:
  /* Returns true if the contained USD prim is the root of a transform hierarchy. */
  bool is_root_xform_prim() const;

  /**
   * Return the USD prim's local transformation.
   *
   * \param time: Time code for evaluating the transform.
   *
   * \return: Optional tuple with the following elements:
   *          - The transform matrix.
   *          - A boolean flag indicating whether the matrix
   *            is constant over time.
   */
  virtual std::optional<XformResult> get_local_usd_xform(float time) const;
};

}  // namespace blender::io::usd
