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

#ifndef _COM_Operation_h
#define _COM_Operation_h

#include <list>
#include <string>
#include <sstream>

extern "C" {
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_threads.h"
}

#include "COM_Node.h"
#include "COM_MemoryBuffer.h"
#include "COM_MemoryProxy.h"
#include "COM_SocketReader.h"

#include "OCL_opencl.h"

using std::list;
using std::min;
using std::max;

class OpenCLDevice;
class ReadBufferOperation;
class WriteBufferOperation;

class NodeOperationInput;
class NodeOperationOutput;

/**
 * @brief Resize modes of inputsockets
 * How are the input and working resolutions matched
 * @ingroup Model
 */
typedef enum InputResizeMode {
	/** @brief Center the input image to the center of the working area of the node, no resizing occurs */
	COM_SC_CENTER = NS_CR_CENTER,
	/** @brief The bottom left of the input image is the bottom left of the working area of the node, no resizing occurs */
	COM_SC_NO_RESIZE = NS_CR_NONE,
	/** @brief Fit the width of the input image to the width of the working area of the node */
	COM_SC_FIT_WIDTH = NS_CR_FIT_WIDTH,
	/** @brief Fit the height of the input image to the height of the working area of the node */
	COM_SC_FIT_HEIGHT = NS_CR_FIT_HEIGHT,
	/** @brief Fit the width or the height of the input image to the width or height of the working area of the node, image will be larger than the working area */
	COM_SC_FIT = NS_CR_FIT,
	/** @brief Fit the width and the height of the input image to the width and height of the working area of the node, image will be equally larger than the working area */
	COM_SC_STRETCH = NS_CR_STRETCH
} InputResizeMode;

/**
 * @brief NodeOperation contains calculation logic
 *
 * Subclasses needs to implement the execution method (defined in SocketReader) to implement logic.
 * @ingroup Model
 */
class NodeOperation : public SocketReader {
public:
	typedef std::vector<NodeOperationInput*> Inputs;
	typedef std::vector<NodeOperationOutput*> Outputs;
	
private:
	Inputs m_inputs;
	Outputs m_outputs;
	
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
	 * @brief reference to the editing bNodeTree, used for break and update callback
	 */
	const bNodeTree *m_btree;

	/**
	 * @brief set to truth when resolution for this operation is set
	 */
	bool m_isResolutionSet;
	
public:
	virtual ~NodeOperation();
	
	unsigned int getNumberOfInputSockets() const { return m_inputs.size(); }
	unsigned int getNumberOfOutputSockets() const { return m_outputs.size(); }
	NodeOperationOutput *getOutputSocket(unsigned int index) const;
	NodeOperationOutput *getOutputSocket() const { return getOutputSocket(0); }
	NodeOperationInput *getInputSocket(unsigned int index) const;
	
	/** Check if this is an input operation
	 * An input operation is an operation that only has output sockets and no input sockets
	 */
	bool isInputOperation() const { return m_inputs.empty(); }
	
	/**
	 * @brief determine the resolution of this node
	 * @note this method will not set the resolution, this is the responsibility of the caller
	 * @param resolution the result of this operation
	 * @param preferredResolution the preferable resolution as no resolution could be determined
	 */
	virtual void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);

	/**
	 * @brief isOutputOperation determines whether this operation is an output of the ExecutionSystem during rendering or editing.
	 *
	 * Default behaviour if not overridden, this operation will not be evaluated as being an output of the ExecutionSystem.
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
	virtual void executeRegion(rcti *rect, unsigned int chunkNumber) {}

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
	virtual void executeOpenCLRegion(OpenCLDevice *device, rcti *rect,
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
	virtual void executeOpenCL(OpenCLDevice *device,
	                           MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer,
	                           MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp,
	                           list<cl_kernel> *clKernelsToCleanUp) {}
	virtual void deinitExecution();

	bool isResolutionSet() {
		return this->m_isResolutionSet;
	}

	/**
	 * @brief set the resolution
	 * @param resolution the resolution to set
	 */
	void setResolution(unsigned int resolution[2]) {
		if (!isResolutionSet()) {
			this->m_width = resolution[0];
			this->m_height = resolution[1];
			this->m_isResolutionSet = true;
		}
	}
	

	void getConnectedInputSockets(Inputs *sockets);

	/**
	 * @brief is this operation complex
	 *
	 * Complex operations are typically doing many reads to calculate the output of a single pixel.
	 * Mostly Filter types (Blurs, Convolution, Defocus etc) need this to be set to true.
	 */
	const bool isComplex() const { return this->m_complex; }

	virtual bool isSetOperation() const { return false; }

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
	bool isOpenCL() const { return this->m_openCL; }
	
	virtual bool isViewerOperation() const { return false; }
	virtual bool isPreviewOperation() const { return false; }
	virtual bool isFileOutputOperation() const { return false; }
	virtual bool isProxyOperation() const { return false; }
	
	virtual bool useDatatypeConversion() const { return true; }
	
	inline bool isBreaked() const {
		return this->m_btree->test_break(this->m_btree->tbh);
	}

	inline void updateDraw() {
		if (this->m_btree->update_draw)
			this->m_btree->update_draw(this->m_btree->udh);
	}
protected:
	NodeOperation();

	void addInputSocket(DataType datatype, InputResizeMode resize_mode = COM_SC_CENTER);
	void addOutputSocket(DataType datatype);

	void setWidth(unsigned int width) { this->m_width = width; this->m_isResolutionSet = true; }
	void setHeight(unsigned int height) { this->m_height = height; this->m_isResolutionSet = true; }
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

	/* allow the DebugInfo class to look at internals */
	friend class DebugInfo;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};


class NodeOperationInput {
private:
	NodeOperation *m_operation;
	
	/** Datatype of this socket. Is used for automatically data transformation.
	 * @section data-conversion
	 */
	DataType m_datatype;
	
	/** Resize mode of this socket */
	InputResizeMode m_resizeMode;
	
	/** Connected output */
	NodeOperationOutput *m_link;
	
public:
	NodeOperationInput(NodeOperation *op, DataType datatype, InputResizeMode resizeMode = COM_SC_CENTER);
	
	NodeOperation &getOperation() const { return *m_operation; }
	DataType getDataType() const { return m_datatype; }
	
	void setLink(NodeOperationOutput *link) { m_link = link; }
	NodeOperationOutput *getLink() const { return m_link; }
	bool isConnected() const { return m_link; }
	
	void setResizeMode(InputResizeMode resizeMode) { this->m_resizeMode = resizeMode; }
	InputResizeMode getResizeMode() const { return this->m_resizeMode; }
	
	SocketReader *getReader();
	
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};


class NodeOperationOutput {
private:
	NodeOperation *m_operation;
	
	/** Datatype of this socket. Is used for automatically data transformation.
	 * @section data-conversion
	 */
	DataType m_datatype;
	
public:
	NodeOperationOutput(NodeOperation *op, DataType datatype);
	
	NodeOperation &getOperation() const { return *m_operation; }
	DataType getDataType() const { return m_datatype; }
	
	/**
	 * @brief determine the resolution of this data going through this socket
	 * @param resolution the result of this operation
	 * @param preferredResolution the preferable resolution as no resolution could be determined
	 */
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};

#endif
