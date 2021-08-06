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
 * Adapted from the Blender Alembic importer implementation.
 *
 * Modifications Copyright (C) 2021 Tangent Animation.
 * All rights reserved.
 */
#pragma once

#include "usd.h"
#include "usd_reader_xform.h"

namespace blender::io::usd {

class USDCameraReader : public USDXformReader {

 public:
  USDCameraReader(const pxr::UsdPrim &object,
                  const USDImportParams &import_params,
                  const ImportSettings &settings)
      : USDXformReader(object, import_params, settings)
  {
  }

  void create_object(Main *bmain, double motionSampleTime) override;
  void read_object_data(Main *bmain, double motionSampleTime) override;
};

}  // namespace blender::io::usd
