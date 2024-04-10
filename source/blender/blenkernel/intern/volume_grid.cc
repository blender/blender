/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"

#include "BLI_task.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/Grid.h>
#endif

namespace blender::bke::volume_grid {

#ifdef WITH_OPENVDB

/**
 * Multiple #VolumeDataGrid can implicitly share the same underlying tree with different
 * meta-data/transforms.
 */
class OpenvdbTreeSharingInfo : public ImplicitSharingInfo {
 private:
  std::shared_ptr<openvdb::tree::TreeBase> tree_;

 public:
  OpenvdbTreeSharingInfo(std::shared_ptr<openvdb::tree::TreeBase> tree) : tree_(std::move(tree)) {}

  void delete_self_with_data() override
  {
    MEM_delete(this);
  }

  void delete_data_only() override
  {
    tree_.reset();
  }
};

VolumeGridData::VolumeGridData()
{
  tree_access_token_ = std::make_shared<AccessToken>();
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
  BLI_assert(grid_.unique());
  BLI_assert(grid_->isTreeUnique());

  tree_sharing_info_ = MEM_new<OpenvdbTreeSharingInfo>(__func__, grid_->baseTreePtr());
  tree_access_token_ = std::make_shared<AccessToken>();
}

VolumeGridData::VolumeGridData(std::function<std::shared_ptr<openvdb::GridBase>()> lazy_load_grid,
                               std::shared_ptr<openvdb::GridBase> meta_data_and_transform_grid)
    : grid_(std::move(meta_data_and_transform_grid)), lazy_load_grid_(std::move(lazy_load_grid))
{
  if (grid_) {
    transform_loaded_ = true;
    meta_data_loaded_ = true;
  }
  tree_access_token_ = std::make_shared<AccessToken>();
}

VolumeGridData::~VolumeGridData()
{
  if (tree_sharing_info_) {
    tree_sharing_info_->remove_user_and_delete_if_last();
  }
}

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
    tree_sharing_info_->remove_user_and_delete_if_last();
    tree_sharing_info_ = MEM_new<OpenvdbTreeSharingInfo>(__func__, std::move(tree_copy));
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

bool VolumeGridData::is_loaded() const
{
  std::lock_guard lock{mutex_};
  return tree_loaded_ && transform_loaded_ && meta_data_loaded_;
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
  if (!this->is_reloadable()) {
    return;
  }
  if (!tree_access_token_.unique()) {
    /* Some code is using the tree currently, so it can't be freed. */
    return;
  }
  grid_->newTree();
  tree_loaded_ = false;
  tree_sharing_info_->remove_user_and_delete_if_last();
  tree_sharing_info_ = nullptr;
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
  new_copy->tree_sharing_info_->add_user();
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
  std::shared_ptr<openvdb::GridBase> loaded_grid;
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
  if (!loaded_grid) {
    if (grid_) {
      /* Create a dummy grid of the expected type. */
      loaded_grid = grid_->createGrid("");
    }
    else {
      /* Create a dummy grid. We can't really know the expected data type here. */
      loaded_grid = openvdb::FloatGrid::create();
    }
  }
  BLI_assert(loaded_grid);
  BLI_assert(loaded_grid.unique());
  BLI_assert(loaded_grid->isTreeUnique());

  if (grid_) {
    /* Keep the existing grid pointer and just insert the newly loaded data. */
    BLI_assert(!tree_loaded_);
    BLI_assert(meta_data_loaded_);
    grid_->setTree(loaded_grid->baseTreePtr());
    if (!transform_loaded_) {
      grid_->setTransform(loaded_grid->transformPtr());
    }
  }
  else {
    grid_ = std::move(loaded_grid);
  }

  BLI_assert(tree_sharing_info_ == nullptr);
  tree_sharing_info_ = MEM_new<OpenvdbTreeSharingInfo>(__func__, grid_->baseTreePtr());

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

VolumeGridType get_type(const openvdb::GridBase &grid)
{
  if (grid.isType<openvdb::FloatGrid>()) {
    return VOLUME_GRID_FLOAT;
  }
  if (grid.isType<openvdb::Vec3fGrid>()) {
    return VOLUME_GRID_VECTOR_FLOAT;
  }
  if (grid.isType<openvdb::BoolGrid>()) {
    return VOLUME_GRID_BOOLEAN;
  }
  if (grid.isType<openvdb::DoubleGrid>()) {
    return VOLUME_GRID_DOUBLE;
  }
  if (grid.isType<openvdb::Int32Grid>()) {
    return VOLUME_GRID_INT;
  }
  if (grid.isType<openvdb::Int64Grid>()) {
    return VOLUME_GRID_INT64;
  }
  if (grid.isType<openvdb::Vec3IGrid>()) {
    return VOLUME_GRID_VECTOR_INT;
  }
  if (grid.isType<openvdb::Vec3dGrid>()) {
    return VOLUME_GRID_VECTOR_DOUBLE;
  }
  if (grid.isType<openvdb::MaskGrid>()) {
    return VOLUME_GRID_MASK;
  }
  if (grid.isType<openvdb::points::PointDataGrid>()) {
    return VOLUME_GRID_POINTS;
  }
  return VOLUME_GRID_UNKNOWN;
}

#endif /* WITH_OPENVDB */

std::string get_name(const VolumeGridData &volume_grid)
{
#ifdef WITH_OPENVDB
  return volume_grid.name();
#else
  UNUSED_VARS(volume_grid);
  return "density";
#endif
}

VolumeGridType get_type(const VolumeGridData &volume_grid)
{
#ifdef WITH_OPENVDB
  return volume_grid.grid_type();
#else
  UNUSED_VARS(volume_grid);
#endif
  return VOLUME_GRID_UNKNOWN;
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

void unload_tree_if_possible(const VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  grid.unload_tree_if_possible();
#else
  UNUSED_VARS(grid);
#endif
}

float4x4 get_transform_matrix(const VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  const openvdb::math::Transform &transform = grid.transform();

  /* Perspective not supported for now, getAffineMap() will leave out the
   * perspective part of the transform. */
  openvdb::math::Mat4f matrix = transform.baseMap()->getAffineMap()->getMat4();
  /* Blender column-major and OpenVDB right-multiplication conventions match. */
  float4x4 result;
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      result[col][row] = matrix(col, row);
    }
  }
  return result;
#else
  UNUSED_VARS(grid);
  return float4x4::identity();
#endif
}

void set_transform_matrix(VolumeGridData &grid, const float4x4 &matrix)
{
#ifdef WITH_OPENVDB
  openvdb::math::Mat4f matrix_openvdb;
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      matrix_openvdb(col, row) = matrix[col][row];
    }
  }

  grid.transform_for_write() = openvdb::math::Transform(
      std::make_shared<openvdb::math::AffineMap>(matrix_openvdb));
#else
  UNUSED_VARS(grid, matrix);
#endif
}

void clear_tree(VolumeGridData &grid)
{
#ifdef WITH_OPENVDB
  VolumeTreeAccessToken tree_token;
  grid.grid_for_write(tree_token).clear();
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

}  // namespace blender::bke::volume_grid
