/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_color_types.h"
#include "DNA_node_types.h"

namespace blender::realtime_compositor {
class RenderContext;
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
 * \page Introduction of the Blender Compositor
 *
 * \section bcomp Blender compositor
 * This project redesigns the internals of Blender's compositor.
 * The project has been executed in 2011 by At Mind.
 * At Mind is a technology company located in Amsterdam, The Netherlands.
 * The project has been crowd-funded. This code has been released under GPL2 to be used in Blender.
 *
 * \section goals The goals of the project
 * the new compositor has 2 goals.
 *   - Make a faster compositor (speed of calculation)
 *   - Make the compositor work faster for you (workflow)
 *
 * \section speed Faster compositor
 * The speedup has been done by making better use of the hardware Blenders is working on.
 * The previous compositor only used a single threaded model to calculate a node.
 * The only exception to this is the Defocus node.
 * Only when it is possible to calculate two full nodes in parallel a second thread was used.
 * Current workstations have 8-16 threads available, and most of the time these are idle.
 *
 * In the new compositor we want to use as much of threads as possible.
 * Even new OpenCL capable GPU-hardware can be used for calculation.
 *
 * \section workflow Work faster
 * The previous compositor only showed the final image.
 * The compositor could wait a long time before seeing the result of his work.
 * The new compositor will work in a way that it will focus on
 * getting information back to the user. It will prioritize its work to get earlier user feedback.
 *
 * \page memory Memory model
 * The main issue is the type of memory model to use.
 * Blender is used by consumers and professionals.
 * Ranging from low-end machines to very high-end machines.
 * The system should work on high-end machines and on low-end machines.
 * \page executing Executing
 * \section prepare Prepare execution
 *
 * during the preparation of the execution All ReadBufferOperation will receive an offset.
 * This offset is used during execution as an optimization trick
 * Next all operations will be initialized for execution \see NodeOperation.init_execution
 * Next all ExecutionGroup's will be initialized for execution \see ExecutionGroup.init_execution
 * this all is controlled from \see ExecutionSystem.execute
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
 * For every priority the ExecutionGroup's are check if the
 * priority do match.
 * When match the ExecutionGroup will be executed (this happens in serial)
 *
 * \see ExecutionSystem.execute control of the Render priority
 * \see NodeOperation.get_render_priority receive the render priority
 * \see ExecutionGroup.execute the main loop to execute a whole ExecutionGroup
 *
 * \section order Chunk order
 *
 * When a ExecutionGroup is executed, first the order of chunks are determined.
 * The settings are stored in the ViewerNode inside the ExecutionGroup.
 * ExecutionGroups that have no viewer-node,
 * will use a default one.
 * There are several possible chunk orders
 *  - [@ref ChunkOrdering.CenterOut]:
 *    Start calculating from a configurable point and order by nearest chunk.
 *  - [@ref ChunkOrdering.Random]:
 *    Randomize all chunks.
 *  - [@ref ChunkOrdering.TopDown]:
 *    Start calculation from the bottom to the top of the image.
 *  - [@ref ChunkOrdering.RuleOfThirds]:
 *    Experimental order based on 9 hot-spots in the image.
 *
 * When the chunk-order is determined, the first few chunks will be checked if they can be scheduled.
 * Chunks can have three states:
 *  - [@ref eWorkPackageState.NotScheduled]:
 *    Chunk is not yet scheduled, or dependencies are not met.
 *  - [@ref eWorkPackageState.Scheduled]:
 *    All dependencies are met, chunk is scheduled, but not finished.
 *  - [@ref eWorkPackageState.Executed]:
 *    Chunk is finished.
 *
 * \see ExecutionGroup.execute
 * \see ViewerOperation.get_chunk_order
 * \see ChunkOrdering
 *
 * \section interest Area of interest
 * An ExecutionGroup can have dependencies to other ExecutionGroup's.
 * Data passing from one ExecutionGroup to another one are stored in 'chunks'.
 * If not all input chunks are available the chunk execution will not be scheduled.
 * <pre>
 * +-------------------------------------+              +--------------------------------------+
 * | ExecutionGroup A                    |              | ExecutionGroup B                     |
 * | +----------------+  +-------------+ |              | +------------+   +-----------------+ |
 * | | NodeOperation a|  | WriteBuffer | |              | | ReadBuffer |   | ViewerOperation | |
 * | |                *==* Operation   | |              | | Operation  *===*                 | |
 * | |                |  |             | |              | |            |   |                 | |
 * | +----------------+  +-------------+ |              | +------------+   +-----------------+ |
 * |                                |    |              |   |                                  |
 * +--------------------------------|----+              +---|----------------------------------+
 *                                  |                       |
 *                                  |                       |
 *                                +---------------------------+
 *                                | MemoryProxy               |
 *                                | +----------+  +---------+ |
 *                                | | Chunk a  |  | Chunk b | |
 *                                | |          |  |         | |
 *                                | +----------+  +---------+ |
 *                                |                           |
 *                                +---------------------------+
 * </pre>
 *
 * In the above example ExecutionGroup B has an outputoperation (ViewerOperation)
 * and is being executed.
 * The first chunk is evaluated [@ref ExecutionGroup.schedule_chunk_when_possible],
 * but not all input chunks are available.
 * The relevant ExecutionGroup (that can calculate the missing chunks; ExecutionGroup A)
 * is asked to calculate the area ExecutionGroup B is missing.
 * [@ref ExecutionGroup.schedule_area_when_possible]
 * ExecutionGroup B checks what chunks the area spans, and tries to schedule these chunks.
 * If all input data is available these chunks are scheduled [@ref ExecutionGroup.schedule_chunk]
 *
 * <pre>
 *
 * +-------------------------+        +----------------+                           +----------------+
 * | ExecutionSystem.execute |        | ExecutionGroup |                           | ExecutionGroup |
 * +-------------------------+        | (B)            |                           | (A)            |
 *            O                       +----------------+                           +----------------+
 *            O                                |                                            |
 *            O       ExecutionGroup.execute   |                                            |
 *            O------------------------------->O                                            |
 *            .                                O                                            |
 *            .                                O-------\                                    |
 *            .                                .       | ExecutionGroup.schedule_chunk_when_possible
 *            .                                .  O----/ (*)                                |
 *            .                                .  O                                         |
 *            .                                .  O                                         |
 *            .                                .  O  ExecutionGroup.schedule_area_when_possible|
 *            .                                .  O---------------------------------------->O
 *            .                                .  .                                         O----------\ ExecutionGroup.schedule_chunk_when_possible
 *            .                                .  .                                         .          | (*)
 *            .                                .  .                                         .  O-------/
 *            .                                .  .                                         .  O
 *            .                                .  .                                         .  O
 *            .                                .  .                                         .  O-------\ ExecutionGroup.schedule_chunk
 *            .                                .  .                                         .  .       |
 *            .                                .  .                                         .  .  O----/
 *            .                                .  .                                         .  O<=O
 *            .                                .  .                                         O<=O
 *            .                                .  .                                         O
 *            .                                .  O<========================================O
 *            .                                .  O                                         |
 *            .                                O<=O                                         |
 *            .                                O                                            |
 *            .                                O                                            |
 * </pre>
 *
 * This happens until all chunks of (ExecutionGroup B) are finished executing or the user break's the process.
 *
 * NodeOperation like the ScaleOperation can influence the area of interest by reimplementing the
 * [@ref NodeOperation.determine_area_of_interest] method
 *
 * <pre>
 *
 * +--------------------------+                             +---------------------------------+
 * | ExecutionGroup A         |                             | ExecutionGroup B                |
 * |                          |                             |                                 |
 * +--------------------------+                             +---------------------------------+
 *           Needed chunks from ExecutionGroup A               |   Chunk of ExecutionGroup B (to be evaluated)
 *            +-------+ +-------+                              |                  +--------+
 *            |Chunk 1| |Chunk 2|               +----------------+                |Chunk 1 |
 *            |       | |       |               | ScaleOperation |                |        |
 *            +-------+ +-------+               +----------------+                +--------+
 *
 *            +-------+ +-------+
 *            |Chunk 3| |Chunk 4|
 *            |       | |       |
 *            +-------+ +-------+
 *
 * </pre>
 *
 * \see ExecutionGroup.execute Execute a complete ExecutionGroup.
 * Halts until finished or breaked by user
 * \see ExecutionGroup.schedule_chunk_when_possible Tries to schedule a single chunk,
 * checks if all input data is available. Can trigger dependent chunks to be calculated
 * \see ExecutionGroup.schedule_area_when_possible
 * Tries to schedule an area. This can be multiple chunks
 * (is called from [@ref ExecutionGroup.schedule_chunk_when_possible])
 * \see ExecutionGroup.schedule_chunk Schedule a chunk on the WorkScheduler
 * \see NodeOperation.determine_depending_area_of_interest Influence the area of interest of a chunk.
 * \see WriteBufferOperation Operation to write to a MemoryProxy/MemoryBuffer
 * \see ReadBufferOperation Operation to read from a MemoryProxy/MemoryBuffer
 * \see MemoryProxy proxy for information about memory image
 * (a image consist out of multiple chunks)
 * \see MemoryBuffer Allocated memory for a single chunk
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
 *
 * \section devices Devices
 * A Device within the compositor context is a Hardware component that can used to calculate chunks.
 * This chunk is encapsulated in a WorkPackage.
 * the WorkScheduler controls the devices and selects the device where a
 * WorkPackage will be calculated.
 *
 * \subsection WS_Devices Work-scheduler
 * The WorkScheduler controls all Devices.
 * When initializing the compositor the WorkScheduler selects all
 * devices that will be used during compositor.
 * There are two types of Devices, CPUDevice and OpenCLDevice.
 * When an ExecutionGroup schedules a Chunk the schedule method of the WorkScheduler
 * The Workscheduler determines if the chunk can be run on an OpenCLDevice
 * (and that there are available OpenCLDevice).
 * If this is the case the chunk will be added to the work-list for OpenCLDevice's
 * otherwise the chunk will be added to the work-list of CPUDevices.
 *
 * A thread will read the work-list and sends a work-package to its device.
 *
 * \see WorkScheduler.schedule method that is called to schedule a chunk
 * \see Device.execute method called to execute a chunk
 *
 * \subsection CPUDevice CPUDevice
 * When a CPUDevice gets a WorkPackage the Device will get the input-buffer that is needed to
 * calculate the chunk. Allocation is already done by the ExecutionGroup.
 * The output-buffer of the chunk is being created.
 * The OutputOperation of the ExecutionGroup is called to execute the area of the output-buffer.
 *
 * \see ExecutionGroup
 * \see NodeOperation.execute_region executes a single chunk of a NodeOperation
 * \see CPUDevice.execute
 *
 * \subsection GPUDevice OpenCLDevice
 *
 * To be completed!
 * \see NodeOperation.execute_opencl_region
 * \see OpenCLDevice.execute
 *
 * \section execute_pixel executing a pixel
 * Finally the last step, the node functionality :)
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
/* clang-format off */

void COM_execute(Render *render,
                 RenderData *render_data,
                 Scene *scene,
                 bNodeTree *node_tree,
                 bool rendering,
                 const char *view_name,
                 blender::realtime_compositor::RenderContext *render_context);

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
