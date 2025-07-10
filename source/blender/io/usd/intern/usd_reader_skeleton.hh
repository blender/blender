/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"
#include "usd_reader_xform.hh"

#include <pxr/usd/usdSkel/skeleton.h>

namespace blender::io::usd {

class USDSkeletonReader : public USDXformReader {
 private:
  pxr::UsdSkelSkeleton skel_;

 public:
  USDSkeletonReader(const pxr::UsdPrim &prim,
                    const USDImportParams &import_params,
                    const ImportSettings &settings)
      : USDXformReader(prim, import_params, settings), skel_(prim)
  {
  }

  bool valid() const override
  {
    return bool(skel_);
  }

  void create_object(Main *bmain) override;
  void read_object_data(Main *bmain, pxr::UsdTimeCode time) override;
};

}  // namespace blender::io::usd
