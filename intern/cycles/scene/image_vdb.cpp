/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_vdb.h"

#include "util/image_metadata.h"
#include "util/log.h"
#include "util/nanovdb.h"
#include "util/openvdb.h"
#include "util/types_image.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/Dense.h>
#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENVDB
VDBImageLoader::VDBImageLoader(openvdb::GridBase::ConstPtr grid_,
                               const string &grid_name,
                               const float clipping)
    : grid_name(grid_name), clipping(clipping), grid(grid_)
{
}
#endif

VDBImageLoader::VDBImageLoader(const string &grid_name, const float clipping)
    : grid_name(grid_name), clipping(clipping)
{
}

VDBImageLoader::~VDBImageLoader() = default;

bool VDBImageLoader::load_metadata(ImageMetaData &metadata)
{
#ifdef WITH_NANOVDB
  load_grid();

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

  /* Convert OpenVDB to NanoVDB grid. */
  nanogrid = openvdb_to_nanovdb(grid, precision, clipping);
  if (!nanogrid) {
    grid.reset();
    return false;
  }

  /* Set dimensions. */
  bbox = grid->evalActiveVoxelBoundingBox();
  if (bbox.empty()) {
    metadata.type = IMAGE_DATA_TYPE_NANOVDB_EMPTY;
    metadata.nanovdb_byte_size = 1;
    grid.reset();
    return true;
  }

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
  else if (metadata.channels == 3) {
    metadata.type = IMAGE_DATA_TYPE_NANOVDB_FLOAT3;
  }
  else if (metadata.channels == 4) {
    metadata.type = IMAGE_DATA_TYPE_NANOVDB_FLOAT4;
  }
  else {
    grid.reset();
    return false;
  }

#  if NANOVDB_MAJOR_VERSION_NUMBER > 32 || \
      (NANOVDB_MAJOR_VERSION_NUMBER == 32 && NANOVDB_MINOR_VERSION_NUMBER >= 9)
  /* size() was deprecated in this version. */
  metadata.nanovdb_byte_size = nanogrid.bufferSize();
#  else
  metadata.nanovdb_byte_size = nanogrid.size();
#  endif

  /* Set transform from object space to voxel index. */
  openvdb::math::Mat4f grid_matrix = grid->transform().baseMap()->getAffineMap()->getMat4();
  Transform index_to_object;
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 3; row++) {
      index_to_object[row][col] = (float)grid_matrix[col][row];
    }
  }

  metadata.transform_3d = transform_inverse(index_to_object);
  metadata.use_transform_3d = true;

  /* Only NanoGrid needed now, free OpenVDB grid. */
  grid.reset();

  return true;
#else
  (void)metadata;
  return false;
#endif
}

bool VDBImageLoader::load_pixels(const ImageMetaData &metadata, void *pixels)
{
#ifdef WITH_NANOVDB
  if (metadata.type == IMAGE_DATA_TYPE_NANOVDB_EMPTY) {
    memset(pixels, 0, metadata.nanovdb_byte_size);
    return true;
  }
  if (nanogrid) {
    memcpy(pixels, nanogrid.data(), metadata.nanovdb_byte_size);
    return true;
  }
#else
  (void)metadata;
  (void)pixels;
#endif

  return false;
}

string VDBImageLoader::name() const
{
  return grid_name;
}

bool VDBImageLoader::equals(const ImageLoader &other) const
{
#ifdef WITH_OPENVDB
  const VDBImageLoader &other_loader = (const VDBImageLoader &)other;
  return grid && grid == other_loader.grid;
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

template<typename GridType>
openvdb::GridBase::ConstPtr create_grid(const float *voxels,
                                        const size_t width,
                                        const size_t height,
                                        const size_t depth,
                                        Transform transform_3d,
                                        const float clipping)
{
  using ValueType = typename GridType::ValueType;
  openvdb::GridBase::ConstPtr grid;

  const openvdb::CoordBBox dense_bbox(0, 0, 0, width - 1, height - 1, depth - 1);

  typename GridType::Ptr sparse = GridType::create(ValueType(0.0f));
  if (dense_bbox.empty()) {
    return sparse;
  }

  const openvdb::tools::Dense<const ValueType, openvdb::tools::MemoryLayout::LayoutXYZ> dense(
      dense_bbox, reinterpret_cast<const ValueType *>(voxels));

  openvdb::tools::copyFromDense(dense, *sparse, ValueType(clipping));

  /* Compute index to world matrix. */
  const float3 voxel_size = make_float3(1.0f / width, 1.0f / height, 1.0f / depth);

  transform_3d = transform_inverse(transform_3d);

  const openvdb::Mat4R index_to_world_mat((double)(voxel_size.x * transform_3d[0][0]),
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          (double)(voxel_size.y * transform_3d[1][1]),
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          (double)(voxel_size.z * transform_3d[2][2]),
                                          0.0,
                                          (double)transform_3d[0][3] + voxel_size.x,
                                          (double)transform_3d[1][3] + voxel_size.y,
                                          (double)transform_3d[2][3] + voxel_size.z,
                                          1.0);

  const openvdb::math::Transform::Ptr index_to_world_tfm =
      openvdb::math::Transform::createLinearTransform(index_to_world_mat);

  sparse->setTransform(index_to_world_tfm);

  return sparse;
}
#endif

void VDBImageLoader::grid_from_dense_voxels(const size_t width,
                                            const size_t height,
                                            const size_t depth,
                                            const int channels,
                                            const float *voxels,
                                            Transform transform_3d)
{
#ifdef WITH_OPENVDB
  /* TODO: Create NanoVDB grid directly? */
  if (channels == 1) {
    grid = create_grid<openvdb::FloatGrid>(voxels, width, height, depth, transform_3d, clipping);
  }
  else if (channels == 3) {
    grid = create_grid<openvdb::Vec3fGrid>(voxels, width, height, depth, transform_3d, clipping);
  }
  else if (channels == 4) {
    grid = create_grid<openvdb::Vec4fGrid>(voxels, width, height, depth, transform_3d, clipping);
  }

  /* Clipping already applied, no need to do it again. */
  clipping = 0.0f;
#else
  (void)width;
  (void)height;
  (void)depth;
  (void)channels;
  (void)voxels;
  (void)transform_3d;
#endif
}

CCL_NAMESPACE_END
