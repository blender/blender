/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"

struct Mesh;
namespace blender::bke {
class AnonymousAttributePropagationInfo;
}

namespace blender::geometry {

void split_edges(Mesh &mesh,
                 const IndexMask &mask,
                 const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
