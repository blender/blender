/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"

namespace blender::bke::bake {

/**
 * A "bake item" contains the baked data of e.g. one node socket at one frame. Typically, multiple
 * bake items form the entire baked state for one frame.
 *
 * Bake items can be serialized. Also see `BKE_bake_items_serialize.hh`.
 */
class BakeItem {
 public:
  virtual ~BakeItem() = default;
};

struct BakeState {
  /**
   * The ids are usually correspond to socket ids, so that the mapping stays intact even if socket
   * order changes.
   */
  Map<int, std::unique_ptr<BakeItem>> items_by_id;
};

/** Same as above, but does not own the bake items. */
struct BakeStateRef {
  Map<int, const BakeItem *> items_by_id;

  BakeStateRef() = default;
  BakeStateRef(const BakeState &bake_state);
};

class GeometryBakeItem : public BakeItem {
 public:
  GeometrySet geometry;

  GeometryBakeItem(GeometrySet geometry);

  /**
   * Removes parts of the geometry that can't be stored in the simulation state:
   * - Anonymous attributes can't be stored because it is not known which of them will or will not
   * be used in the future.
   * - Materials can't be stored directly, because they are linked ID data blocks that can't be
   *   restored from baked data currently.
   */
  static void cleanup_geometry(GeometrySet &geometry);
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

/** Storage for a single value of a trivial type like `float`, `int`, etc. */
class PrimitiveBakeItem : public BakeItem {
 private:
  const CPPType &type_;
  void *value_;

 public:
  PrimitiveBakeItem(const CPPType &type, const void *value);
  ~PrimitiveBakeItem();

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
};

}  // namespace blender::bke::bake
