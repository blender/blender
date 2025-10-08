/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_process.hh"
#include "BKE_volume_openvdb.hh"

#include "BLI_index_mask.hh"
#include "BLI_memory_counter.hh"
#include "BLI_task.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/Grid.h>
#  include <openvdb/tools/Prune.h>
#endif

namespace blender::bke::volume_grid {

#ifdef WITH_OPENVDB

VolumeGridData::VolumeGridData()
{
  tree_access_token_ = std::make_shared<AccessToken>(*this);
}

struct CreateGridOp {
  template<typename GridT> openvdb::GridBase::Ptr operator()() const
  {
    return GridT::create();
  }
};

static openvdb::GridBase::Ptr create_grid_for_type(const VolumeGridType grid_type)
{
  return BKE_volume_grid_type_operation(grid_type, CreateGridOp{});
}

VolumeGridData::VolumeGridData(const VolumeGridType grid_type)
    : VolumeGridData(create_grid_for_type(grid_type))
{
}

VolumeGridData::VolumeGridData(std::shared_ptr<openvdb::GridBase> grid)
    : grid_(std::move(grid)), tree_loaded_(true), transform_loaded_(true), meta_data_loaded_(true)
{
  BLI_assert(grid_);
  BLI_assert(grid_.use_count() == 1);
  BLI_assert(grid_->isTreeUnique());

  tree_sharing_info_ = OpenvdbTreeSharingInfo::make(grid_->baseTreePtr());
  tree_access_token_ = std::make_shared<AccessToken>(*this);
}

VolumeGridData::VolumeGridData(std::function<LazyLoadedGrid()> lazy_load_grid,
                               std::shared_ptr<openvdb::GridBase> meta_data_and_transform_grid)
    : grid_(std::move(meta_data_and_transform_grid)), lazy_load_grid_(std::move(lazy_load_grid))
{
  if (grid_) {
    transform_loaded_ = true;
    meta_data_loaded_ = true;
  }
  tree_access_token_ = std::make_shared<AccessToken>(*this);
}

VolumeGridData::~VolumeGridData() = default;

void VolumeGridData::delete_self()
{
  MEM_delete(this);
}

const openvdb::GridBase &VolumeGridData::grid(VolumeTreeAccessToken &r_token) const
{
  return *this->grid_ptr(r_token);
}

openvdb::GridBase &VolumeGridData::grid_for_write(VolumeTreeAccessToken &r_token)
{
  return *this->grid_ptr_for_write(r_token);
}

std::shared_ptr<const openvdb::GridBase> VolumeGridData::grid_ptr(
    VolumeTreeAccessToken &r_token) const
{
  std::lock_guard lock{mutex_};
  this->ensure_grid_loaded();
  r_token.token_ = tree_access_token_;
  return grid_;
}

std::shared_ptr<openvdb::GridBase> VolumeGridData::grid_ptr_for_write(
    VolumeTreeAccessToken &r_token)
{
  BLI_assert(this->is_mutable());
  std::lock_guard lock{mutex_};
  this->ensure_grid_loaded();
  r_token.token_ = tree_access_token_;
  if (tree_sharing_info_->is_mutable()) {
    tree_sharing_info_->tag_ensured_mutable();
  }
  else {
    auto tree_copy = grid_->baseTree().copy();
    grid_->setTree(tree_copy);
    tree_sharing_info_ = OpenvdbTreeSharingInfo::make(std::move(tree_copy));
  }
  /* Can't reload the grid anymore if it has been changed. */
  lazy_load_grid_ = {};
  return grid_;
}

const openvdb::math::Transform &VolumeGridData::transform() const
{
  std::lock_guard lock{mutex_};
  if (!transform_loaded_) {
    this->ensure_grid_loaded();
  }
  return grid_->transform();
}

openvdb::math::Transform &VolumeGridData::transform_for_write()
{
  BLI_assert(this->is_mutable());
  std::lock_guard lock{mutex_};
  if (!transform_loaded_) {
    this->ensure_grid_loaded();
  }
  return grid_->transform();
}

std::string VolumeGridData::name() const
{
  std::lock_guard lock{mutex_};
  if (!meta_data_loaded_) {
    this->ensure_grid_loaded();
  }
  return grid_->getName();
}

void VolumeGridData::set_name(const StringRef name)
{
  BLI_assert(this->is_mutable());
  std::lock_guard lock{mutex_};
  if (!meta_data_loaded_) {
    this->ensure_grid_loaded();
  }
  grid_->setName(name);
}

VolumeGridType VolumeGridData::grid_type() const
{
  std::lock_guard lock{mutex_};
  if (!meta_data_loaded_) {
    this->ensure_grid_loaded();
  }
  return get_type(*grid_);
}

std::optional<VolumeGridType> VolumeGridData::grid_type_without_load() const
{
  std::lock_guard lock{mutex_};
  if (!meta_data_loaded_) {
    return std::nullopt;
  }
  return get_type(*grid_);
}

openvdb::GridClass VolumeGridData::grid_class() const
{
  std::lock_guard lock{mutex_};
  if (!meta_data_loaded_) {
    this->ensure_grid_loaded();
  }
  return grid_->getGridClass();
}

bool VolumeGridData::is_reloadable() const
{
  return bool(lazy_load_grid_);
}

void VolumeGridData::tag_tree_modified() const
{
  active_voxels_mutex_.tag_dirty();
  active_leaf_voxels_mutex_.tag_dirty();
  active_tiles_mutex_.tag_dirty();
  size_in_bytes_mutex_.tag_dirty();
  active_bounds_mutex_.tag_dirty();
}

bool VolumeGridData::is_loaded() const
{
  std::lock_guard lock{mutex_};
  return tree_loaded_ && transform_loaded_ && meta_data_loaded_;
}

void VolumeGridData::count_memory(MemoryCounter &memory) const
{
  std::lock_guard lock{mutex_};
  if (!tree_loaded_) {
    return;
  }
  memory.add_shared(tree_sharing_info_.get(), [&](MemoryCounter &shared_memory) {
    shared_memory.add(this->size_in_bytes());
  });
}

int64_t VolumeGridData::active_voxels() const
{
  active_voxels_mutex_.ensure([&]() {
    VolumeTreeAccessToken token;
    const openvdb::GridBase &grid = this->grid(token);
    const openvdb::TreeBase &tree = grid.baseTree();
    active_voxels_ = tree.activeVoxelCount();
  });
  return active_voxels_;
}

int64_t VolumeGridData::active_leaf_voxels() const
{
  active_leaf_voxels_mutex_.ensure([&]() {
    VolumeTreeAccessToken token;
    const openvdb::GridBase &grid = this->grid(token);
    const openvdb::TreeBase &tree = grid.baseTree();
    active_leaf_voxels_ = tree.activeLeafVoxelCount();
  });
  return active_leaf_voxels_;
}

int64_t VolumeGridData::active_tiles() const
{
  active_tiles_mutex_.ensure([&]() {
    VolumeTreeAccessToken token;
    const openvdb::GridBase &grid = this->grid(token);
    const openvdb::TreeBase &tree = grid.baseTree();
    active_tiles_ = tree.activeTileCount();
  });
  return active_tiles_;
}

int64_t VolumeGridData::size_in_bytes() const
{
  size_in_bytes_mutex_.ensure([&]() {
    VolumeTreeAccessToken token;
    const openvdb::GridBase &grid = this->grid(token);
    const openvdb::TreeBase &tree = grid.baseTree();
    size_in_bytes_ = tree.memUsage();
  });
  return size_in_bytes_;
}

const openvdb::CoordBBox &VolumeGridData::active_bounds() const
{
  active_bounds_mutex_.ensure([&]() {
    VolumeTreeAccessToken token;
    const openvdb::GridBase &grid = this->grid(token);
    const openvdb::TreeBase &tree = grid.baseTree();
    tree.evalActiveVoxelBoundingBox(active_bounds_);
  });
  return active_bounds_;
}

std::string VolumeGridData::error_message() const
{
  std::lock_guard lock{mutex_};
  return error_message_;
}

void VolumeGridData::unload_tree_if_possible() const
{
  std::lock_guard lock{mutex_};
  if (!grid_) {
    return;
  }
  if (!tree_loaded_) {
    return;
  }
  if (!this->is_reloadable()) {
    return;
  }
  if (tree_access_token_.use_count() != 1) {
    /* Some code is using the tree currently, so it can't be freed. */
    return;
  }
  grid_->newTree();
  tree_loaded_ = false;
  tree_sharing_info_.reset();
}

GVolumeGrid VolumeGridData::copy() const
{
  std::lock_guard lock{mutex_};
  this->ensure_grid_loaded();
  /* Can't use #MEM_new because the default constructor is private. */
  VolumeGridData *new_copy = new (MEM_mallocN(sizeof(VolumeGridData), __func__)) VolumeGridData();
  /* Makes a deep copy of the meta-data but shares the tree. */
  new_copy->grid_ = grid_->copyGrid();
  new_copy->tree_sharing_info_ = tree_sharing_info_;
  new_copy->tree_loaded_ = tree_loaded_;
  new_copy->transform_loaded_ = transform_loaded_;
  new_copy->meta_data_loaded_ = meta_data_loaded_;
  return GVolumeGrid(new_copy);
}

void VolumeGridData::ensure_grid_loaded() const
{
  /* Assert that the mutex is locked. */
  BLI_assert(!mutex_.try_lock());

  if (tree_loaded_ && transform_loaded_ && meta_data_loaded_) {
    return;
  }
  BLI_assert(lazy_load_grid_);
  LazyLoadedGrid loaded_grid;
  /* Isolate because the a mutex is locked. */
  threading::isolate_task([&]() {
    error_message_.clear();
    try {
      loaded_grid = lazy_load_grid_();
    }
    catch (const openvdb::IoError &e) {
      error_message_ = e.what();
    }
    catch (...) {
      error_message_ = "Unknown error reading VDB file";
    }
  });
  if (!loaded_grid.grid) {
    BLI_assert(!loaded_grid.tree_sharing_info);
    if (grid_) {
      const openvdb::Name &grid_type = grid_->type();
      if (openvdb::GridBase::isRegistered(grid_type)) {
        /* Create a dummy grid of the expected type. */
        loaded_grid.grid = openvdb::GridBase::createGrid(grid_type);
      }
    }
  }
  if (!loaded_grid.grid) {
    /* Create a dummy grid. We can't really know the expected data type here. */
    loaded_grid.grid = openvdb::FloatGrid::create();
  }
  BLI_assert(loaded_grid.grid);
  BLI_assert(loaded_grid.grid.use_count() == 1);

  if (!loaded_grid.tree_sharing_info) {
    BLI_assert(loaded_grid.grid->isTreeUnique());
    loaded_grid.tree_sharing_info = OpenvdbTreeSharingInfo::make(loaded_grid.grid->baseTreePtr());
  }

  if (grid_) {
    /* Keep the existing grid pointer and just insert the newly loaded data. */
    BLI_assert(!tree_loaded_);
    BLI_assert(meta_data_loaded_);
    grid_->setTree(loaded_grid.grid->baseTreePtr());
    if (!transform_loaded_) {
      grid_->setTransform(loaded_grid.grid->transformPtr());
    }
  }
  else {
    grid_ = std::move(loaded_grid.grid);
  }

  BLI_assert(!tree_sharing_info_);
  BLI_assert(loaded_grid.tree_sharing_info);
  tree_sharing_info_ = std::move(loaded_grid.tree_sharing_info);

  tree_loaded_ = true;
  transform_loaded_ = true;
  meta_data_loaded_ = true;
}

GVolumeGrid::GVolumeGrid(std::shared_ptr<openvdb::GridBase> grid)
{
  data_ = ImplicitSharingPtr(MEM_new<VolumeGridData>(__func__, std::move(grid)));
}

GVolumeGrid::GVolumeGrid(const VolumeGridType grid_type)
    : GVolumeGrid(create_grid_for_type(grid_type))
{
}

VolumeGridData &GVolumeGrid::get_for_write()
{
  BLI_assert(*this);
  if (data_->is_mutable()) {
    data_->tag_ensured_mutable();
  }
  else {
    *this = data_->copy();
  }
  return const_cast<VolumeGridData &>(*data_);
}

VolumeGridType get_type(const openvdb::TreeBase &tree)
{
  if (tree.isType<openvdb::FloatTree>()) {
    return VOLUME_GRID_FLOAT;
  }
  if (tree.isType<openvdb::Vec3fTree>()) {
    return VOLUME_GRID_VECTOR_FLOAT;
  }
  if (tree.isType<openvdb::BoolTree>()) {
    return VOLUME_GRID_BOOLEAN;
  }
  if (tree.isType<openvdb::DoubleTree>()) {
    return VOLUME_GRID_DOUBLE;
  }
  if (tree.isType<openvdb::Int32Tree>()) {
    return VOLUME_GRID_INT;
  }
  if (tree.isType<openvdb::Int64Tree>()) {
    return VOLUME_GRID_INT64;
  }
  if (tree.isType<openvdb::Vec3ITree>()) {
    return VOLUME_GRID_VECTOR_INT;
  }
  if (tree.isType<openvdb::Vec3dTree>()) {
    return VOLUME_GRID_VECTOR_DOUBLE;
  }
  if (tree.isType<openvdb::MaskTree>()) {
    return VOLUME_GRID_MASK;
  }
  if (tree.isType<openvdb::points::PointDataTree>()) {
    return VOLUME_GRID_POINTS;
  }
  return VOLUME_GRID_UNKNOWN;
}

VolumeGridType get_type(const openvdb::GridBase &grid)
{
  return get_type(grid.baseTree());
}

ImplicitSharingPtr<> OpenvdbTreeSharingInfo::make(std::shared_ptr<openvdb::tree::TreeBase> tree)
{
  return ImplicitSharingPtr<>{MEM_new<OpenvdbTreeSharingInfo>(__func__, std::move(tree))};
}

OpenvdbTreeSharingInfo::OpenvdbTreeSharingInfo(std::shared_ptr<openvdb::tree::TreeBase> tree)
    : tree_(std::move(tree))
{
}

void OpenvdbTreeSharingInfo::delete_self_with_data()
{
  MEM_delete(this);
}

void OpenvdbTreeSharingInfo::delete_data_only()
{
  tree_.reset();
}

VolumeTreeAccessToken::~VolumeTreeAccessToken()
{
  const VolumeGridData *grid = token_ ? &token_->grid : nullptr;
  token_.reset();
  if (grid) {
    /* Unload immediately when the value is not used anymore. However, the tree may still be cached
     * at a deeper level and thus usually does not have to be loaded from disk again. */
    grid->unload_tree_if_possible();
  }
}

#endif /* WITH_OPENVDB */

std::string get_name(const VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  return grid.name();
#else
  UNUSED_VARS(grid);
  return "density";
#endif
}

VolumeGridType get_type(const VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  return grid.grid_type();
#else
  UNUSED_VARS(grid);
  return VOLUME_GRID_UNKNOWN;
#endif
}

int get_channels_num(const VolumeGridType type)
{
  switch (type) {
    case VOLUME_GRID_BOOLEAN:
    case VOLUME_GRID_FLOAT:
    case VOLUME_GRID_DOUBLE:
    case VOLUME_GRID_INT:
    case VOLUME_GRID_INT64:
    case VOLUME_GRID_MASK:
      return 1;
    case VOLUME_GRID_VECTOR_FLOAT:
    case VOLUME_GRID_VECTOR_DOUBLE:
    case VOLUME_GRID_VECTOR_INT:
      return 3;
    case VOLUME_GRID_POINTS:
    case VOLUME_GRID_UNKNOWN:
      return 0;
  }
  return 0;
}

float4x4 get_transform_matrix(const VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  return BKE_volume_transform_to_blender(grid.transform());
#else
  UNUSED_VARS(grid);
  return float4x4::identity();
#endif
}

void set_transform_matrix(VolumeGridData &grid, const float4x4 &matrix)
{
#ifdef WITH_OPENVDB
  grid.transform_for_write() = BKE_volume_transform_to_openvdb(matrix);
#else
  UNUSED_VARS(grid, matrix);
#endif
}

void clear_tree(VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  VolumeTreeAccessToken tree_token;
  grid.grid_for_write(tree_token).clear();
  grid.tag_tree_modified();
#else
  UNUSED_VARS(grid);
#endif
}

bool is_loaded(const VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  return grid.is_loaded();
#else
  UNUSED_VARS(grid);
  return false;
#endif
}

void count_memory(const VolumeGridData &grid, MemoryCounter &memory)
{
#ifdef WITH_OPENVDB
  grid.count_memory(memory);
#else
  UNUSED_VARS(grid, memory);
#endif
}

void load(const VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  VolumeTreeAccessToken tree_token;
  /* Just "touch" the grid, so that it is loaded. */
  grid.grid(tree_token);
#else
  UNUSED_VARS(grid);
#endif
}

std::string error_message_from_load(const VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  return grid.error_message();
#else
  UNUSED_VARS(grid);
  return "";
#endif
}

#ifdef WITH_OPENVDB

/**
 * Call #process_leaf_fn on the leaf node if it has a certain minimum number of active voxels. If
 * there are only a few active voxels, gather those in #r_coords for later batch processing.
 */
template<typename LeafNodeT>
static void parallel_grid_topology_tasks_leaf_node(const LeafNodeT &node,
                                                   const ProcessLeafFn process_leaf_fn,
                                                   Vector<openvdb::Coord, 1024> &r_coords)
{
  using NodeMaskT = typename LeafNodeT::NodeMaskType;

  const int on_count = node.onVoxelCount();
  /* This number is somewhat arbitrary. 64 is a 1/8th of the number of voxels in a standard leaf
   * which is 8x8x8. It's a trade-off between benefiting from the better performance of
   * leaf-processing vs. processing more voxels in a batch. */
  const int on_count_threshold = 64;
  if (on_count <= on_count_threshold) {
    /* The leaf contains only a few active voxels. It's beneficial to process them in a batch with
     * active voxels from other leafs. So only gather them here for later processing. */
    for (auto value_iter = node.cbeginValueOn(); value_iter.test(); ++value_iter) {
      const openvdb::Coord coord = value_iter.getCoord();
      r_coords.append(coord);
    }
    return;
  }
  /* Process entire leaf at once. This is especially beneficial when very many of the voxels in
   * the leaf are active. In that case, one can work on the openvdb arrays stored in the leafs
   * directly. */
  const NodeMaskT &value_mask = node.getValueMask();
  const openvdb::CoordBBox bbox = node.getNodeBoundingBox();
  process_leaf_fn(value_mask, bbox, [&](MutableSpan<openvdb::Coord> r_voxels) {
    for (auto value_iter = node.cbeginValueOn(); value_iter.test(); ++value_iter) {
      r_voxels[value_iter.pos()] = value_iter.getCoord();
    }
  });
}

/**
 * Calls the process functions on all the active tiles and voxels within the given internal node.
 */
template<typename InternalNodeT>
static void parallel_grid_topology_tasks_internal_node(const InternalNodeT &node,
                                                       const ProcessLeafFn process_leaf_fn,
                                                       const ProcessVoxelsFn process_voxels_fn,
                                                       const ProcessTilesFn process_tiles_fn)
{
  using ChildNodeT = typename InternalNodeT::ChildNodeType;
  using LeafNodeT = typename InternalNodeT::LeafNodeType;
  using NodeMaskT = typename InternalNodeT::NodeMaskType;
  using UnionT = typename InternalNodeT::UnionType;

  /* Gather the active sub-nodes first, to be able to parallelize over them more easily. */
  const NodeMaskT &child_mask = node.getChildMask();
  const UnionT *table = node.getTable();
  Vector<int, 512> child_indices;
  for (auto child_mask_iter = child_mask.beginOn(); child_mask_iter.test(); ++child_mask_iter) {
    child_indices.append(child_mask_iter.pos());
  }

  threading::parallel_for(child_indices.index_range(), 8, [&](const IndexRange range) {
    /* Voxels collected from potentially multiple leaf nodes to be processed in one batch. This
     * inline buffer size is sufficient to avoid an allocation in all cases (a single standard leaf
     * has 512 voxels). */
    Vector<openvdb::Coord, 1024> gathered_voxels;
    for (const int child_index : child_indices.as_span().slice(range)) {
      const ChildNodeT &child = *table[child_index].getChild();
      if constexpr (std::is_same_v<ChildNodeT, LeafNodeT>) {
        parallel_grid_topology_tasks_leaf_node(child, process_leaf_fn, gathered_voxels);
        /* If enough voxels have been gathered, process them in one batch. */
        if (gathered_voxels.size() >= 512) {
          process_voxels_fn(gathered_voxels);
          gathered_voxels.clear();
        }
      }
      else {
        /* Recurse into lower-level internal nodes. */
        parallel_grid_topology_tasks_internal_node(
            child, process_leaf_fn, process_voxels_fn, process_tiles_fn);
      }
    }
    /* Process any remaining voxels. */
    if (!gathered_voxels.is_empty()) {
      process_voxels_fn(gathered_voxels);
      gathered_voxels.clear();
    }
  });

  /* Process the active tiles within the internal node. Note that these are not processed above
   * already because there only sub-nodes are handled, but tiles are "inlined" into internal nodes.
   * All tiles are first gathered and then processed in one batch. */
  const NodeMaskT &value_mask = node.getValueMask();
  Vector<openvdb::CoordBBox> tile_bboxes;
  for (auto value_mask_iter = value_mask.beginOn(); value_mask_iter.test(); ++value_mask_iter) {
    const openvdb::Index32 index = value_mask_iter.pos();
    const openvdb::Coord tile_origin = node.offsetToGlobalCoord(index);
    const openvdb::CoordBBox tile_bbox = openvdb::CoordBBox::createCube(tile_origin,
                                                                        ChildNodeT::DIM);
    tile_bboxes.append(tile_bbox);
  }
  if (!tile_bboxes.is_empty()) {
    process_tiles_fn(tile_bboxes);
  }
}

/* Call the process functions on all active tiles and voxels in the given tree. */
void parallel_grid_topology_tasks(const openvdb::MaskTree &mask_tree,
                                  const ProcessLeafFn process_leaf_fn,
                                  const ProcessVoxelsFn process_voxels_fn,
                                  const ProcessTilesFn process_tiles_fn)
{
  /* Iterate over the root internal nodes. */
  for (auto root_child_iter = mask_tree.cbeginRootChildren(); root_child_iter.test();
       ++root_child_iter)
  {
    const auto &internal_node = *root_child_iter;
    parallel_grid_topology_tasks_internal_node(
        internal_node, process_leaf_fn, process_voxels_fn, process_tiles_fn);
  }
}

openvdb::GridBase::Ptr create_grid_with_topology(const openvdb::MaskTree &topology,
                                                 const openvdb::math::Transform &transform,
                                                 const VolumeGridType grid_type)
{
  openvdb::GridBase::Ptr grid;
  BKE_volume_grid_type_to_static_type(grid_type, [&](auto type_tag) {
    using GridT = typename decltype(type_tag)::type;
    using TreeT = typename GridT::TreeType;
    using ValueType = typename TreeT::ValueType;
    const ValueType background{};
    auto tree = std::make_shared<TreeT>(topology, background, openvdb::TopologyCopy());
    grid = openvdb::createGrid(std::move(tree));
    grid->setTransform(transform.copy());
  });
  return grid;
}

void set_grid_values(openvdb::GridBase &grid_base,
                     const GSpan values,
                     const Span<openvdb::Coord> voxels)
{
  BLI_assert(values.size() == voxels.size());
  to_typed_grid(grid_base, [&](auto &grid) {
    using GridT = std::decay_t<decltype(grid)>;
    using ValueType = typename GridT::ValueType;
    const ValueType *data = static_cast<const ValueType *>(values.data());

    auto accessor = grid.getUnsafeAccessor();
    for (const int64_t i : voxels.index_range()) {
      accessor.setValue(voxels[i], data[i]);
    }
  });
}

void set_tile_values(openvdb::GridBase &grid_base,
                     const GSpan values,
                     const Span<openvdb::CoordBBox> tiles)
{
  BLI_assert(values.size() == tiles.size());
  to_typed_grid(grid_base, [&](auto &grid) {
    using GridT = typename std::decay_t<decltype(grid)>;
    using TreeT = typename GridT::TreeType;
    using ValueType = typename GridT::ValueType;
    auto &tree = grid.tree();

    const ValueType *computed_values = static_cast<const ValueType *>(values.data());

    const auto set_tile_value = [&](auto &node, const openvdb::Coord &coord_in_tile, auto value) {
      const openvdb::Index n = node.coordToOffset(coord_in_tile);
      BLI_assert(node.isChildMaskOff(n));
      /* TODO: Figure out how to do this without const_cast, although the same is done in
       * `openvdb_ax/openvdb_ax/compiler/VolumeExecutable.cc` which has a similar purpose.
       * It seems like OpenVDB generally allows that, but it does not have a proper public
       * API for this yet. */
      using UnionType = typename std::decay_t<decltype(node)>::UnionType;
      auto *table = const_cast<UnionType *>(node.getTable());
      table[n].setValue(value);
    };

    for (const int i : tiles.index_range()) {
      const openvdb::CoordBBox tile = tiles[i];
      const openvdb::Coord coord_in_tile = tile.min();
      const auto &computed_value = computed_values[i];
      using InternalNode1 = typename TreeT::RootNodeType::ChildNodeType;
      using InternalNode2 = typename InternalNode1::ChildNodeType;
      /* Find the internal node that contains the tile and update the value in there. */
      if (auto *node = tree.template probeNode<InternalNode2>(coord_in_tile)) {
        set_tile_value(*node, coord_in_tile, computed_value);
      }
      else if (auto *node = tree.template probeNode<InternalNode1>(coord_in_tile)) {
        set_tile_value(*node, coord_in_tile, computed_value);
      }
      else {
        BLI_assert_unreachable();
      }
    }
  });
}

void set_mask_leaf_buffer_from_bools(openvdb::BoolGrid &grid,
                                     const Span<bool> values,
                                     const IndexMask &index_mask,
                                     const Span<openvdb::Coord> voxels)
{
  auto accessor = grid.getUnsafeAccessor();
  /* Could probably use int16_t for the iteration index. Double check this. */
  index_mask.foreach_index_optimized<int>([&](const int i) {
    const openvdb::Coord &coord = voxels[i];
    accessor.setValue(coord, values[i]);
  });
}

void set_grid_background(openvdb::GridBase &grid_base, const GPointer value)
{
  to_typed_grid(grid_base, [&](auto &grid) {
    using GridT = std::decay_t<decltype(grid)>;
    using ValueType = typename GridT::ValueType;
    auto &tree = grid.tree();

    BLI_assert(value.type()->size == sizeof(ValueType));
    tree.root().setBackground(*static_cast<const ValueType *>(value.get()), true);
  });
}

void prune_inactive(openvdb::GridBase &grid_base)
{
  to_typed_grid(grid_base, [&](auto &grid) { openvdb::tools::pruneInactive(grid.tree()); });
}

#endif /* WITH_OPENVDB */

}  // namespace blender::bke::volume_grid
