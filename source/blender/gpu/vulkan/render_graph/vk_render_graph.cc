/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_render_graph.hh"
#include "gpu_backend.hh"

#include <sstream>

namespace blender::gpu::render_graph {

VKRenderGraph::VKRenderGraph(VKResourceStateTracker &resources) : resources_(resources) {}

void VKRenderGraph::reset()
{
#if 0
  memstats();
#endif

  links_.clear_and_shrink();
  for (VKRenderGraphNode &node : nodes_) {
    node.free_data(storage_);
  }
  nodes_.clear_and_shrink();
  storage_.reset();

  debug_.node_group_map.clear();
  debug_.used_groups.clear();
  debug_.group_stack.clear();
  debug_.groups.clear();
}

void VKRenderGraph::memstats() const
{
  std::cout << __func__ << " nodes: (" << nodes_.size() << "/" << nodes_.capacity() << "), "
            << "links: (" << links_.size() << "/" << links_.capacity() << ")\n";
#define PRINT_STORAGE(name) \
  std::cout << " " #name " : (" << storage_.name.size() << " / " << storage_.name.capacity() \
            << ")\n "

  PRINT_STORAGE(begin_rendering);
  PRINT_STORAGE(clear_attachments);
  PRINT_STORAGE(blit_image);
  PRINT_STORAGE(copy_buffer_to_image);
  PRINT_STORAGE(copy_image);
  PRINT_STORAGE(copy_image_to_buffer);
  PRINT_STORAGE(draw);
  PRINT_STORAGE(draw_indexed);
  PRINT_STORAGE(draw_indexed_indirect);
  PRINT_STORAGE(draw_indirect);
#undef PRINT_STORAGE
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

void VKRenderGraph::debug_group_begin(const char *name, const ColorTheme4f &color)
{
  ColorTheme4f useColor = color;
  if ((color == blender::gpu::debug::GPU_DEBUG_GROUP_COLOR_DEFAULT) &&
      (debug_.group_stack.size() > 0))
  {
    useColor = debug_.groups[debug_.group_stack.last()].color;
  }
  DebugGroupNameID name_id = debug_.groups.index_of_or_add({std::string(name), useColor});
  debug_.group_stack.append(name_id);
  debug_.group_used = false;
}

void VKRenderGraph::debug_group_end()
{
  debug_.group_stack.pop_last();
  debug_.group_used = false;
}

void VKRenderGraph::debug_print(NodeHandle node_handle) const
{
  std::ostream &os = std::cout;
  os << "NODE:\n";
  const VKRenderGraphNode &node = nodes_[node_handle];
  os << "  type:" << node.type << "\n";

  const VKRenderGraphNodeLinks &links = links_[node_handle];
  os << " inputs:\n";
  for (const VKRenderGraphLink &link : links.inputs) {
    os << "  ";
    link.debug_print(os, resources_);
    os << "\n";
  }
  os << " outputs:\n";
  for (const VKRenderGraphLink &link : links.outputs) {
    os << "  ";
    link.debug_print(os, resources_);
    os << "\n";
  }
}

std::string VKRenderGraph::full_debug_group(NodeHandle node_handle) const
{
  if ((G.debug & G_DEBUG_GPU) == 0) {
    return std::string();
  }

  DebugGroupID debug_group = debug_.node_group_map[node_handle];
  if (debug_group == -1) {
    return std::string();
  }

  std::stringstream ss;
  for (const VKRenderGraph::DebugGroupNameID &name_id : debug_.used_groups[debug_group]) {
    ss << "/" << debug_.groups[name_id].name;
  }

  return ss.str();
}

/** \} */

}  // namespace blender::gpu::render_graph
