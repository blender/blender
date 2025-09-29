/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * The render graph primarily is a a graph of GPU commands that are then serialized into command
 * buffers. The submission order can be altered and barriers are added for resource sync.
 *
 * # Building render graph
 *
 * The graph contains nodes that refers to resources it reads from, or modifies.
 * The resources that are read from are linked to the node inputs. The resources that are written
 * to are linked to the node outputs.
 *
 * Resources needs to be tracked as usage can alter the content of the resource. For example an
 * image can be optimized for data transfer, or optimized for sampling which can use a different
 * pixel layout on the device.
 *
 * When adding a node to the render graph the input and output links are extracted from the
 * See `VKNodeInfo::build_links`.
 *
 * # Executing render graph
 *
 * Executing a render graph is done by calling `submit_for_read` or `submit_for_present`. When
 * called the nodes that are needed to render the resource are determined by a `VKScheduler`. The
 * nodes are converted to `vkCmd*` and recorded in the command buffer by `VKCommandBuilder`.
 *
 * # Thread safety
 *
 * When the render graph is called the device will be locked. Nodes inside the render graph relies
 * on the resources which are device specific. The locked time is tiny when adding new nodes.
 * During execution this takes a longer time, but the lock can be released when the commands have
 * been queued. So other threads can continue.
 */

#pragma once

#include <mutex>
#include <optional>
#include <pthread.h>

#include "BKE_global.hh"

#include "BLI_color_types.hh"
#include "BLI_map.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BKE_global.hh"

#include "vk_common.hh"

#include "vk_command_buffer_wrapper.hh"
#include "vk_command_builder.hh"
#include "vk_render_graph_links.hh"
#include "vk_resource_state_tracker.hh"

namespace blender::gpu::render_graph {
class VKScheduler;

class VKRenderGraph : public NonCopyable {
  friend class VKCommandBuilder;
  friend class VKScheduler;
  using DebugGroupNameID = int64_t;
  using DebugGroupID = int64_t;

  /** All links inside the graph indexable via NodeHandle. */
  Vector<VKRenderGraphNodeLinks, 1024> links_;
  /** All nodes inside the graph indexable via NodeHandle. */
  Vector<VKRenderGraphNode, 1024> nodes_;
  /** Storage for large node datas to improve CPU cache pre-loading. */
  VKRenderGraphStorage storage_;

  /**
   * Not owning pointer to device resources.
   *
   * To improve testability the render graph doesn't access VKDevice or VKBackend directly.
   * resources_ can be replaced by a local variable. This way test cases don't need to create a
   * fully working context in order to test something render graph specific. Is marked optional as
   * device could
   */
  VKResourceStateTracker &resources_;

  struct DebugGroup {
    std::string name;
    ColorTheme4f color;

    BLI_STRUCT_EQUALITY_OPERATORS_2(DebugGroup, name, color)
    uint64_t hash() const
    {
      return get_default_hash<std::string, ColorTheme4f>(name, color);
    }
  };

  struct {
    VectorSet<DebugGroup> groups;

    /** Current stack of debug group names. */
    Vector<DebugGroupNameID> group_stack;

    /**
     * Has a node been added to the current stack? If not the group stack will be added to
     * used_groups.
     */
    bool group_used = false;

    /** All used debug groups. */
    Vector<Vector<DebugGroupNameID>> used_groups;

    /**
     * Map of a node_handle to an index of debug group in used_groups.
     *
     * <source>
     * int used_group_id = node_group_map[node_handle];
     * const Vector<DebugGroupNameID> &used_group = used_groups[used_group_id];
     * </source>
     */
    Vector<DebugGroupID> node_group_map;
  } debug_;

 public:
  /**
   * Construct a new render graph instance.
   *
   * To improve testability the command buffer and resources they work on are provided as a
   * parameter.
   */
  VKRenderGraph(VKResourceStateTracker &resources);

 private:
  /**
   * Add a node to the render graph.
   */
  template<typename NodeInfo> NodeHandle add_node(const typename NodeInfo::CreateInfo &create_info)
  {
    std::scoped_lock lock(resources_.mutex);
    static VKRenderGraphNode node_template = {};
    NodeHandle node_handle = nodes_.append_and_get_index(node_template);
#if 0
    /* Useful during debugging. When a validation error occurs during submission we know the node
     * type and node handle, but we don't know when and by who that specific node was added to the
     * render graph. By enabling this part of the code and set the correct node_handle and node
     * type a debugger can break at the moment the node has been added to the render graph. */
    if (node_handle == 267 && NodeInfo::node_type == VKNodeType::DRAW) {
      std::cout << "break\n";
    }
#endif
    if (nodes_.size() > links_.size()) {
      links_.resize(nodes_.size());
    }
    VKRenderGraphNode &node = nodes_[node_handle];
    node.set_node_data<NodeInfo>(storage_, create_info);

    VKRenderGraphNodeLinks &node_links = links_[node_handle];
    BLI_assert(node_links.inputs.is_empty());
    BLI_assert(node_links.outputs.is_empty());
    node.build_links<NodeInfo>(resources_, node_links, create_info);

    if (G.debug & G_DEBUG_GPU) {
      if (!debug_.group_used) {
        debug_.group_used = true;
        debug_.used_groups.append(debug_.group_stack);
      }
      if (nodes_.size() > debug_.node_group_map.size()) {
        debug_.node_group_map.resize(nodes_.size());
      }
      debug_.node_group_map[node_handle] = debug_.used_groups.size() - 1;
    }
    return node_handle;
  }

 public:
#define ADD_NODE(NODE_CLASS) \
  NodeHandle add_node(const NODE_CLASS::CreateInfo &create_info) \
  { \
    return add_node<NODE_CLASS>(create_info); \
  }
  ADD_NODE(VKBeginQueryNode)
  ADD_NODE(VKBeginRenderingNode)
  ADD_NODE(VKEndQueryNode)
  ADD_NODE(VKEndRenderingNode)
  ADD_NODE(VKClearAttachmentsNode)
  ADD_NODE(VKClearColorImageNode)
  ADD_NODE(VKClearDepthStencilImageNode)
  ADD_NODE(VKFillBufferNode)
  ADD_NODE(VKCopyBufferNode)
  ADD_NODE(VKCopyBufferToImageNode)
  ADD_NODE(VKCopyImageNode)
  ADD_NODE(VKCopyImageToBufferNode)
  ADD_NODE(VKBlitImageNode)
  ADD_NODE(VKDispatchNode)
  ADD_NODE(VKDispatchIndirectNode)
  ADD_NODE(VKDrawNode)
  ADD_NODE(VKDrawIndexedNode)
  ADD_NODE(VKDrawIndexedIndirectNode)
  ADD_NODE(VKDrawIndirectNode)
  ADD_NODE(VKResetQueryPoolNode)
  ADD_NODE(VKUpdateBufferNode)
  ADD_NODE(VKUpdateMipmapsNode)
  ADD_NODE(VKSynchronizationNode)
#undef ADD_NODE

  /**
   * Get the reference to the node data for a VKCopyBufferNode.
   *
   * Allows altering a previous added node. Is useful to reduce barriers when a streaming buffer
   * requires data that can still fit in the previous copy command.
   */
  VKCopyBufferNode::Data &get_node_data(NodeHandle node_handle)
  {
    VKRenderGraphNode &node = nodes_[node_handle];
    BLI_assert(node.type == VKNodeType::COPY_BUFFER);
    return node.copy_buffer;
  }

  /**
   * Push a new debugging group to the stack with the given name.
   *
   * New nodes added to the render graph will be associated with this debug group.
   */
  void debug_group_begin(const char *name, const ColorTheme4f &color);

  /**
   * Pop the top of the debugging group stack.
   *
   * New nodes added to the render graph will be associated with the parent of the current debug
   * group.
   */
  void debug_group_end();

  /**
   * Return the full debug group of the given node_handle. Returns an empty string when debug
   * groups are not enabled (`--debug-gpu`).
   */
  std::string full_debug_group(NodeHandle node_handle) const;

  /**
   * Utility function that is used during debugging.
   *
   * When debugging most of the time know the node_handle that is needed after the node has been
   * constructed. When haunting a bug it is more useful to query what the next node handle will be
   * so you can step through the node building process.
   */
  NodeHandle next_node_handle()
  {
    return nodes_.size();
  }

  bool is_empty()
  {
    return nodes_.is_empty();
  }

  void debug_print(NodeHandle node_handle) const;

  /**
   * Reset the render graph.
   */
  void reset();

  void memstats() const;

 private:
};

}  // namespace blender::gpu::render_graph
