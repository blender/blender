/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_attribute_enums.hh"
#include "BKE_bake_data_block_map.hh"
#include "BKE_node_socket_value.hh"

#include "BLI_compute_context.hh"
#include "BLI_map.hh"

namespace blender::bke::bake {

/**
 * Intermediate storage for values that are read from a bake or are about to written to a bake.
 * Values stored in this intermediate form have some constraints. For example, they can't store
 * arbitrary fields (only attribute fields currently since those can be serialized). Additionally,
 * data-block references to e.g. materials are weak and will only be restored in
 * #to_runtime_values.
 */
class BakeValues {
 public:
  struct Item {
    SocketValueVariant value;
    std::optional<std::string> name;
  };

 private:
  Map<int, Item> values_by_id_;

 public:
  struct InputValue {
    int id;
    std::string name;
    SocketValueVariant value;
    /**
     * If a domain is given and the value is a field (that is not just an attribute), the field is
     * captured on the previous geometry. This mainly exists to preserve legacy behavior.
     */
    std::optional<AttrDomain> field_domain;
  };
  struct OutputKey {
    int id;
    eNodeSocketDatatype type;
  };

  BakeValues() = default;
  explicit BakeValues(Map<int, Item> values_by_id) : values_by_id_(std::move(values_by_id)) {}

  /**
   * Creates new bake values from the given values. This makes sure that the values follow the
   * restrictions for bake data.
   */
  static BakeValues from_runtime_values(Vector<InputValue> runtime_values,
                                        BakeDataBlockMap *data_block_map);

  /**
   * Create fully valid run-time data again from the bake data. This also restores potential
   * data-block references.
   */
  Vector<SocketValueVariant> to_runtime_values(const Span<OutputKey> keys,
                                               const ComputeContext &compute_context,
                                               BakeDataBlockMap *data_block_map) const;

  bool is_empty() const
  {
    return values_by_id_.is_empty();
  }

  void clear()
  {
    values_by_id_.clear();
  }

  const Map<int, Item> &values_by_id() const
  {
    return values_by_id_;
  }
};

}  // namespace blender::bke::bake
