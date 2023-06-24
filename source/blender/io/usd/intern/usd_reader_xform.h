/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.h"
#include "usd_reader_prim.h"

namespace blender::io::usd {

class USDXformReader : public USDPrimReader {
 protected:
  bool use_parent_xform_;

  /* Indicates if the created object is the root of a
   * transform hierarchy. */
  bool is_root_xform_;

  bool needs_cachefile_;

 public:
  USDXformReader(const pxr::UsdPrim &prim,
                 const USDImportParams &import_params,
                 const ImportSettings &settings)
      : USDPrimReader(prim, import_params, settings),
        use_parent_xform_(false),
        is_root_xform_(is_root_xform_prim()),
        needs_cachefile_(false)
  {
  }

  void create_object(Main *bmain, double motionSampleTime) override;
  void read_object_data(Main *bmain, double motionSampleTime) override;

  bool needs_cachefile() override
  {
    return needs_cachefile_;
  }
  void apply_cache_file(CacheFile *cache_file) override;

  void read_matrix(float r_mat[4][4], const float time, const float scale, bool *r_is_constant);

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

  virtual bool get_local_usd_xform(pxr::GfMatrix4d *r_xform,
                                   bool *r_is_constant,
                                   const float time) const;
};

}  // namespace blender::io::usd
