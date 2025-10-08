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

#include "BLI_string_ref.hh"
#include "BLI_struct_equality_utils.hh"
#include "BLI_sys_types.h"

#include "BLI_vector_set.hh"

namespace blender::bke {
enum class AttrDomain : int8_t;
}

namespace blender::draw {

struct DRW_MeshCDMask {
  VectorSet<std::string> uv;
  VectorSet<std::string> tan;
  bool orco = false;
  bool tan_orco = false;
  bool sculpt_overlays = false;
  bool edit_uv = false;
  BLI_STRUCT_EQUALITY_OPERATORS_6(
      DRW_MeshCDMask, uv.as_span(), uv.as_span(), orco, tan_orco, sculpt_overlays, edit_uv);
  void clear()
  {
    uv.clear_and_keep_capacity();
    tan.clear_and_keep_capacity();
    orco = false;
    tan_orco = false;
    sculpt_overlays = false;
    edit_uv = false;
  }
};

void drw_attributes_merge(VectorSet<std::string> *dst, const VectorSet<std::string> *src);

/* Return true if all requests in b are in a. */
bool drw_attributes_overlap(const VectorSet<std::string> *a, const VectorSet<std::string> *b);

void drw_attributes_add_request(VectorSet<std::string> *attrs, StringRef name);

}  // namespace blender::draw
