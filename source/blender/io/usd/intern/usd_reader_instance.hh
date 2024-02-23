/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "usd_reader_xform.hh"

#include <pxr/usd/usdGeom/xform.h>

struct Collection;

namespace blender::io::usd {

/**
 * Convert a USD instanced prim to a blender collection instance.
 */

class USDInstanceReader : public USDXformReader {

 public:
  USDInstanceReader(const pxr::UsdPrim &prim,
                    const USDImportParams &import_params,
                    const ImportSettings &settings);

  bool valid() const override;

  /**
   * Create an object that instances a collection.
   */
  void create_object(Main *bmain, double motionSampleTime) override;

  /**
   * Assign the given collection to the object.
   */
  void set_instance_collection(Collection *coll);

  /**
   * Get the path of the USD prototype prim.
   */
  pxr::SdfPath proto_path() const;
};

}  // namespace blender::io::usd
