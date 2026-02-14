/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif
#ifdef WITH_NANOVDB
#  include <nanovdb/NanoVDB.h>
#  if NANOVDB_MAJOR_VERSION_NUMBER > 32 || \
      (NANOVDB_MAJOR_VERSION_NUMBER == 32 && NANOVDB_MINOR_VERSION_NUMBER >= 7)
#    include <nanovdb/GridHandle.h>
#  else
#    include <nanovdb/util/GridHandle.h>
#  endif
#endif

#include "scene/image_loader.h"

#include "util/transform.h"

CCL_NAMESPACE_BEGIN

class VDBImageLoader : public ImageLoader {
 public:
#ifdef WITH_OPENVDB
  VDBImageLoader(openvdb::GridBase::ConstPtr grid_,
                 const string &grid_name,
                 const float clipping = 0.001f);
#endif
  VDBImageLoader(const string &grid_name, const float clipping = 0.001f);
  ~VDBImageLoader() override;

  bool load_metadata(ImageMetaData &metadata) override;

  bool load_pixels(const ImageMetaData &metadata, void *pixels) override;

  string name() const override;

  bool equals(const ImageLoader &other) const override;

  void cleanup() override;

  bool is_vdb_loader() const override;

#ifdef WITH_OPENVDB
  openvdb::GridBase::ConstPtr get_grid();
#endif

 protected:
  virtual void load_grid() {}

  void grid_from_dense_voxels(const size_t width,
                              const size_t height,
                              const size_t depth,
                              const int channels,
                              const float *voxels,
                              Transform transform_3d);

  string grid_name;
  float clipping = 0.001f;
#ifdef WITH_OPENVDB
  openvdb::GridBase::ConstPtr grid;
  openvdb::CoordBBox bbox;
#endif
#ifdef WITH_NANOVDB
  nanovdb::GridHandle<> nanogrid;
  int precision = 16;
#endif
};

CCL_NAMESPACE_END
