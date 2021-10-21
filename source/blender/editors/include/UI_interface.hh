/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup editorui
 */

#pragma once

#include <memory>

#include "BLI_string_ref.hh"

namespace blender::nodes::geometry_nodes_eval_log {
struct GeometryAttributeInfo;
}

struct uiBlock;
namespace blender::ui {
class AbstractTreeView;

void attribute_search_add_items(
    StringRefNull str,
    const bool is_output,
    Span<const nodes::geometry_nodes_eval_log::GeometryAttributeInfo *> infos,
    uiSearchItems *items,
    const bool is_first);

}  // namespace blender::ui

blender::ui::AbstractTreeView *UI_block_add_view(
    uiBlock &block,
    blender::StringRef idname,
    std::unique_ptr<blender::ui::AbstractTreeView> tree_view);
