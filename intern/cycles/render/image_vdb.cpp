/*
 * Copyright 2011-2020 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/image_vdb.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Dense.h>
#endif
#ifdef WITH_NANOVDB
#  include <nanovdb/util/OpenToNanoVDB.h>
#endif

CCL_NAMESPACE_BEGIN

VDBImageLoader::VDBImageLoader(const string &grid_name) : grid_name(grid_name)
{
}

VDBImageLoader::~VDBImageLoader()
{
}

bool VDBImageLoader::load_metadata(ImageMetaData &metadata)
{
#ifdef WITH_OPENVDB
  if (!grid) {
    return false;
  }

  bbox = grid->evalActiveVoxelBoundingBox();
  if (bbox.empty()) {
    return false;
  }

  /* Set dimensions. */
  openvdb::Coord dim = bbox.dim();
  metadata.width = dim.x();
  metadata.height = dim.y();
  metadata.depth = dim.z();

  /* Set data type. */
  if (grid->isType<openvdb::FloatGrid>()) {
    metadata.channels = 1;
#  ifdef WITH_NANOVDB
    nanogrid = nanovdb::openToNanoVDB(*openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid));
#  endif
  }
  else if (grid->isType<openvdb::Vec3fGrid>()) {
    metadata.channels = 3;
#  ifdef WITH_NANOVDB
    nanogrid = nanovdb::openToNanoVDB(*openvdb::gridConstPtrCast<openvdb::Vec3fGrid>(grid));
#  endif
  }
  else if (grid->isType<openvdb::BoolGrid>()) {
    metadata.channels = 1;
#  ifdef WITH_NANOVDB
    nanogrid = nanovdb::openToNanoVDB(
        openvdb::FloatGrid(*openvdb::gridConstPtrCast<openvdb::BoolGrid>(grid)));
#  endif
  }
  else if (grid->isType<openvdb::DoubleGrid>()) {
    metadata.channels = 1;
#  ifdef WITH_NANOVDB
    nanogrid = nanovdb::openToNanoVDB(
        openvdb::FloatGrid(*openvdb::gridConstPtrCast<openvdb::DoubleGrid>(grid)));
#  endif
  }
  else if (grid->isType<openvdb::Int32Grid>()) {
    metadata.channels = 1;
#  ifdef WITH_NANOVDB
    nanogrid = nanovdb::openToNanoVDB(
        openvdb::FloatGrid(*openvdb::gridConstPtrCast<openvdb::Int32Grid>(grid)));
#  endif
  }
  else if (grid->isType<openvdb::Int64Grid>()) {
    metadata.channels = 1;
#  ifdef WITH_NANOVDB
    nanogrid = nanovdb::openToNanoVDB(
        openvdb::FloatGrid(*openvdb::gridConstPtrCast<openvdb::Int64Grid>(grid)));
#  endif
  }
  else if (grid->isType<openvdb::Vec3IGrid>()) {
    metadata.channels = 3;
#  ifdef WITH_NANOVDB
    nanogrid = nanovdb::openToNanoVDB(
        openvdb::Vec3fGrid(*openvdb::gridConstPtrCast<openvdb::Vec3IGrid>(grid)));
#  endif
  }
  else if (grid->isType<openvdb::Vec3dGrid>()) {
    metadata.channels = 3;
#  ifdef WITH_NANOVDB
    nanogrid = nanovdb::openToNanoVDB(
        openvdb::Vec3fGrid(*openvdb::gridConstPtrCast<openvdb::Vec3dGrid>(grid)));
#  endif
  }
  else if (grid->isType<openvdb::MaskGrid>()) {
    metadata.channels = 1;
#  ifdef WITH_NANOVDB
    return false;  // Unsupported
#  endif
  }
  else {
    return false;
  }

#  ifdef WITH_NANOVDB
  metadata.byte_size = nanogrid.size();
  if (metadata.channels == 1) {
    metadata.type = IMAGE_DATA_TYPE_NANOVDB_FLOAT;
  }
  else {
    metadata.type = IMAGE_DATA_TYPE_NANOVDB_FLOAT3;
  }
#  else
  if (metadata.channels == 1) {
    metadata.type = IMAGE_DATA_TYPE_FLOAT;
  }
  else {
    metadata.type = IMAGE_DATA_TYPE_FLOAT4;
  }
#  endif

  /* Set transform from object space to voxel index. */
  openvdb::math::Mat4f grid_matrix = grid->transform().baseMap()->getAffineMap()->getMat4();
  Transform index_to_object;
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 3; row++) {
      index_to_object[row][col] = (float)grid_matrix[col][row];
    }
  }

#  ifdef WITH_NANOVDB
  Transform texture_to_index = transform_identity();
#  else
  openvdb::Coord min = bbox.min();
  Transform texture_to_index = transform_translate(min.x(), min.y(), min.z()) *
                               transform_scale(dim.x(), dim.y(), dim.z());
#  endif

  metadata.transform_3d = transform_inverse(index_to_object * texture_to_index);
  metadata.use_transform_3d = true;

  return true;
#else
  (void)metadata;
  return false;
#endif
}

bool VDBImageLoader::load_pixels(const ImageMetaData &, void *pixels, const size_t, const bool)
{
#ifdef WITH_OPENVDB
#  ifdef WITH_NANOVDB
  memcpy(pixels, nanogrid.data(), nanogrid.size());
#  else
  if (grid->isType<openvdb::FloatGrid>()) {
    openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, (float *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid), dense);
  }
  else if (grid->isType<openvdb::Vec3fGrid>()) {
    openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
        bbox, (openvdb::Vec3f *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3fGrid>(grid), dense);
  }
  else if (grid->isType<openvdb::BoolGrid>()) {
    openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, (float *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::BoolGrid>(grid), dense);
  }
  else if (grid->isType<openvdb::DoubleGrid>()) {
    openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, (float *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::DoubleGrid>(grid), dense);
  }
  else if (grid->isType<openvdb::Int32Grid>()) {
    openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, (float *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Int32Grid>(grid), dense);
  }
  else if (grid->isType<openvdb::Int64Grid>()) {
    openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, (float *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Int64Grid>(grid), dense);
  }
  else if (grid->isType<openvdb::Vec3IGrid>()) {
    openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
        bbox, (openvdb::Vec3f *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3IGrid>(grid), dense);
  }
  else if (grid->isType<openvdb::Vec3dGrid>()) {
    openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
        bbox, (openvdb::Vec3f *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3dGrid>(grid), dense);
  }
  else if (grid->isType<openvdb::MaskGrid>()) {
    openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, (float *)pixels);
    openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::MaskGrid>(grid), dense);
  }
#  endif
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
