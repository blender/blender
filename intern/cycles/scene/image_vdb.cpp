/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_vdb.h"

#include "util/log.h"
#include "util/nanovdb.h"
#include "util/openvdb.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/Dense.h>
#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENVDB
struct ToDenseOp {
  openvdb::CoordBBox bbox;
  void *pixels;

  template<typename GridType, typename FloatDataType, const int channels>
  bool operator()(const typename GridType::ConstPtr &grid)
  {
    openvdb::tools::Dense<FloatDataType, openvdb::tools::LayoutXYZ> dense(bbox,
                                                                          (FloatDataType *)pixels);
    openvdb::tools::copyToDense(*grid, dense);
    return true;
  }
};

VDBImageLoader::VDBImageLoader(openvdb::GridBase::ConstPtr grid_, const string &grid_name)
    : grid_name(grid_name), grid(grid_)
{
}
#endif

VDBImageLoader::VDBImageLoader(const string &grid_name) : grid_name(grid_name) {}

VDBImageLoader::~VDBImageLoader() = default;

bool VDBImageLoader::load_metadata(const ImageDeviceFeatures &features, ImageMetaData &metadata)
{
#ifdef WITH_OPENVDB
  if (!grid) {
    return false;
  }

  /* Convert to the few float types that we know. */
  grid = openvdb_convert_to_known_type(grid);
  if (!grid) {
    return false;
  }

  /* Get number of channels from type. */
  metadata.channels = openvdb_num_channels(grid);

  /* Set data type. */
#  ifdef WITH_NANOVDB
  if (features.has_nanovdb) {
    /* Convert OpenVDB to NanoVDB grid. */
    nanogrid = openvdb_to_nanovdb(grid, precision, 0.0f);
    if (!nanogrid) {
      return false;
    }
  }
#  endif

  /* Set dimensions. */
  bbox = grid->evalActiveVoxelBoundingBox();
  if (bbox.empty()) {
    return false;
  }

  openvdb::Coord dim = bbox.dim();
  metadata.width = dim.x();
  metadata.height = dim.y();
  metadata.depth = dim.z();

#  ifdef WITH_NANOVDB
  if (nanogrid) {
    metadata.byte_size = nanogrid.size();
    if (metadata.channels == 1) {
      if (precision == 0) {
        metadata.type = IMAGE_DATA_TYPE_NANOVDB_FPN;
      }
      else if (precision == 16) {
        metadata.type = IMAGE_DATA_TYPE_NANOVDB_FP16;
      }
      else {
        metadata.type = IMAGE_DATA_TYPE_NANOVDB_FLOAT;
      }
    }
    else {
      metadata.type = IMAGE_DATA_TYPE_NANOVDB_FLOAT3;
    }
  }
  else
#  endif
  {
    if (metadata.channels == 1) {
      metadata.type = IMAGE_DATA_TYPE_FLOAT;
    }
    else {
      metadata.type = IMAGE_DATA_TYPE_FLOAT4;
    }
  }

  /* Set transform from object space to voxel index. */
  openvdb::math::Mat4f grid_matrix = grid->transform().baseMap()->getAffineMap()->getMat4();
  Transform index_to_object;
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 3; row++) {
      index_to_object[row][col] = (float)grid_matrix[col][row];
    }
  }

  Transform texture_to_index;
#  ifdef WITH_NANOVDB
  if (nanogrid) {
    texture_to_index = transform_identity();
  }
  else
#  endif
  {
    openvdb::Coord min = bbox.min();
    texture_to_index = transform_translate(min.x(), min.y(), min.z()) *
                       transform_scale(dim.x(), dim.y(), dim.z());
  }

  metadata.transform_3d = transform_inverse(index_to_object * texture_to_index);
  metadata.use_transform_3d = true;

#  ifndef WITH_NANOVDB
  (void)features;
#  endif
  return true;
#else
  (void)metadata;
  (void)features;
  return false;
#endif
}

bool VDBImageLoader::load_pixels(const ImageMetaData & /*metadata*/,
                                 void *pixels,
                                 const size_t /*pixels_size*/,
                                 const bool /*associate_alpha*/)
{
#ifdef WITH_OPENVDB
#  ifdef WITH_NANOVDB
  if (nanogrid) {
    memcpy(pixels, nanogrid.data(), nanogrid.size());
  }
  else
#  endif
  {
    ToDenseOp op;
    op.pixels = pixels;
    op.bbox = bbox;
    openvdb_grid_type_operation(grid, op);
  }
  return true;
#else
  (void)pixels;
  return false;
#endif
}

string VDBImageLoader::name() const
{
  return grid_name;
}

bool VDBImageLoader::equals(const ImageLoader &other) const
{
#ifdef WITH_OPENVDB
  const VDBImageLoader &other_loader = (const VDBImageLoader &)other;
  return grid == other_loader.grid;
#else
  (void)other;
  return true;
#endif
}

void VDBImageLoader::cleanup()
{
#ifdef WITH_OPENVDB
  /* Free OpenVDB grid memory as soon as we can. */
  grid.reset();
#endif
#ifdef WITH_NANOVDB
  nanogrid.reset();
#endif
}

bool VDBImageLoader::is_vdb_loader() const
{
  return true;
}

#ifdef WITH_OPENVDB
openvdb::GridBase::ConstPtr VDBImageLoader::get_grid()
{
  return grid;
}
#endif

CCL_NAMESPACE_END
