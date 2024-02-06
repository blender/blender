/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.hh"
#include "usd_reader_xform.hh"

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
