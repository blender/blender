/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"
#include "usd_reader_prim.hh"

#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/domeLight_1.h>

struct Main;
struct Scene;

namespace blender::io::usd {

class USDDomeLightReader : public USDPrimReader {

 public:
  USDDomeLightReader(const pxr::UsdPrim &prim,
                     const USDImportParams &import_params,
                     const ImportSettings &settings)
      : USDPrimReader(prim, import_params, settings)
  {
  }

  bool valid() const override
  {
    return prim_.IsA<pxr::UsdLuxDomeLight>() || prim_.IsA<pxr::UsdLuxDomeLight_1>();
  }

  /* Until Blender supports DomeLight objects natively, use a separate create_object overload that
   * allows the caller to pass in the required Scene data. */

  void create_object(Main * /*bmain*/) override {};
  void create_object(Scene *scene, Main *bmain);
};

}  // namespace blender::io::usd
