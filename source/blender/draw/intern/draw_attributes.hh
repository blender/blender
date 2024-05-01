/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Utilities for rendering attributes.
 */

#pragma once

#include <mutex>

#include "DNA_customdata_types.h"

#include "BLI_sys_types.h"

#include "GPU_shader.hh"

namespace blender::bke {
enum class AttrDomain : int8_t;
}

namespace blender::draw {

struct DRW_AttributeRequest {
  eCustomDataType cd_type;
  int layer_index;
  blender::bke::AttrDomain domain;
  char attribute_name[64];
};

struct DRW_Attributes {
  DRW_AttributeRequest requests[GPU_MAX_ATTR];
  int num_requests;
};

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

void drw_attributes_clear(DRW_Attributes *attributes);

void drw_attributes_merge(DRW_Attributes *dst,
                          const DRW_Attributes *src,
                          std::mutex &render_mutex);

/* Return true if all requests in b are in a. */
bool drw_attributes_overlap(const DRW_Attributes *a, const DRW_Attributes *b);

void drw_attributes_add_request(DRW_Attributes *attrs,
                                const char *name,
                                eCustomDataType data_type,
                                int layer_index,
                                blender::bke::AttrDomain domain);

bool drw_custom_data_match_attribute(const CustomData *custom_data,
                                     const char *name,
                                     int *r_layer_index,
                                     eCustomDataType *r_type);

}  // namespace blender::draw
