/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 Tangent Animation.
 * All rights reserved.
 */
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
                          double motionSampleTime,
                          int read_flag,
                          const char **err_str) = 0;

  virtual bool topology_changed(Mesh * /* existing_mesh */, double /* motionSampleTime */)
  {
    return true;
  }

  void add_cache_modifier();
  void add_subdiv_modifier();
};

}  // namespace blender::io::usd
