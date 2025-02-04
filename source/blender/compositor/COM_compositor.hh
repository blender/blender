/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "DNA_node_types.h"

namespace blender::compositor {
class RenderContext;
class Profiler;
enum class OutputTypes : uint8_t;
}  // namespace blender::compositor

struct Render;

/**
 * \brief The main method that is used to execute the compositor tree.
 * It can be executed during editing (`blenkernel/node.cc`) or rendering
 * (`renderer/pipeline.cc`).
 *
 * \param render: Render instance for GPU context.
 *
 * \param render_data: Render data for this composite, this won't always belong to a scene.
 *
 * \param node_tree: Reference to the compositor editing tree
 *
 * \param rendering: This parameter determines whether the function is called from rendering
 *    (true) or editing (false).
 *    based on this setting the system will work differently:
 *     - during rendering only Composite & the File output node will be calculated
 * \see NodeOperation.is_output_program(bool rendering) of the specific operations
 *
 *     - during editing all output nodes will be calculated
 * \see NodeOperation.is_output_program(bool rendering) of the specific operations
 *
 *     - another quality setting can be used bNodeTree.
 *       The quality is determined by the bNodeTree fields.
 *       quality can be modified by the user from within the node panels.
 *
 *     - output nodes can have different priorities in the WorkScheduler.
 * This is implemented in the COM_execute function.
 *
 * OCIO_TODO: this options only used in rare cases, namely in output file node,
 *            so probably this settings could be passed in a nicer way.
 *            should be checked further, probably it'll be also needed for preview
 *            generation in display space
 */

void COM_execute(Render *render,
                 RenderData *render_data,
                 Scene *scene,
                 bNodeTree *node_tree,
                 const char *view_name,
                 blender::compositor::RenderContext *render_context,
                 blender::compositor::Profiler *profiler,
                 blender::compositor::OutputTypes needed_outputs);

/**
 * \brief Deinitialize the compositor caches and allocated memory.
 * Use COM_clear_caches to only free the caches.
 */
void COM_deinitialize();

/**
 * \brief Clear all compositor caches. (Compositor system will still remain available).
 * To deinitialize the compositor use the COM_deinitialize method.
 */
// void COM_clear_caches(); // NOT YET WRITTEN
