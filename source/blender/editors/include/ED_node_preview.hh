/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "RE_pipeline.h"

#include "IMB_imbuf.h"

struct bContext;
struct bNodeTree;
struct ImBuf;
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
ImBuf *node_preview_acquire_ibuf(bNodeTree &ntree,
                                 NestedTreePreviews &tree_previews,
                                 const bNode &node);
void node_release_preview_ibuf(NestedTreePreviews &tree_previews);
NestedTreePreviews *get_nested_previews(const bContext &C, SpaceNode &snode);
void stop_preview_job(wmWindowManager &wm);

}  // namespace blender::ed::space_node
