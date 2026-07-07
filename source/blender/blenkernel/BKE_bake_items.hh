/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_memory_counter_fwd.hh"

#include "BKE_bake_data_block_map.hh"
#include "BKE_geometry_set.hh"
#include "BKE_volume_grid_fwd.hh"

#include "NOD_geometry_nodes_list_fwd.hh"

namespace blender::bke::bake {

/**
 * A "bake item" contains the baked data of e.g. one node socket at one frame. Typically, multiple
 * bake items form the entire baked state for one frame.
 *
 * Bake items can be serialized. Also see `BKE_bake_items_serialize.hh`.
 */
class BakeItem {
 public:
  /**
   * User-defined name. This is not necessarily unique and might change over time. It's purpose is
   * to make bakes more inspectable.
   */
  std::string name;

  virtual ~BakeItem() = default;

  virtual void count_memory(MemoryCounter &memory) const;
};

struct BakeState {
  /**
   * The ids are usually correspond to socket ids, so that the mapping stays intact even if socket
   * order changes.
   */
  Map<int, std::unique_ptr<BakeItem>> items_by_id;

  void count_memory(MemoryCounter &memory) const;
};

/** Same as #BakeState, but does not own the bake items. */
struct BakeStateRef {
  Map<int, const BakeItem *> items_by_id;

  BakeStateRef() = default;
  BakeStateRef(const BakeState &bake_state);
};

class GeometryBakeItem : public BakeItem {
 public:
  GeometrySet geometry;

  GeometryBakeItem(GeometrySet geometry);

  void count_memory(MemoryCounter &memory) const override;

  /**
   * Removes parts of the geometry that can't be baked/cached (anonymous attributes) and replaces
   * data-block pointers with #BakeDataBlockID.
   */
  static void prepare_geometry_for_bake(GeometrySet &geometry, BakeDataBlockMap *data_block_map);

  /**
   * The baked data does not have raw pointers to referenced data-blocks because those would become
   * dangling quickly. Instead it has weak name-based references (#BakeDataBlockID). This function
   * attempts to restore the actual data block pointers based on the weak references using the
   * given mapping.
   */
  static void try_restore_data_blocks(GeometrySet &geometry, BakeDataBlockMap *data_block_map);
};

/**
 * References a field input/output that becomes an attribute as part of the simulation state.
 * The attribute is actually stored in a #GeometryBakeItem, so this just references
 * the attribute's name.
 */
class AttributeBakeItem : public BakeItem {
 private:
  std::string name_;

 public:
  AttributeBakeItem(std::string name) : name_(std::move(name)) {}

  StringRefNull name() const
  {
    return name_;
  }
};

#ifdef WITH_OPENVDB
class VolumeGridBakeItem : public BakeItem {
 public:
  /** Using #unique_ptr so that `BKE_volume_grid_fwd.hh` can be used. */
  std::unique_ptr<GVolumeGrid> grid;

  VolumeGridBakeItem(std::unique_ptr<GVolumeGrid> grid);
  ~VolumeGridBakeItem() override;

  void count_memory(MemoryCounter &memory) const override;
};
#endif

/** Storage for a single value of a trivial type like `float`, `int`, etc. */
class PrimitiveBakeItem : public BakeItem {
 private:
  const CPPType &type_;
  void *value_;

 public:
  PrimitiveBakeItem(const CPPType &type, const void *value);
  ~PrimitiveBakeItem() override;

  const void *value() const
  {
    return value_;
  }

  const CPPType &type() const
  {
    return type_;
  }
};

class StringBakeItem : public BakeItem {
 private:
  std::string value_;

 public:
  StringBakeItem(std::string value);

  StringRefNull value() const
  {
    return value_;
  }

  void count_memory(MemoryCounter &memory) const override;
};

/**
 * \note It's not possible to use #PrimitiveBakeItem for bundles in general, because the items in
 * the bundle also have to be converted to their bakeable form. This is especially important when
 * serializing the bake.
 */
class BundleBakeItem : public BakeItem {
 public:
  struct SocketValue {
    std::string socket_idname;
    std::unique_ptr<BakeItem> value;
  };

  struct InternalValue {
    ImplicitSharingPtr<> value;
  };

  struct Item {
    std::string key;
    std::variant<SocketValue, InternalValue> value;
  };

  Vector<Item> items;
};

class ListBakeItem : public BakeItem {
 public:
  /* List of bake items for bundles which need additional preparation for baking. */
  using BundleList = Vector<BundleBakeItem>;

  std::variant<nodes::ListPtr, BundleList> value;

  ListBakeItem(nodes::ListPtr list);
  ListBakeItem(Vector<BundleBakeItem> &&items);
  ~ListBakeItem() override;

  void count_memory(MemoryCounter &memory) const override;
};

}  // namespace blender::bke::bake
