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

#ifndef _COM_NodeOperation_h
#define _COM_NodeOperation_h
class OpenCLDevice;
#include "COM_Node.h"
#include <string>
#include <sstream>
#include "COM_MemoryBuffer.h"
#include "COM_MemoryProxy.h"
#include "COM_SocketReader.h"
#include "OCL_opencl.h"
#include "list"
#include "BLI_threads.h"

#include "BLI_math_color.h"
#include "BLI_math_vector.h"

class ReadBufferOperation;

/**
 * @brief NodeOperation are contains calculation logic
 *
 * Subclasses needs to implement the execution method (defined in SocketReader) to implement logic.
 * @ingroup Model
 */
class NodeOperation : public NodeBase, public SocketReader {
private:
	/**
	 * @brief the index of the input socket that will be used to determine the resolution
	 */
	unsigned int m_resolutionInputSocketIndex;

	/**
	 * @brief is this operation a complex one.
	 *
	 * Complex operations are typically doing many reads to calculate the output of a single pixel.
	 * Mostly Filter types (Blurs, Convolution, Defocus etc) need this to be set to true.
	 */
	bool m_complex;

	/**
	 * @brief can this operation be scheduled on an OpenCL device.
	 * @note Only applicable if complex is True
	 */
	bool m_openCL;

	/**
	 * @brief mutex reference for very special node initializations
	 * @note only use when you really know what you are doing.
	 * this mutex is used to share data among chunks in the same operation
	 * @see TonemapOperation for an example of usage
	 * @see NodeOperation.initMutex initializes this mutex
	 * @see NodeOperation.deinitMutex deinitializes this mutex
	 * @see NodeOperation.getMutex retrieve a pointer to this mutex.
	 */
	ThreadMutex m_mutex;
	
	/**
	 * @brief reference to the editing bNodeTree only used for break callback
	 */
	const bNodeTree *m_btree;

public:
	/**
	 * @brief is this node an operation?
	 * This is true when the instance is of the subclass NodeOperation.
	 * @return [true:false]
	 * @see NodeBase
	 */
	const int isOperation() const { return true; }

	/**
	 * @brief determine the resolution of this node
	 * @note this method will not set the resolution, this is the responsibility of the caller
	 * @param resolution the result of this operation
	 * @param preferredResolution the preferrable resolution as no resolution could be determined
	 */
	virtual void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

	/**
	 * @brief isOutputOperation determines whether this operation is an output of the ExecutionSystem during rendering or editing.
	 *
	 * Default behaviour if not overriden, this operation will not be evaluated as being an output of the ExecutionSystem.
	 *
	 * @see ExecutionSystem
	 * @group check
	 * @param rendering [true false]
	 *  true: rendering
	 *  false: editing
	 *
	 * @return bool the result of this method
	 */
	virtual bool isOutputOperation(bool rendering) const { return false; }

	/**
	 * isBufferOperation returns if this is an operation that work directly on buffers.
	 *
	 * there are only 2 implementation where this is true:
	 * @see ReadBufferOperation
	 * @see WriteBufferOperation
	 * for all other operations this will result in false.
	 */
	virtual int isBufferOperation() { return false; }
	virtual int isSingleThreaded() { return false; }

	void setbNodeTree(const bNodeTree *tree) { this->m_btree = tree; }
	virtual void initExecution();
	
	/**
	 * @brief when a chunk is executed by a CPUDevice, this method is called
	 * @ingroup execution
	 * @param rect the rectangle of the chunk (location and size)
	 * @param chunkNumber the chunkNumber to be calculated
	 * @param memoryBuffers all input MemoryBuffer's needed
	 */
	virtual void executeRegion(rcti *rect, unsigned int chunkNumber, MemoryBuffer **memoryBuffers) {}

	/**
	 * @brief when a chunk is executed by an OpenCLDevice, this method is called
	 * @ingroup execution
	 * @note this method is only implemented in WriteBufferOperation
	 * @param context the OpenCL context
	 * @param program the OpenCL program containing all compositor kernels
	 * @param queue the OpenCL command queue of the device the chunk is executed on
	 * @param rect the rectangle of the chunk (location and size)
	 * @param chunkNumber the chunkNumber to be calculated
	 * @param memoryBuffers all input MemoryBuffer's needed
	 * @param outputBuffer the outputbuffer to write to
	 */
	virtual void executeOpenCLRegion(OpenCLDevice* device, rcti *rect,
	                                 unsigned int chunkNumber, MemoryBuffer **memoryBuffers, MemoryBuffer *outputBuffer) {}

	/**
	 * @brief custom handle to add new tasks to the OpenCL command queue in order to execute a chunk on an GPUDevice
	 * @ingroup execution
	 * @param context the OpenCL context
	 * @param program the OpenCL program containing all compositor kernels
	 * @param queue the OpenCL command queue of the device the chunk is executed on
	 * @param outputMemoryBuffer the allocated memory buffer in main CPU memory
	 * @param clOutputBuffer the allocated memory buffer in OpenCLDevice memory
	 * @param inputMemoryBuffers all input MemoryBuffer's needed
	 * @param clMemToCleanUp all created cl_mem references must be added to this list. Framework will clean this after execution
	 * @param clKernelsToCleanUp all created cl_kernel references must be added to this list. Framework will clean this after execution
	 */
	virtual void executeOpenCL(OpenCLDevice* device, MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer, MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, list<cl_kernel> *clKernelsToCleanUp) {}
	virtual void deinitExecution();

	bool isResolutionSet() {
		return this->m_width != 0 && this->m_height != 0;
	}

	/**
	 * @brief set the resolution
	 * @param resolution the resolution to set
	 */
	void setResolution(unsigned int resolution[]) {
		if (!isResolutionSet()) {
			this->m_width = resolution[0];
			this->m_height = resolution[1];
		}
	}
	

	void getConnectedInputSockets(vector<InputSocket *> *sockets);

	/**
	 * @brief is this operation complex
	 *
	 * Complex operations are typically doing many reads to calculate the output of a single pixel.
	 * Mostly Filter types (Blurs, Convolution, Defocus etc) need this to be set to true.
	 */
	const bool isComplex() const { return this->m_complex; }
	virtual const bool isSetOperation() const { return false; }

	/**
	 * @brief is this operation of type ReadBufferOperation
	 * @return [true:false]
	 * @see ReadBufferOperation
	 */
	virtual const bool isReadBufferOperation() const { return false; }

	/**
	 * @brief is this operation of type WriteBufferOperation
	 * @return [true:false]
	 * @see WriteBufferOperation
	 */
	virtual const bool isWriteBufferOperation() const { return false; }

	/**
	 * @brief is this operation the active viewer output
	 * user can select an ViewerNode to be active (the result of this node will be drawn on the backdrop)
	 * @return [true:false]
	 * @see BaseViewerOperation
	 */
	virtual const bool isActiveViewerOutput() const { return false; }

	virtual bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	/**
	 * @brief set the index of the input socket that will determine the resolution of this operation
	 * @param index the index to set
	 */
	void setResolutionInputSocketIndex(unsigned int index);

	/**
	 * @brief get the render priority of this node.
	 * @note only applicable for output operations like ViewerOperation
	 * @return CompositorPriority
	 */
	virtual const CompositorPriority getRenderPriority() const { return COM_PRIORITY_LOW; }

	/**
	 * @brief can this NodeOperation be scheduled on an OpenCLDevice
	 * @see WorkScheduler.schedule
	 * @see ExecutionGroup.addOperation
	 */
	bool isOpenCL() { return this->m_openCL; }
	
	virtual bool isViewerOperation() { return false; }
	virtual bool isPreviewOperation() { return false; }
	
	inline bool isBreaked() {
		return this->m_btree->test_break(this->m_btree->tbh);
	}

protected:
	NodeOperation();

	void setWidth(unsigned int width) { this->m_width = width; }
	void setHeight(unsigned int height) { this->m_height = height; }
	SocketReader *getInputSocketReader(unsigned int inputSocketindex);
	NodeOperation *getInputOperation(unsigned int inputSocketindex);

	void deinitMutex();
	void initMutex();
	void lockMutex();
	void unlockMutex();
	

	/**
	 * @brief set whether this operation is complex
	 *
	 * Complex operations are typically doing many reads to calculate the output of a single pixel.
	 * Mostly Filter types (Blurs, Convolution, Defocus etc) need this to be set to true.
	 */
	void setComplex(bool complex) { this->m_complex = complex; }

	/**
	 * @brief set if this NodeOperation can be scheduled on a OpenCLDevice
	 */
	void setOpenCL(bool openCL) { this->m_openCL = openCL; }

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};

#endif
