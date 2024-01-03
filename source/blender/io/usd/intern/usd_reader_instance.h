<<<<<<< HEAD
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
 * The Original Code is Copyright (C) 2021 NVIDIA Corporation.
 * All rights reserved.
 */
=======
/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
>>>>>>> main
#pragma once

#include "usd_reader_xform.h"

#include <pxr/usd/usdGeom/xform.h>

struct Collection;

namespace blender::io::usd {

<<<<<<< HEAD
/* Wraps the UsdGeomXform schema. Creates a Blender Empty object. */

=======
/**
 * Convert a USD instanced prim to a blender collection instance.
 */
>>>>>>> main
class USDInstanceReader : public USDXformReader {

 public:
  USDInstanceReader(const pxr::UsdPrim &prim,
                    const USDImportParams &import_params,
                    const ImportSettings &settings);

  bool valid() const override;

<<<<<<< HEAD
  void create_object(Main *bmain, double motionSampleTime) override;

  void set_instance_collection(Collection *coll);

=======
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
>>>>>>> main
  pxr::SdfPath proto_path() const;
};

}  // namespace blender::io::usd
