/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_color_types.h"
#include "DNA_node_types.h"

namespace blender::realtime_compositor {
class RenderContext;
}
namespace blender::compositor {
class ProfilerData;
}

struct Render;

/* Keep ascii art. */
/* clang-format off */

/**
 * \defgroup Model The data model of the compositor
 * \ingroup compositor
 * \defgroup Memory The memory management stuff
 * \ingroup compositor
 * \defgroup Execution The execution logic
 * \ingroup compositor
 * \defgroup Conversion Conversion logic
 * \ingroup compositor
 * \defgroup Node All nodes of the compositor
 * \ingroup compositor
 * \defgroup Operation All operations of the compositor
 * \ingroup compositor
 *
 * \section priority Render priority
 * Render priority is an priority of an output node.
 * A user has a different need of Render priorities of output nodes
 * than during editing.
 * for example. the Active ViewerNode has top priority during editing,
 * but during rendering a CompositeNode has.
 * All NodeOperation has a setting for their render-priority,
 * but only for output NodeOperation these have effect.
 * In ExecutionSystem.execute all priorities are checked.
 *
 * \section workscheduler WorkScheduler
 * the WorkScheduler is implemented as a static class. the responsibility of the WorkScheduler
 * is to balance WorkPackages to the available and free devices.
 * the work-scheduler can work in 2 states.
 * For witching these between the state you need to recompile blender
 *
 * \subsection multithread Multi threaded
 * Default the work-scheduler will place all work as WorkPackage in a queue.
 * For every CPUcore a working thread is created.
 * These working threads will ask the WorkScheduler if there is work
 * for a specific Device.
 * the work-scheduler will find work for the device and the device
 * will be asked to execute the WorkPackage.
 *
 * \subsection singlethread Single threaded
 * For debugging reasons the multi-threading can be disabled.
 * This is done by changing the `COM_threading_model`
 * to `ThreadingModel::SingleThreaded`. When compiling the work-scheduler
 * will be changes to support no threading and run everything on the CPU.
 */

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
 * \see bNodeTree.edit_quality
 * \see bNodeTree.render_quality
 *
 *     - output nodes can have different priorities in the WorkScheduler.
 * This is implemented in the COM_execute function.
 *
 * OCIO_TODO: this options only used in rare cases, namely in output file node,
 *            so probably this settings could be passed in a nicer way.
 *            should be checked further, probably it'll be also needed for preview
 *            generation in display space
 */
/* clang-format on */

void COM_execute(Render *render,
                 RenderData *render_data,
                 Scene *scene,
                 bNodeTree *node_tree,
                 bool rendering,
                 const char *view_name,
                 blender::realtime_compositor::RenderContext *render_context,
                 blender::compositor::ProfilerData &profiler_data);

/**
 * \brief Deinitialize the compositor caches and allocated memory.
 * Use COM_clear_caches to only free the caches.
 */
void COM_deinitialize(void);

/**
 * \brief Clear all compositor caches. (Compositor system will still remain available).
 * To deinitialize the compositor use the COM_deinitialize method.
 */
// void COM_clear_caches(void); // NOT YET WRITTEN
