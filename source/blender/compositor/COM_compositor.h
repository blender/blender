/*
 * Copyright 2011, Blender Foundation.
 *
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
 *
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#ifndef __COM_COMPOSITOR_H__
#define __COM_COMPOSITOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_color_types.h"
#include "DNA_node_types.h"

/**
 * @defgroup Model The data model of the compositor
 * @defgroup Memory The memory management stuff
 * @defgroup Execution The execution logic
 * @defgroup Conversion Conversion logic
 * @defgroup Node All nodes of the compositor
 * @defgroup Operation All operations of the compositor
 *
 * @mainpage Introduction of the Blender Compositor
 *
 * @section bcomp Blender compositor
 * This project redesigns the internals of Blender's compositor. The project has been executed in 2011 by At Mind.
 * At Mind is a technology company located in Amsterdam, The Netherlands.
 * The project has been crowd-funded. This code has been released under GPL2 to be used in Blender.
 *
 * @section goals The goals of the project
 * the new compositor has 2 goals.
 *   - Make a faster compositor (speed of calculation)
 *   - Make the compositor work faster for you (workflow)
 *
 * @section speed Faster compositor
 * The speedup has been done by making better use of the hardware Blenders is working on. The previous compositor only
 * used a single threaded model to calculate a node. The only exception to this is the Defocus node.
 * Only when it is possible to calculate two full nodes in parallel a second thread was used.
 * Current workstations have 8-16 threads available, and most of the time these are idle.
 *
 * In the new compositor we want to use as much of threads as possible. Even new OpenCL capable GPU-hardware can be
 * used for calculation.
 *
 * @section workflow Work faster
 * The previous compositor only showed the final image. The compositor could wait a long time before seeing the result
 * of his work. The new compositor will work in a way that it will focus on getting information back to the user.
 * It will prioritize its work to get earlier user feedback.
 *
 * @page memory Memory model
 * The main issue is the type of memory model to use. Blender is used by consumers and professionals.
 * Ranging from low-end machines to very high-end machines.
 * The system should work on high-end machines and on low-end machines.
 *
 *
 * @page executing Executing
 * @section prepare Prepare execution
 *
 * during the preparation of the execution All ReadBufferOperation will receive an offset.
 * This offset is used during execution as an optimization trick
 * Next all operations will be initialized for execution @see NodeOperation.initExecution
 * Next all ExecutionGroup's will be initialized for execution @see ExecutionGroup.initExecution
 * this all is controlled from @see ExecutionSystem.execute
 *
 * @section priority Render priority
 * Render priority is an priority of an output node. A user has a different need of Render priorities of output nodes
 * than during editing.
 * for example. the Active ViewerNode has top priority during editing, but during rendering a CompositeNode has.
 * All NodeOperation has a setting for their render-priority, but only for output NodeOperation these have effect.
 * In ExecutionSystem.execute all priorities are checked. For every priority the ExecutionGroup's are check if the
 * priority do match.
 * When match the ExecutionGroup will be executed (this happens in serial)
 *
 * @see ExecutionSystem.execute control of the Render priority
 * @see NodeOperation.getRenderPriority receive the render priority
 * @see ExecutionGroup.execute the main loop to execute a whole ExecutionGroup
 *
 * @section order Chunk order
 *
 * When a ExecutionGroup is executed, first the order of chunks are determined.
 * The settings are stored in the ViewerNode inside the ExecutionGroup. ExecutionGroups that have no viewer-node,
 * will use a default one.
 * There are several possible chunk orders
 *  - [@ref OrderOfChunks.COM_TO_CENTER_OUT]: Start calculating from a configurable point and order by nearest chunk
 *  - [@ref OrderOfChunks.COM_TO_RANDOM]: Randomize all chunks.
 *  - [@ref OrderOfChunks.COM_TO_TOP_DOWN]: Start calculation from the bottom to the top of the image
 *  - [@ref OrderOfChunks.COM_TO_RULE_OF_THIRDS]: Experimental order based on 9 hot-spots in the image
 *
 * When the chunk-order is determined, the first few chunks will be checked if they can be scheduled.
 * Chunks can have three states:
 *  - [@ref ChunkExecutionState.COM_ES_NOT_SCHEDULED]: Chunk is not yet scheduled, or dependencies are not met
 *  - [@ref ChunkExecutionState.COM_ES_SCHEDULED]: All dependencies are met, chunk is scheduled, but not finished
 *  - [@ref ChunkExecutionState.COM_ES_EXECUTED]: Chunk is finished
 *
 * @see ExecutionGroup.execute
 * @see ViewerOperation.getChunkOrder
 * @see OrderOfChunks
 *
 * @section interest Area of interest
 * An ExecutionGroup can have dependencies to other ExecutionGroup's. Data passing from one ExecutionGroup to another
 * one are stored in 'chunks'.
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
 * In the above example ExecutionGroup B has an outputoperation (ViewerOperation) and is being executed.
 * The first chunk is evaluated [@ref ExecutionGroup.scheduleChunkWhenPossible],
 * but not all input chunks are available. The relevant ExecutionGroup (that can calculate the missing chunks;
 * ExecutionGroup A) is asked to calculate the area ExecutionGroup B is missing.
 * [@ref ExecutionGroup.scheduleAreaWhenPossible]
 * ExecutionGroup B checks what chunks the area spans, and tries to schedule these chunks.
 * If all input data is available these chunks are scheduled [@ref ExecutionGroup.scheduleChunk]
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
 *            .                                .       | ExecutionGroup.scheduleChunkWhenPossible
 *            .                                .  O----/ (*)                                |
 *            .                                .  O                                         |
 *            .                                .  O                                         |
 *            .                                .  O  ExecutionGroup.scheduleAreaWhenPossible|
 *            .                                .  O---------------------------------------->O
 *            .                                .  .                                         O----------\ ExecutionGroup.scheduleChunkWhenPossible
 *            .                                .  .                                         .          | (*)
 *            .                                .  .                                         .  O-------/
 *            .                                .  .                                         .  O
 *            .                                .  .                                         .  O
 *            .                                .  .                                         .  O-------\ ExecutionGroup.scheduleChunk
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
 * [@ref NodeOperation.determineAreaOfInterest] method
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
 * @see ExecutionGroup.execute Execute a complete ExecutionGroup. Halts until finished or breaked by user
 * @see ExecutionGroup.scheduleChunkWhenPossible Tries to schedule a single chunk,
 * checks if all input data is available. Can trigger dependant chunks to be calculated
 * @see ExecutionGroup.scheduleAreaWhenPossible Tries to schedule an area. This can be multiple chunks
 * (is called from [@ref ExecutionGroup.scheduleChunkWhenPossible])
 * @see ExecutionGroup.scheduleChunk Schedule a chunk on the WorkScheduler
 * @see NodeOperation.determineDependingAreaOfInterest Influence the area of interest of a chunk.
 * @see WriteBufferOperation NodeOperation to write to a MemoryProxy/MemoryBuffer
 * @see ReadBufferOperation NodeOperation to read from a MemoryProxy/MemoryBuffer
 * @see MemoryProxy proxy for information about memory image (a image consist out of multiple chunks)
 * @see MemoryBuffer Allocated memory for a single chunk
 *
 * @section workscheduler WorkScheduler
 * the WorkScheduler is implemented as a static class. the responsibility of the WorkScheduler is to balance
 * WorkPackages to the available and free devices.
 * the work-scheduler can work in 2 states. For witching these between the state you need to recompile blender
 *
 * @subsection multithread Multi threaded
 * Default the work-scheduler will place all work as WorkPackage in a queue.
 * For every CPUcore a working thread is created. These working threads will ask the WorkScheduler if there is work
 * for a specific Device.
 * the work-scheduler will find work for the device and the device will be asked to execute the WorkPackage
 *
 * @subsection singlethread Single threaded
 * For debugging reasons the multi-threading can be disabled. This is done by changing the COM_CURRENT_THREADING_MODEL
 * to COM_TM_NOTHREAD. When compiling the work-scheduler
 * will be changes to support no threading and run everything on the CPU.
 *
 * @section devices Devices
 * A Device within the compositor context is a Hardware component that can used to calculate chunks.
 * This chunk is encapsulated in a WorkPackage.
 * the WorkScheduler controls the devices and selects the device where a WorkPackage will be calculated.
 *
 * @subsection WS_Devices Workscheduler
 * The WorkScheduler controls all Devices. When initializing the compositor the WorkScheduler selects
 * all devices that will be used during compositor.
 * There are two types of Devices, CPUDevice and OpenCLDevice.
 * When an ExecutionGroup schedules a Chunk the schedule method of the WorkScheduler
 * The Workscheduler determines if the chunk can be run on an OpenCLDevice
 * (and that there are available OpenCLDevice). If this is the case the chunk will be added to the worklist for
 * OpenCLDevice's
 * otherwise the chunk will be added to the worklist of CPUDevices.
 *
 * A thread will read the work-list and sends a workpackage to its device.
 *
 * @see WorkScheduler.schedule method that is called to schedule a chunk
 * @see Device.execute method called to execute a chunk
 *
 * @subsection CPUDevice CPUDevice
 * When a CPUDevice gets a WorkPackage the Device will get the inputbuffer that is needed to calculate the chunk.
 * Allocation is already done by the ExecutionGroup.
 * The outputbuffer of the chunk is being created.
 * The OutputOperation of the ExecutionGroup is called to execute the area of the outputbuffer.
 *
 * @see ExecutionGroup
 * @see NodeOperation.executeRegion executes a single chunk of a NodeOperation
 * @see CPUDevice.execute
 *
 * @subsection GPUDevice OpenCLDevice
 *
 * To be completed!
 * @see NodeOperation.executeOpenCLRegion
 * @see OpenCLDevice.execute
 *
 * @section executePixel executing a pixel
 * Finally the last step, the node functionality :)
 *
 * @page newnode Creating new nodes
 */

/**
 * @brief The main method that is used to execute the compositor tree.
 * It can be executed during editing (blenkernel/node.c) or rendering
 * (renderer/pipeline.c)
 *
 * @param rd [struct RenderData]
 *   Render data for this composite, this won't always belong to a scene.
 *
 * @param editingtree [struct bNodeTree]
 *   reference to the compositor editing tree
 *
 * @param rendering [true false]
 *    This parameter determines whether the function is called from rendering (true) or editing (false).
 *    based on this setting the system will work differently:
 *     - during rendering only Composite & the File output node will be calculated
 * @see NodeOperation.isOutputProgram(int rendering) of the specific operations
 *
 *     - during editing all output nodes will be calculated
 * @see NodeOperation.isOutputProgram(int rendering) of the specific operations
 *
 *     - another quality setting can be used bNodeTree. The quality is determined by the bNodeTree fields.
 *       quality can be modified by the user from within the node panels.
 * @see bNodeTree.edit_quality
 * @see bNodeTree.render_quality
 *
 *     - output nodes can have different priorities in the WorkScheduler.
 * This is implemented in the COM_execute function.
 *
 * @param viewSettings
 *   reference to view settings used for color management
 *
 * @param displaySettings
 *   reference to display settings used for color management
 *
 * OCIO_TODO: this options only used in rare cases, namely in output file node,
 *            so probably this settings could be passed in a nicer way.
 *            should be checked further, probably it'll be also needed for preview
 *            generation in display space
 */
void COM_execute(RenderData *rd, Scene *scene, bNodeTree *editingtree, int rendering,
                 const ColorManagedViewSettings *viewSettings, const ColorManagedDisplaySettings *displaySettings);

/**
 * @brief Deinitialize the compositor caches and allocated memory.
 * Use COM_clearCaches to only free the caches.
 */
void COM_deinitialize(void);

/**
 * @brief Clear all compositor caches. (Compositor system will still remain available). 
 * To deinitialize the compositor use the COM_deinitialize method.
 */
// void COM_clearCaches(void); // NOT YET WRITTEN

/**
 * @brief Return a list of highlighted bnodes pointers.
 * @return 
 */
void COM_startReadHighlights(void);

/**
 * @brief check if a bnode is highlighted
 * @param bnode
 * @return 
 */
int COM_isHighlightedbNode(bNode *bnode);

#ifdef __cplusplus
}
#endif

#endif  /* __COM_COMPOSITOR_H__ */
