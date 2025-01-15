/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"

#include "RE_pipeline.h"

#include "IMB_imbuf.hh"

#include "DNA_material_types.h"

struct ImBuf;
struct SpaceNode;
struct bContext;
struct bNode;
struct bNodeTree;
struct wmWindowManager;
struct Render;

namespace blender::ed::space_node {

struct NestedTreePreviews {
  Render *previews_render = nullptr;
  /** Use this map to keep track of the latest #ImBuf used (after freeing the render-result). */
  blender::Map<int32_t, ImBuf *> previews_map;
  int preview_size;
  bool rendering = false;
  bool restart_needed = false;
  ePreviewType cached_preview_type = MA_FLAT;
  ePreviewType rendering_preview_type = MA_FLAT;
  uint32_t cached_previews_refresh_state = -1;
  uint32_t rendering_previews_refresh_state = -1;
  NestedTreePreviews(const int size) : preview_size(size) {}
  ~NestedTreePreviews()
  {
    if (this->previews_render) {
      RE_FreeRender(this->previews_render);
    }
    for (ImBuf *ibuf : this->previews_map.values()) {
      IMB_freeImBuf(ibuf);
    }
  }
};

void free_previews(wmWindowManager &wm, SpaceNode &snode);
/**
 * \note #node_release_preview_ibuf should be called after this.
 */
ImBuf *node_preview_acquire_ibuf(bNodeTree &ntree,
                                 NestedTreePreviews &tree_previews,
                                 const bNode &node);
void node_release_preview_ibuf(NestedTreePreviews &tree_previews);
/**
 * This function returns the `NestedTreePreviews *` for the node-tree shown in the #SpaceNode.
 * This is the first function in charge of the previews by calling `ensure_nodetree_previews`.
 */
NestedTreePreviews *get_nested_previews(const bContext &C, SpaceNode &snode);

}  // namespace blender::ed::space_node
