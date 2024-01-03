/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_index_mask.hh"

struct Mesh;
namespace blender {
namespace fn {
template<typename T> class Field;
}
namespace bke {
enum class AttrDomain : int8_t;
class AnonymousAttributePropagationInfo;
}  // namespace bke
}  // namespace blender

namespace blender::geometry {

std::optional<Mesh *> mesh_copy_selection(
    const Mesh &src_mesh,
    const VArray<bool> &selection,
    bke::AttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

std::optional<Mesh *> mesh_copy_selection_keep_verts(
    const Mesh &src_mesh,
    const VArray<bool> &selection,
    bke::AttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

std::optional<Mesh *> mesh_copy_selection_keep_edges(
    const Mesh &mesh,
    const VArray<bool> &selection,
    bke::AttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
