/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.h"
#include "usd_reader_xform.h"

struct Mesh;

namespace blender::io::usd {

class USDGeomReader : public USDXformReader {

 public:
  USDGeomReader(const pxr::UsdPrim &prim,
                const USDImportParams &import_params,
                const ImportSettings &settings)
      : USDXformReader(prim, import_params, settings)
  {
  }

  virtual Mesh *read_mesh(struct Mesh *existing_mesh,
                          USDMeshReadParams params,
                          const char **err_str) = 0;

  virtual bool topology_changed(const Mesh * /* existing_mesh */, double /* motionSampleTime */)
  {
    return true;
  }

  void add_cache_modifier();
  void add_subdiv_modifier();
};

}  // namespace blender::io::usd
