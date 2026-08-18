/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_color.hh"
#include "BLI_math_base.hh"
#include "BLI_math_rotation.hh"
#include "BLI_string_utils.hh"

#include "BKE_attribute_math.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_type_traits.hh"
#include "BKE_volume_openvdb.hh"

#include "GEO_grid_samplers.hh"
#include "GEO_points_to_volume.hh"

#include <type_traits>

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/points/PointConversion.h>
#  include <openvdb/points/PointTransfer.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/Morphology.h>
#  include <openvdb/tools/ParticlesToLevelSet.h>
#  include <openvdb/tools/PointIndexGrid.h>
#endif

namespace blender::geometry {

const CPPType &points_rasterize_attribute_type(const PointRasterizeType rasterize_type)
{
  switch (rasterize_type) {
    case PointRasterizeType::Scalar:
      return CPPType::get<float>();
    case PointRasterizeType::Vector:
      return CPPType::get<float3>();
  }
  BLI_assert_unreachable();
  return CPPType::get<float>();
}

const CPPType &points_rasterize_grid_type(const PointRasterizeType rasterize_type)
{
  switch (rasterize_type) {
    case PointRasterizeType::Scalar:
      return CPPType::get<float>();
    case PointRasterizeType::Vector:
      return CPPType::get<float3>();
  }
  BLI_assert_unreachable();
  return CPPType::get<float>();
}

#ifdef WITH_OPENVDB

/* Implements the interface required by #openvdb::tools::ParticlesToLevelSet. */
class OpenVDBParticleList {
 public:
  using PosType = openvdb::Vec3R;

 private:
  Span<float3> positions_;
  Span<float> radii_;
  float voxel_size_inv_;

 public:
  OpenVDBParticleList(const Span<float3> positions,
                      const Span<float> radii,
                      const float voxel_size)
      : positions_(positions), radii_(radii), voxel_size_inv_(math::rcp(voxel_size))
  {
    BLI_assert(voxel_size > 0.0f);
  }

  size_t size() const
  {
    return size_t(positions_.size());
  }

  void getPos(size_t n, openvdb::Vec3R &xyz) const
  {
    const float3 pos = positions_[n] * voxel_size_inv_;
    xyz = &pos.x;
  }

  void getPosRad(size_t n, openvdb::Vec3R &xyz, openvdb::Real &radius) const
  {
    this->getPos(n, xyz);
    radius = radii_[n] * voxel_size_inv_;
  }
};

static openvdb::FloatGrid::Ptr points_to_sdf_grid_impl(const Span<float3> positions,
                                                       const Span<float> radii,
                                                       const float voxel_size)
{
  if (!BKE_volume_voxel_size_valid(float3(voxel_size))) {
    return nullptr;
  }

  /* Create a new grid that will be filled. #ParticlesToLevelSet requires
   * the background value to be positive */
  openvdb::FloatGrid::Ptr new_grid = openvdb::FloatGrid::create(1.0f);

  /* Create a narrow-band level set grid based on the positions and radii. */
  openvdb::tools::ParticlesToLevelSet op{*new_grid};
  /* Don't ignore particles based on their radius. */
  op.setRmin(0.0f);
  op.setRmax(std::numeric_limits<float>::max());
  OpenVDBParticleList particles{positions, radii, voxel_size};
  op.rasterizeSpheres(particles);
  op.finalize();

  new_grid->transform().postScale(voxel_size);
  new_grid->setGridClass(openvdb::GRID_LEVEL_SET);

  return new_grid;
}

bke::VolumeGrid<float> points_to_sdf_grid(const Span<float3> positions,
                                          const Span<float> radii,
                                          const float voxel_size)
{
  return bke::VolumeGrid<float>(points_to_sdf_grid_impl(positions, radii, voxel_size));
}

bke::VolumeGridData *fog_volume_grid_add_from_points(Volume *volume,
                                                     const StringRefNull name,
                                                     const Span<float3> positions,
                                                     const Span<float> radii,
                                                     const float voxel_size,
                                                     const float density)
{
  openvdb::FloatGrid::Ptr new_grid = points_to_sdf_grid_impl(positions, radii, voxel_size);
  new_grid->setGridClass(openvdb::GRID_FOG_VOLUME);

  /* Convert the level set to a fog volume. This also sets the background value to zero. Inside the
   * fog there will be a density of 1. */
  openvdb::tools::sdfToFogVolume(*new_grid);

  /* Take the desired density into account. */
  openvdb::tools::foreach(new_grid->beginValueOn(),
                          [&](const openvdb::FloatGrid::ValueOnIter &iter) {
                            iter.modifyValue([&](float &value) { value *= density; });
                          });

  return BKE_volume_grid_add_vdb(*volume, name, std::move(new_grid));
}

bool is_point_attribute_grid_supported(const CPPType &cpp_type)
{
  return ELEM(cpp_type,
              CPPType::get<bool>(),
              CPPType::get<float>(),
              CPPType::get<int>(),
              CPPType::get<int64_t>(),
              CPPType::get<float3>(),
              CPPType::get<int3>(),
              CPPType::get<float4x4>());
}

/* Remove characters that are invalid for OpenVDB attribute names. */
static std::string sanitize_name_for_openvdb(const StringRef name)
{
  /* Based on openvdb::points::AttributeSet::Descriptor::validName. */
  std::string result = name;
  result.erase(std::remove_if(result.begin(),
                              result.end(),
                              [](const char c) {
                                return !(isalnum(c) || (c == '_') || (c == '|') || (c == ':'));
                              }),
               result.end());
  return result;
}

/* Find a unique attribute name based on a generic string that may contain invalid characters. */
[[maybe_unused]] static std::string add_unique_vdb_attribute_name(
    const StringRef name, VectorSet<std::string> &used_names)
{
  const std::string vdb_base_name = sanitize_name_for_openvdb(name);

  std::string vdb_name = vdb_base_name;
  int duplicates = 0;
  while (!used_names.add(vdb_name)) {
    ++duplicates;
    vdb_name = vdb_base_name + "_" + std::to_string(duplicates);
  }
  return vdb_name;
}

static std::optional<StringRef> find_vdb_attribute_name(const PointAttributeNameMap &attribute_map,
                                                        const StringRef name)
{
  for (const std::pair<std::string, std::string> &name_pair : attribute_map) {
    if (name_pair.first == name) {
      return name_pair.second;
    }
  }
  return std::nullopt;
}

/* Helper class providing a point data interface to OpenVDB. */
template<typename T> class PointAttributeSpan {
 private:
  Span<T> data_;

 public:
  using type_traits = bke::VolumeGridTraits<T>;

  using value_type = typename type_traits::PrimitiveType;
  using PosType = value_type;

  PointAttributeSpan(const Span<T> data) : data_(data) {}

  size_t size() const
  {
    return data_.size();
  }
  void getPos(size_t n, PosType &xyz) const
  {
    xyz = type_traits::to_openvdb(data_[n]);
  }
  void get(value_type &value, size_t n) const
  {
    value = type_traits::to_openvdb(data_[n]);
  }
  void get(value_type &value, size_t n, openvdb::Index m) const
  {
    value = type_traits::to_openvdb(data_[n + m]);
  }
};

MappedPointDataGrid points_to_point_data_grid(const Span<float3> positions,
                                              const Span<PointDataGridAttributeInfo> attributes,
                                              const float4x4 &transform)
{
#  ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#  endif
  const PointAttributeSpan positions_wrapper(positions);
  /* Note: The createPointIndexGrid function is expecting a cell-centered transform while the input
   * transform is corner-centered! The internal PointPartitioner can be configured, but that
   * argument is not exposed. */
  const float4x4 transform_cell_centered = transform * math::from_location<float4x4>(float3(0.5f));
  const openvdb::math::Transform vdb_transform = BKE_volume_transform_to_openvdb(transform);
  const openvdb::math::Transform vdb_transform_cell_centered = BKE_volume_transform_to_openvdb(
      transform_cell_centered);

  /* Create point index grid in advance so it can be used for all attribute grids. */
  openvdb::tools::PointIndexGrid::Ptr point_index_grid =
      openvdb::tools::createPointIndexGrid<openvdb::tools::PointIndexGrid>(
          positions_wrapper, vdb_transform_cell_centered);

  /* Convert the main positions array.
   * Note: Use the corner-centered transform here so that position data in the points array is in
   * the regular 0..1 sampling space. */
  openvdb::points::PointDataGrid::Ptr point_data_grid =
      openvdb::points::createPointDataGrid<openvdb::points::NullCodec,
                                           openvdb::points::PointDataGrid>(
          *point_index_grid, positions_wrapper, vdb_transform);

  VectorSet<std::string> used_names;
  Vector<std::pair<std::string, std::string>> attribute_map;
  for (const PointDataGridAttributeInfo &info : attributes) {
    const CPPType &cpp_type = info.data.type();
    bke::attribute_math::to_static_type(cpp_type, [&]<typename ValueT>() {
      using type_traits = typename bke::VolumeGridTraits<ValueT>;

      if constexpr (!std::is_same_v<typename type_traits::PrimitiveType, void>) {
        /* Note: some attributes could benefit from specialized codecs. The OpenVDB cookbook
         * suggests to store e.g. radius attribute with a fixed-point codec. This is not supported
         * here yet.
         */
        // openvdb::points::FixedPointCodec</*1-byte=*/false, openvdb::points::UnitRange>;
        using Codec = openvdb::points::NullCodec;
        using AttributeArray =
            openvdb::points::TypedAttributeArray<typename type_traits::PrimitiveType, Codec>;
        if (!AttributeArray::isRegistered()) {
          AttributeArray::registerType();
        }

        const std::string vdb_name = add_unique_vdb_attribute_name(info.name, used_names);
        attribute_map.append_as(info.name, vdb_name);

        openvdb::NamePair point_data_attribute = AttributeArray::attributeType();
        openvdb::points::appendAttribute(point_data_grid->tree(), vdb_name, point_data_attribute);

        const PointAttributeSpan data_wrapper(info.data.typed<ValueT>());
        openvdb::points::populateAttribute(
            point_data_grid->tree(), point_index_grid->tree(), vdb_name, data_wrapper);
      }
    });
  }

  return {bke::GVolumeGrid(std::move(point_data_grid)), std::move(attribute_map)};
}

MappedPointDataGrid points_to_point_data_grid(const VArray<float3> positions,
                                              const bke::AttributeAccessor &attributes,
                                              const bke::AttributeFilter &attribute_filter,
                                              const float4x4 &transform)
{
  const VArraySpan<float3> positions_span = positions;

  Vector<GVArraySpan> attribute_arrays;
  Vector<PointDataGridAttributeInfo> attributes_info;
  MappedPointDataGrid result;
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const CPPType &cpp_type = bke::attribute_type_to_cpp_type(iter.data_type);
    if (!is_point_attribute_grid_supported(cpp_type)) {
      return;
    }
    const bke::GAttributeReader reader = iter.get(bke::AttrDomain::Point);
    if (!reader) {
      return;
    }
    attribute_arrays.append(*reader);
    attributes_info.append({iter.name, attribute_arrays.last()});

    result = points_to_point_data_grid(positions_span, attributes_info, transform);
  });

  return result;
}

namespace kernel_functions {

/* Range of a kernel function in voxels.
 *
 * TODO currently this expands the search box uniformly in both directions.
 * Some kernel functions (nearest point, quadratic) only affect an odd number of voxels.
 * A smaller search box could be used by sorting the point data grid with a half-voxel offset.
 * This is equivalent to the offset applied to sampling positions,
 * see geometry::grid_sampling::sample_tree.
 */
inline int kernel_size(const KernelType kernel_type)
{
  using namespace geometry::grid_sampling;

  switch (kernel_type) {
    case KernelType::NearestPoint:
      return std::max(NearestPointKernel::samples_left, NearestPointKernel::samples_right);
    case KernelType::Linear:
      return std::max(LinearKernel::samples_left, LinearKernel::samples_right);
    case KernelType::Quadratic:
      return std::max(QuadraticBSplineKernel::samples_left, QuadraticBSplineKernel::samples_right);
    case KernelType::Cubic:
      return std::max(CubicBSplineKernel::samples_left, CubicBSplineKernel::samples_right);
  }
  BLI_assert_unreachable();
  return 0;
}

/* Evaluate a kernel weight function in one dimension. */
inline float kernel_eval_component(const KernelType kernel_type, const float t)
{
  switch (kernel_type) {
    case KernelType::NearestPoint:
      return geometry::grid_sampling::NearestPointKernel::weight(t);
    case KernelType::Linear:
      return geometry::grid_sampling::LinearKernel::weight(t);
    case KernelType::Quadratic:
      return geometry::grid_sampling::QuadraticBSplineKernel::weight(t);
    case KernelType::Cubic:
      return geometry::grid_sampling::CubicBSplineKernel::weight(t);
  }
  return 0.0f;
}

/* Evaluate a kernel weight function in three dimensions. */
inline float kernel_eval(const KernelType kernel_type, const float3 &v)
{
  return kernel_eval_component(kernel_type, v.x) * kernel_eval_component(kernel_type, v.y) *
         kernel_eval_component(kernel_type, v.z);
}

}  // namespace kernel_functions

/**
 * Transfer class implementing point data accumulation on grids.
 * This is used with the openvdb::points::rasterize function.
 * It uses a kernel function with a known support interval, which determines the size of
 * the intersection box required.
 *
 * Points are first sorted into a `PointDataGrid`. A leaf of this grid contains all points located
 * inside the leaf node bounds. Each voxel in turn contains all points located inside the voxel.
 *
 * The point data grid has the same transform as the output grid. An output grid leaf can be mapped
 * directly to a point data grid leaf.
 *
 * The general procedure in openvdb::points::rasterize:
 *   For each target leaf of the output tree:
 *   1. Get the target bounding box (index space)
 *   2. Construct the search box by expanding the leaf bounds based on the kernel size.
 *   3. For each leaf node overlapping with the search box:
 *       - Find the matching source leaf in the point data grid.
 *       - For every voxel in the source leaf buffer:
 *           - Get the point range contained in the voxel.
 *           - Using the Transfer struct:
 *               - Compute overlap of the kernel range box around the point with the target leaf.
 *               - For each voxel within this overlap box calculate the kernel weight and add the
 *                 point value to that voxel.
 */

/* TODO use multifunction evaluation for efficient kernel eval.
 * Currently the kernel type is checked at runtime to determine the weights for each point.
 * This could be optimized by making the kernel type a template argument, but that increases code
 * generation. Using multi-function evaluation can avoid that but requires significant changes to
 * the Transfer class and possible rewrite of the rasterization grid operator itself. */

template<typename AttributeT, typename GridValueT>
struct KernelTransferBase : public openvdb::points::TransformTransfer,
                            public openvdb::points::VolumeTransfer<
                                typename bke::VolumeGridTraits<GridValueT>::TreeType> {
  using AttributeTraits = bke::VolumeGridTraits<AttributeT>;
  using AttributeType = typename AttributeTraits::PrimitiveType;

  using GridTraits = bke::VolumeGridTraits<GridValueT>;
  using TreeType = typename GridTraits::TreeType;
  using GridType = openvdb::Grid<TreeType>;
  using GridValueType = typename GridTraits::PrimitiveType;
  using NodeMaskType = typename TreeType::LeafNodeType::NodeMaskType;

  static const int32_t LOG2DIM = TreeType::LeafNodeType::LOG2DIM;
  static const int32_t DIM = TreeType::LeafNodeType::DIM;

  KernelType kernel_type_;
  int kernel_size_;

  /* Point attribute name for input values. */
  StringRef value_attribute_;
  /* Point attribute handle for positions in the current leaf. */
  std::optional<openvdb::points::AttributeHandle<openvdb::Vec3f>> position_handle_;
  /* Point attribute handle for input values in the current leaf. */
  std::optional<openvdb::points::AttributeHandle<AttributeType>> value_handle_;

  KernelTransferBase(const KernelType kernel_type,
                     const openvdb::points::PointDataGrid &source,
                     const StringRef value_attribute,
                     GridType &dest)
      : TransformTransfer(source.transform(), dest.transform()),
        openvdb::points::VolumeTransfer<TreeType>(dest.tree()),
        kernel_type_(kernel_type),
        kernel_size_(kernel_functions::kernel_size(kernel_type)),
        value_attribute_(value_attribute)
  {
  }

  KernelTransferBase(const KernelTransferBase &other)
      : TransformTransfer(other),
        openvdb::points::VolumeTransfer<TreeType>(other),
        kernel_type_(other.kernel_type_),
        kernel_size_(other.kernel_size_),
        value_attribute_(other.value_attribute_)
  {
  }

  KernelType kernel_type() const
  {
    return kernel_type_;
  }

  /* Search range for point voxels around the target voxel. */
  openvdb::Int32 range(const openvdb::Coord & /*leaf_origin*/, size_t /*leaf_idx*/) const
  {
    return (kernel_size_ + 1) >> 1;
  }

  AttributeType get_value(const openvdb::Index point_index)
  {
    return value_handle_->get(point_index);
  }

  bool startPointLeaf(const openvdb::points::PointDataTree::LeafNodeType &leaf)
  {
    position_handle_.emplace(
        openvdb::points::AttributeHandle<openvdb::Vec3f>(leaf.constAttributeArray("P")));

    BLI_assert(leaf.hasAttribute(value_attribute_));
    value_handle_.emplace(openvdb::points::AttributeHandle<AttributeType>(
        leaf.constAttributeArray(value_attribute_)));
    return true;
  }

  /**
   * For each point, compute its relative index space position in the destination tree and
   * sum a function of per-point values.
   *
   * \param ijk Point voxel coordinate.
   * \param point_index_range Range of points inside the point voxel bounds.
   * \param target_bounds Coordinate region of the destination tree to add into.
   */
  template<typename ValueFn>
  void add_points_to_voxels(const openvdb::Coord &ijk,
                            const IndexRange point_index_range,
                            const openvdb::CoordBBox &target_bounds,
                            ValueFn value_fn)
  {
    const int max_offset = ((kernel_size_ + 1) >> 1);
    openvdb::CoordBBox intersect_box(ijk.offsetBy(-max_offset + 1), ijk.offsetBy(max_offset));
    intersect_box.intersect(target_bounds);
    if (intersect_box.empty()) {
      return;
    }

    auto *const data = this->template buffer<0>();
    const auto &mask = *(this->template mask<0>());

    for (const openvdb::Index point_index : point_index_range) {
      const openvdb::Vec3d source_position = ijk.asVec3d() +
                                             this->position_handle_->get(point_index);
      const openvdb::Vec3d target_position = this->transformSourceToTarget(source_position);

      const openvdb::Coord &a(intersect_box.min());
      const openvdb::Coord &b(intersect_box.max());
      for (openvdb::Coord c = a; c.x() <= b.x(); ++c.x()) {
        const openvdb::Index i = ((c.x() & (DIM - 1u)) << 2 * LOG2DIM);
        for (c.y() = a.y(); c.y() <= b.y(); ++c.y()) {
          const openvdb::Index j = ((c.y() & (DIM - 1u)) << LOG2DIM);
          for (c.z() = a.z(); c.z() <= b.z(); ++c.z()) {
            BLI_assert(target_bounds.isInside(c));
            const openvdb::Index offset = i + j + /*k*/ (c.z() & (DIM - 1u));
            if (!mask.isOn(offset)) {
              continue;
            }

            const float3 kernel_distance = float3(
                openvdb::Vec3f(c.asVec3d() - target_position).asV());
            data[offset] += value_fn(point_index, kernel_distance);
          }
        }
      }
    }
  }

  bool endPointLeaf(const openvdb::points::PointDataTree::LeafNodeType & /*leaf_node*/)
  {
    /* If endPointLeaf returns false for the given point leaf node
     * then finalize is called and rasterization may be repeated for that point leaf. */
    return true;
  }

  bool finalize(const openvdb::Coord & /*origin*/, size_t /*idx*/)
  {
    /* If finalize returns false for the given leaf origin
     * then the rasterization is repeated for that leaf. */
    return true;
  }
};

template<typename AttributeT, typename GridValueT>
struct ValueTransfer : public KernelTransferBase<AttributeT, GridValueT> {
  using Base = KernelTransferBase<AttributeT, GridValueT>;
  using AttributeType = typename Base::AttributeType;

  using KernelTransferBase<AttributeT, GridValueT>::KernelTransferBase;

  void rasterizePoints(const openvdb::Coord &ijk,
                       const openvdb::Index point_index_begin,
                       const openvdb::Index point_index_end,
                       const openvdb::CoordBBox &target_bounds)
  {
    this->add_points_to_voxels(
        ijk,
        IndexRange::from_begin_end(point_index_begin, point_index_end),
        target_bounds,
        [&](const openvdb::Index point_index, const float3 &kernel_distance) {
          const AttributeType source_value = this->get_value(point_index);
          const float weight = kernel_functions::kernel_eval(this->kernel_type(), kernel_distance);
          return weight * source_value;
        });
  }
};

/* Construct a destination grid for a point data attribute.
 * The grid transform is determined by the pre-constructed point data grid and the topology is
 * dilated by the kernel size to ensure all point values are fully rasterized. */
template<typename GridType>
static typename GridType::Ptr prepare_destination_grid(
    const openvdb::points::PointDataGrid &point_data_grid,
    const float4x4 &transform,
    const KernelType kernel_type)
{
  typename std::shared_ptr<GridType> dst_grid = GridType::create();
  dst_grid->transform() = BKE_volume_transform_to_openvdb(transform);
  {
    /* Activate all voxels with particles in them. */
#  ifdef DEBUG_TIME
    SCOPED_TIMER("      topologyUnion");
#  endif
    dst_grid->tree().topologyUnion(point_data_grid.tree());
  }
  {
#  ifdef DEBUG_TIME
    SCOPED_TIMER("      dilateActiveValues");
#  endif
    /* Dilate to ensure all voxels within range of a particle are active. */
    const int max_offset = (kernel_functions::kernel_size(kernel_type) + 1) >> 1;
    openvdb::tools::dilateActiveValues(dst_grid->tree(),
                                       max_offset,
                                       openvdb::tools::NN_FACE_EDGE_VERTEX,
                                       openvdb::tools::TilePolicy::PRESERVE_TILES,
                                       false);
  }
  {
#  ifdef DEBUG_TIME
    SCOPED_TIMER("      voxelizeActiveTiles");
#  endif
    /* Voxelize all tiles since each voxel gets a different value. */
    dst_grid->tree().voxelizeActiveTiles(true);
  }
  return dst_grid;
}

/* Rasterize a point attribute with a known static destination grid type. */
template<typename AttributeT, typename GridValueT, typename TransferT>
static bke::GVolumeGrid points_rasterize_with_static_type(
    const openvdb::points::PointDataGrid &point_data_grid,
    const StringRef value_attribute,
    const PointRasterizeAttributeInfo & /*attribute_info*/,
    const float4x4 &transform,
    const KernelType kernel_type)
{
  using GridTraits = bke::VolumeGridTraits<GridValueT>;
  using TreeType = typename GridTraits::TreeType;
  using GridType = openvdb::Grid<TreeType>;

  typename std::shared_ptr<GridType> dst_grid;

  {
#  ifdef DEBUG_TIME
    SCOPED_TIMER("    prepare_destination_grid");
#  endif
    dst_grid = prepare_destination_grid<GridType>(point_data_grid, transform, kernel_type);
  }

  {
#  ifdef DEBUG_TIME
    SCOPED_TIMER("    rasterize");
#  endif
    BLI_assert(!value_attribute.is_empty());
    TransferT transfer(kernel_type, point_data_grid, value_attribute, *dst_grid);
    openvdb::points::rasterize(point_data_grid, transfer);
  }

  {
#  ifdef DEBUG_TIME
    SCOPED_TIMER("    finalize_grid");
#  endif
    dst_grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
  }

  if (dst_grid) {
    return bke::GVolumeGrid(std::move(dst_grid));
  }
  return {};
}

static bke::GVolumeGrid points_attribute_rasterize(
    const openvdb::points::PointDataGrid &point_data_grid,
    const KernelType kernel_type,
    const StringRef value_attribute,
    const PointRasterizeAttributeInfo &attribute_info,
    const float4x4 &transform)
{
  bke::GVolumeGrid result;
  /* Dispatch to the correct static grid type function. */
  switch (attribute_info.type) {
    case PointRasterizeType::Scalar:
      result = points_rasterize_with_static_type<float, float, ValueTransfer<float, float>>(
          point_data_grid, value_attribute, attribute_info, transform, kernel_type);
      break;
    case PointRasterizeType::Vector:
      result = points_rasterize_with_static_type<float3, float3, ValueTransfer<float3, float3>>(
          point_data_grid, value_attribute, attribute_info, transform, kernel_type);
      break;
  }

  return result;
}

void points_rasterize(const MappedPointDataGrid &point_data_grid,
                      const KernelType kernel_type,
                      Span<PointRasterizeAttributeInfo> point_attributes,
                      const float4x4 &transform,
                      MutableSpan<bke::GVolumeGrid> r_attribute_grids)
{
#  ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#  endif

  BLI_assert(r_attribute_grids.size() == point_attributes.size());

  if (!point_data_grid.grid || point_data_grid.grid->grid_type() != VOLUME_GRID_POINTS) {
    return;
  }
  bke::VolumeTreeAccessToken access_token;
  const openvdb::points::PointDataGrid::ConstPtr vdb_point_data_grid =
      openvdb::GridBase::grid<openvdb::points::PointDataGrid>(
          point_data_grid.grid->grid_ptr(access_token));
  BLI_assert(vdb_point_data_grid);

  for (const int i : point_attributes.index_range()) {
    const PointRasterizeAttributeInfo &attribute = point_attributes[i];
#  ifdef DEBUG_TIME
    SCOPED_TIMER("  attribute: " + attribute.name);
#  endif

    const std::optional<std::string> vdb_attribute_name = find_vdb_attribute_name(
        point_data_grid.attribute_map, attribute.name);
    BLI_assert(vdb_attribute_name.has_value());
    r_attribute_grids[i] = points_attribute_rasterize(
        *vdb_point_data_grid, kernel_type, *vdb_attribute_name, attribute, transform);
  }
}

#endif

}  // namespace blender::geometry
