/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_string_ref.hh"
#include "BLI_struct_equality_utils.hh"
#include "BLI_vector.hh"

#include "DNA_ID_enums.h"

struct ID;
struct NodesModifierDataBlock;

namespace blender::bke::bake {

/**
 * Unique weak reference to a data block within a #Main. It's used when caching/baking data-block
 * references. Data-block pointers can't be used directly, because they are not stable over time
 * and between Blender sessions.
 */
struct BakeDataBlockID {
  ID_Type type;
  /**
   * Name of the data-block, without the type prefix.
   */
  std::string id_name;
  /**
   * Name of the library data-block that the data-block is in. This refers to `Library.id.name` and
   * not the file path. The type prefix of the name is omitted. If this is empty, the data-block is
   * expected to be local and not linked.
   */
  std::string lib_name;

  BakeDataBlockID(ID_Type type, std::string id_name, std::string lib_name);
  BakeDataBlockID(const ID &id);
  BakeDataBlockID(const NodesModifierDataBlock &data_block);

  uint64_t hash() const;

  friend std::ostream &operator<<(std::ostream &stream, const BakeDataBlockID &id);

  BLI_STRUCT_EQUALITY_OPERATORS_3(BakeDataBlockID, type, id_name, lib_name)
};

/**
 * A list of weak data-block references for material slots.
 */
struct BakeMaterialsList : public Vector<std::optional<BakeDataBlockID>> {};

}  // namespace blender::bke::bake
