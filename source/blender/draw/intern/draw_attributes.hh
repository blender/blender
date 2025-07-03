/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Utilities for rendering attributes.
 */

#pragma once

#include <string>

#include "DNA_customdata_types.h"

#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"

#include "BLI_vector_set.hh"

namespace blender::bke {
enum class AttrDomain : int8_t;
}

namespace blender::draw {

struct DRW_MeshCDMask {
  uint32_t uv : 8;
  uint32_t tan : 8;
  uint32_t orco : 1;
  uint32_t tan_orco : 1;
  uint32_t sculpt_overlays : 1;
  /**
   * Edit uv layer is from the base edit mesh as modifiers could remove it. (see #68857)
   */
  uint32_t edit_uv : 1;
};

/* Keep `DRW_MeshCDMask` struct within a `uint32_t`.
 * bit-wise and atomic operations are used to compare and update the struct.
 * See `mesh_cd_layers_type_*` functions. */
static_assert(sizeof(DRW_MeshCDMask) <= sizeof(uint32_t), "DRW_MeshCDMask exceeds 32 bits");

void drw_attributes_merge(VectorSet<std::string> *dst, const VectorSet<std::string> *src);

/* Return true if all requests in b are in a. */
bool drw_attributes_overlap(const VectorSet<std::string> *a, const VectorSet<std::string> *b);

void drw_attributes_add_request(VectorSet<std::string> *attrs, StringRef name);

bool drw_custom_data_match_attribute(const CustomData &custom_data,
                                     StringRef name,
                                     int *r_layer_index,
                                     eCustomDataType *r_type);

}  // namespace blender::draw
