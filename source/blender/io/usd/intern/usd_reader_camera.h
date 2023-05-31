/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation.
 * Modifications Copyright 2021 Tangent Animation. All rights reserved. */
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
