/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_index_mask.hh"

#include "BKE_attribute.h"

struct Mesh;
namespace blender {
namespace fn {
template<typename T> class Field;
}
namespace bke {
class AnonymousAttributePropagationInfo;
}
}  // namespace blender

namespace blender::geometry {

std::optional<Mesh *> mesh_copy_selection(
    const Mesh &src_mesh,
    const fn::Field<bool> &selection,
    eAttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

std::optional<Mesh *> mesh_copy_selection_keep_verts(
    const Mesh &src_mesh,
    const fn::Field<bool> &selection,
    eAttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

std::optional<Mesh *> mesh_copy_selection_keep_edges(
    const Mesh &mesh,
    const fn::Field<bool> &selection,
    eAttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
