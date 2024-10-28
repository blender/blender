/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_reader_xform.hh"

#include <pxr/usd/usdGeom/pointInstancer.h>

struct Collection;

namespace blender::io::usd {

/* Wraps the UsdGeomPointInstancer schema. Creates a Blender point cloud object. */

class USDPointInstancerReader : public USDXformReader {
 private:
  pxr::UsdGeomPointInstancer point_instancer_prim_;

 public:
  USDPointInstancerReader(const pxr::UsdPrim &prim,
                          const USDImportParams &import_params,
                          const ImportSettings &settings)
      : USDXformReader(prim, import_params, settings), point_instancer_prim_(prim)
  {
  }

  bool valid() const override
  {
    return bool(point_instancer_prim_);
  }

  void create_object(Main *bmain, double motionSampleTime) override;

  void read_object_data(Main *bmain, double motionSampleTime) override;

  pxr::SdfPathVector proto_paths() const;

  /**
   * Set the given collection on the Collection Info
   * node referenced by the geometry nodes modifier
   * on the object created by the reader.  This assumes
   * create_object() and read_object_data() have already
   * been called.
   *
   * \param bmain: Pointer to Main
   * \param coll: The collection to set
   */
  void set_collection(Main *bmain, Collection &coll);
};

}  // namespace blender::io::usd
