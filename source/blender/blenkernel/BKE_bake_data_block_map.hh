/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <optional>

#include "BLI_string_ref.hh"
#include "BLI_struct_equality_utils.hh"

#include "BKE_bake_data_block_id.hh"

#include "DNA_ID_enums.h"

namespace blender::bke::bake {

/**
 * Maps #BakeDataBlockID to the corresponding data-blocks. This is used during depsgraph evaluation
 * to remap weak data-block references stored in baked data to the actual data-blocks at run-time.
 *
 * Also it keeps track of missing data-blocks, so that they can be added later.
 */
struct BakeDataBlockMap {
 public:
  /**
   * Tries to retrieve the data block for the given key. If it's not explicitly mapped, it might be
   * added to the mapping. If it's still not found, null is returned.
   */
  virtual ID *lookup_or_remember_missing(const BakeDataBlockID &key) = 0;

  /**
   * Tries to add the data block to the map. This may not succeed in all cases, e.g. if the
   * implementation does not allow inserting new mapping items.
   */
  virtual void try_add(ID &id) = 0;
};

}  // namespace blender::bke::bake
