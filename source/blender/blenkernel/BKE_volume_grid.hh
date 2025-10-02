/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_volume_grid_fwd.hh"

#ifdef WITH_OPENVDB

#  include <functional>
#  include <optional>

#  include "BKE_volume_enums.hh"
#  include "BKE_volume_grid_type_traits.hh"

#  include "BLI_cache_mutex.hh"
#  include "BLI_implicit_sharing_ptr.hh"
#  include "BLI_mutex.hh"
#  include "BLI_string_ref.hh"

#  include "openvdb_fwd.hh"

namespace blender::bke::volume_grid {

/**
 * A grid or tree may be loaded lazily when it's accessed. This is especially useful for grids that
 * are loaded from disk. Those may even be unloaded temporarily to avoid using too much memory.
 */
struct LazyLoadedGrid {
  /**
   * The newly loaded grid. In some cases, only the tree if this is used. The referenced tree is
   * expected to be either uniquely owned or implicitly-shared. In the latter case, the
   * implicit-sharing info for the tree has to be available too.
   */
  std::shared_ptr<openvdb::GridBase> grid;
  ImplicitSharingPtr<> tree_sharing_info;
};

/**
 * Main volume grid data structure. It wraps an OpenVDB grid and adds some features on top of it.
 *
 * A grid contains the following:
 * - Transform: The mapping between index and object space. It also determines e.g. the voxel size.
 * - Meta-data: Contains e.g. the name and grid class (fog volume or SDF) and potentially other
 *   data.
 * - Tree: This is the heavy data that contains all the voxel values.
 *
 * Features:
 * - Implicit sharing of the #VolumeGridData: This makes it cheap to copy e.g. a #VolumeGrid<T>,
 *   because it just increases the number of users. An actual copy is only done when the grid is
 *   modified.
 * - Implicit sharing of the referenced OpenVDB tree (not grid): The tree is the heavy piece of
 *   data that contains all the voxel values. Multiple #VolumeGridData can reference the same tree
 *   with independent meta-data and transforms. The tree is only actually copied when necessary.
 * - Lazy loading of the entire grid or just the tree: When constructing the #VolumeGridData it is
 *   possible to provide a callback that lazy-loads the grid when it is first accessed. This is
 *   especially beneficial when loading grids from a file and it's not clear in the beginning if
 *   the tree is actually needed. It's also supported to just load the meta-data and transform
 *   first and to load the tree only when it's used. This allows e.g. transforming or renaming the
 *   grid without loading the tree.
 * - Unloading of the tree: It's possible to unload the tree data when it is not in use. This is
 *   only supported on a shared grid if the tree could be reloaded (e.g. by reading it from a VDB
 *   file) and if no one is currently accessing the grid data.
 */
class VolumeGridData : public ImplicitSharingMixin {
 private:
  /**
   * Exists so that it can be used as token in #VolumeTreeAccessToken.
   */
  struct AccessToken {
    const VolumeGridData &grid;

    AccessToken(const VolumeGridData &grid) : grid(grid) {}
  };

  /**
   * A mutex that needs to be locked whenever working with the data members below.
   */
  mutable Mutex mutex_;
  /**
   * The actual grid. Depending on the current state, is in one of multiple possible states:
   * - Empty: When the grid is lazy-loaded and no meta-data is provided.
   * - Only meta-data and transform: When the grid is lazy-loaded and initial meta-data is
   *   provided.
   * - Complete: When the grid is fully loaded. It then contains the meta-data, transform and tree.
   *
   * `std::shared_ptr` is used, because some OpenVDB APIs expect the use of those. Unfortunately,
   * one can not insert and release data from a `shared_ptr`. Therefore, the grid has to be wrapped
   * by the `shared_ptr` at all times.
   *
   * However, this #VolumeGridData is considered to be the only actual owner of the grid. It is
   * also considered to be the only owner of the meta-data and transform in the grid. It is
   * possible to share the tree though.
   */
  mutable std::shared_ptr<openvdb::GridBase> grid_;
  /**
   * Keeps track of whether the tree in `grid_` is currently mutable or shared.
   */
  mutable ImplicitSharingPtr<> tree_sharing_info_;

  /** The tree stored in the grid is valid. */
  mutable bool tree_loaded_ = false;
  /** The transform stored in the grid is valid. */
  mutable bool transform_loaded_ = false;
  /** The meta-data stored in the grid is valid. */
  mutable bool meta_data_loaded_ = false;

  /**
   * A function that can load the full grid or also just the tree lazily.
   */
  std::function<LazyLoadedGrid()> lazy_load_grid_;
  /**
   * An error produced while trying to lazily load the grid.
   */
  mutable std::string error_message_;

  mutable CacheMutex active_voxels_mutex_;
  mutable int64_t active_voxels_ = 0;
  mutable CacheMutex active_leaf_voxels_mutex_;
  mutable int64_t active_leaf_voxels_ = 0;
  mutable CacheMutex active_tiles_mutex_;
  mutable int64_t active_tiles_ = 0;
  mutable CacheMutex size_in_bytes_mutex_;
  mutable int64_t size_in_bytes_ = 0;
  mutable CacheMutex active_bounds_mutex_;
  mutable openvdb::CoordBBox active_bounds_;

  /**
   * A token that allows detecting whether some code is currently accessing the tree (not grid) or
   * not. If this variable is the only owner of the `shared_ptr`, no one else has access to the
   * tree. `shared_ptr` is used here because it makes it very easy to manage a user-count without
   * much boilerplate.
   */
  std::shared_ptr<AccessToken> tree_access_token_;

  friend class VolumeTreeAccessToken;

  /** Private default constructor for internal purposes. */
  VolumeGridData();

 public:
  /**
   * Constructs a new volume grid of the given type where all voxels are inactive and the
   * background value is the default value (generally zero).
   */
  explicit VolumeGridData(VolumeGridType grid_type);

  /**
   * Constructs a new volume grid from the provided OpenVDB grid of which it takes ownership. The
   * grid must be moved into this constructor and must not be shared currently.
   */
  explicit VolumeGridData(std::shared_ptr<openvdb::GridBase> grid);

  /**
   * Constructs a new volume grid that loads the underlying OpenVDB data lazily.
   * \param lazy_load_grid: Function that is called when the data is first needed. It returns the
   *   new grid or may raise an exception. The returned meta-data and transform are ignored if the
   *   second parameter is provided here.
   * \param meta_data_and_transform_grid: An initial grid where the tree may be null. This grid
   *   might come from e.g. #readAllGridMetadata. This allows working with the transform and
   *   meta-data without actually loading the tree.
   */
  explicit VolumeGridData(std::function<LazyLoadedGrid()> lazy_load_grid,
                          std::shared_ptr<openvdb::GridBase> meta_data_and_transform_grid = {});

  ~VolumeGridData() override;

  /**
   * Create a copy of the volume grid. This should generally only be done when the current grid is
   * shared and one owner wants to modify it.
   *
   * This makes a deep copy of the transform and meta-data, but the tree remains shared and is only
   * copied later if necessary.
   */
  GVolumeGrid copy() const;

  /**
   * Get the underlying OpenVDB grid for read-only access. This may load the tree lazily if it's
   * not loaded already.
   */
  const openvdb::GridBase &grid(VolumeTreeAccessToken &r_token) const;
  /**
   * Get the underlying OpenVDB grid for read and write access. This may load the tree lazily if
   * it's not loaded already. It may also make a copy of the tree if it's currently shared.
   */
  openvdb::GridBase &grid_for_write(VolumeTreeAccessToken &r_token);

  /**
   * Same as #grid and #grid_for_write but returns the grid as a `shared_ptr` so that it can be
   * used with APIs that only support grids wrapped into one. This method is not supposed to
   * actually transfer ownership of the grid.
   */
  std::shared_ptr<const openvdb::GridBase> grid_ptr(VolumeTreeAccessToken &r_token) const;
  std::shared_ptr<openvdb::GridBase> grid_ptr_for_write(VolumeTreeAccessToken &r_token);

  /**
   * Get the name of the grid that's stored in the grid meta-data.
   */
  std::string name() const;
  /**
   * Replace the name of the grid that's stored in the meta-data.
   */
  void set_name(StringRef name);

  /**
   * Get the transform of the grid for read-only access. This may lazily load the data if it's not
   * yet available.
   */
  const openvdb::math::Transform &transform() const;
  /**
   * Get the transform of the grid for read and write access. This may lazily load the data if it's
   * not yet available.
   */
  openvdb::math::Transform &transform_for_write();

  /**
   * Grid type that's derived from the OpenVDB tree type.
   */
  VolumeGridType grid_type() const;

  /**
   * Same as #grid_type() but does not potentially call the lazy-load function to figure out the
   * grid type. This can be used e.g. by asserts.
   */
  std::optional<VolumeGridType> grid_type_without_load() const;

  /**
   * Grid class that is stored in the grid's meta data.
   */
  openvdb::GridClass grid_class() const;

  /**
   * True if the grid is fully loaded (including the meta-data, transform and tree).
   */
  bool is_loaded() const;

  void count_memory(MemoryCounter &memory) const;

  /**
   * Non-empty string if there was some error when trying to load the volume.
   */
  std::string error_message() const;

  /**
   * Tree if the tree can be loaded again after it has been unloaded.
   */
  bool is_reloadable() const;

  void tag_tree_modified() const;

  int64_t active_voxels() const;
  int64_t active_leaf_voxels() const;
  int64_t active_tiles() const;
  int64_t size_in_bytes() const;
  const openvdb::CoordBBox &active_bounds() const;

 private:
  /**
   * Unloads the tree data if it's reloadable and no one is using it right now.
   */
  void unload_tree_if_possible() const;

  void ensure_grid_loaded() const;
  void delete_self() override;
};

/**
 * Multiple #VolumeDataGrid can implicitly share the same underlying tree with different
 * meta-data/transforms. Note that this is different from using a `shared_ptr`, because that is
 * only used to please different APIs and does not enforce that shared data is immutable.
 */
class OpenvdbTreeSharingInfo : public ImplicitSharingInfo {
 private:
  std::shared_ptr<openvdb::tree::TreeBase> tree_;

 public:
  OpenvdbTreeSharingInfo(std::shared_ptr<openvdb::tree::TreeBase> tree);

  static ImplicitSharingPtr<> make(std::shared_ptr<openvdb::tree::TreeBase> tree);

  void delete_self_with_data() override;
  void delete_data_only() override;
};

class VolumeTreeAccessToken {
 private:
  std::shared_ptr<VolumeGridData::AccessToken> token_;

  friend VolumeGridData;

 public:
  /**
   * Defaulting everything but the destructor is fine because the default behavior works
   * correctly on the single data member. The destructor has a special implementation because it
   * may automatically unload the volume tree if it's not used anymore to conserve memory.
   */
  VolumeTreeAccessToken() = default;
  VolumeTreeAccessToken(const VolumeTreeAccessToken &) = default;
  VolumeTreeAccessToken(VolumeTreeAccessToken &&) = default;
  VolumeTreeAccessToken &operator=(const VolumeTreeAccessToken &) = default;
  VolumeTreeAccessToken &operator=(VolumeTreeAccessToken &&) = default;
  ~VolumeTreeAccessToken();

  /** True if the access token can be used with the given grid. */
  bool valid_for(const VolumeGridData &grid) const;

  /** Revoke the access token to indicating that the tree is not used anymore. */
  void reset();
};

/**
 * A #GVolumeGrid owns a volume grid. Semantically, each #GVolumeGrid is independent but implicit
 * sharing is used to avoid unnecessary deep copies.
 */
class GVolumeGrid {
 protected:
  ImplicitSharingPtr<VolumeGridData> data_;

 public:
  /**
   * Constructs a #GVolumeGrid that does not contain any data.
   */
  GVolumeGrid() = default;
  /**
   * Take (shared) ownership of the given grid data. The caller is responsible for making sure that
   * the user count includes a user for the newly constructed #GVolumeGrid.
   */
  explicit GVolumeGrid(const VolumeGridData *data);
  /**
   * Constructs a new volume grid that takes unique ownership of the passed in OpenVDB grid.
   */
  explicit GVolumeGrid(std::shared_ptr<openvdb::GridBase> grid);
  /**
   * Constructs an empty grid of the given type, where all voxels are inactive and the background
   * is the default value (generally zero).
   */
  explicit GVolumeGrid(VolumeGridType grid_type);

  /**
   * Get the underlying (potentially shared) volume grid data for read-only access.
   */
  const VolumeGridData &get() const;

  /**
   * Get the underlying volume grid data for read and write access. This may make a copy of the
   * grid data is shared.
   */
  VolumeGridData &get_for_write();

  /**
   * Move ownership of the underlying grid data to the caller.
   */
  const VolumeGridData *release();

  /** Makes it more convenient to retrieve data from the grid. */
  const VolumeGridData *operator->() const;

  /** True if this contains a grid. */
  operator bool() const;

  /** Converts to a typed VolumeGrid. This asserts if the type is wrong. */
  template<typename T> VolumeGrid<T> typed() const;
};

/**
 * Same as #GVolumeGrid but makes it easier to work with the grid if the type is known at compile
 * time.
 */
template<typename T> class VolumeGrid : public GVolumeGrid {
 public:
  using base_type = T;

  VolumeGrid() = default;
  explicit VolumeGrid(const VolumeGridData *data);
  explicit VolumeGrid(std::shared_ptr<OpenvdbGridType<T>> grid);

  /**
   * Wraps the same methods on #VolumeGridData but casts to the correct OpenVDB type.
   */
  const OpenvdbGridType<T> &grid(VolumeTreeAccessToken &r_token) const;
  OpenvdbGridType<T> &grid_for_write(VolumeTreeAccessToken &r_token);

 private:
  void assert_correct_type() const;
};

/**
 * Get the volume grid type based on the tree's type.
 */
VolumeGridType get_type(const openvdb::tree::TreeBase &tree);
/**
 * Get the volume grid type based on the tree type in the grid.
 */
VolumeGridType get_type(const openvdb::GridBase &grid);

/* -------------------------------------------------------------------- */
/** \name Inline Methods
 * \{ */

inline GVolumeGrid::GVolumeGrid(const VolumeGridData *data) : data_(data) {}

inline const VolumeGridData &GVolumeGrid::get() const
{
  BLI_assert(*this);
  return *data_;
}

inline const VolumeGridData *GVolumeGrid::release()
{
  return data_.release();
}

inline GVolumeGrid::operator bool() const
{
  return bool(data_);
}

template<typename T> inline VolumeGrid<T> GVolumeGrid::typed() const
{
  if (data_) {
    data_->add_user();
  }
  return VolumeGrid<T>(data_.get());
}

inline const VolumeGridData *GVolumeGrid::operator->() const
{
  BLI_assert(*this);
  return data_.get();
}

template<typename T>
inline VolumeGrid<T>::VolumeGrid(const VolumeGridData *data) : GVolumeGrid(data)
{
  this->assert_correct_type();
}

template<typename T>
inline VolumeGrid<T>::VolumeGrid(std::shared_ptr<OpenvdbGridType<T>> grid)
    : GVolumeGrid(std::move(grid))
{
  this->assert_correct_type();
}

template<typename T>
inline const OpenvdbGridType<T> &VolumeGrid<T>::grid(VolumeTreeAccessToken &r_token) const
{
  return static_cast<const OpenvdbGridType<T> &>(data_->grid(r_token));
}

template<typename T>
inline OpenvdbGridType<T> &VolumeGrid<T>::grid_for_write(VolumeTreeAccessToken &r_token)
{
  return static_cast<OpenvdbGridType<T> &>(this->get_for_write().grid_for_write(r_token));
}

template<typename T> inline void VolumeGrid<T>::assert_correct_type() const
{
#  ifndef NDEBUG
  if (data_) {
    const VolumeGridType expected_type = VolumeGridTraits<T>::EnumType;
    if (const std::optional<VolumeGridType> actual_type = data_->grid_type_without_load()) {
      BLI_assert(expected_type == *actual_type);
    }
  }
#  endif
}

inline bool VolumeTreeAccessToken::valid_for(const VolumeGridData &grid) const
{
  return grid.tree_access_token_ == token_;
}

inline void VolumeTreeAccessToken::reset()
{
  token_.reset();
}

/** \} */

}  // namespace blender::bke::volume_grid

#endif /* WITH_OPENVDB */
