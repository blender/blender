/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __IMAGE_VDB__
#define __IMAGE_VDB__

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif
#ifdef WITH_NANOVDB
#  include <nanovdb/util/GridHandle.h>
#endif

#include "scene/image.h"

CCL_NAMESPACE_BEGIN

class VDBImageLoader : public ImageLoader {
 public:
#ifdef WITH_OPENVDB
  VDBImageLoader(openvdb::GridBase::ConstPtr grid_, const string &grid_name);
#endif
  VDBImageLoader(const string &grid_name);
  ~VDBImageLoader();

  virtual bool load_metadata(const ImageDeviceFeatures &features,
                             ImageMetaData &metadata) override;

  virtual bool load_pixels(const ImageMetaData &metadata,
                           void *pixels,
                           const size_t pixels_size,
                           const bool associate_alpha) override;

  virtual string name() const override;

  virtual bool equals(const ImageLoader &other) const override;

  virtual void cleanup() override;

  virtual bool is_vdb_loader() const override;

#ifdef WITH_OPENVDB
  openvdb::GridBase::ConstPtr get_grid();
#endif

 protected:
  string grid_name;
#ifdef WITH_OPENVDB
  openvdb::GridBase::ConstPtr grid;
  openvdb::CoordBBox bbox;
#endif
#ifdef WITH_NANOVDB
  nanovdb::GridHandle<> nanogrid;
  int precision = 0;
#endif
};

CCL_NAMESPACE_END

#endif /* __IMAGE_VDB__ */
