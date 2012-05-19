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

#ifndef _COM_NodeBase_h
#define _COM_NodeBase_h

#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "DNA_node_types.h"
#include "BKE_text.h"
#include <vector>
#include <string>

using namespace std;


class NodeOperation;
class ExecutionSystem;

/**
  * @brief The NodeBase class is the super-class of all node related objects like @see Node @see NodeOperation
  * the reason for the existence of this class is to support graph-nodes when using ExecutionSystem
  * the NodeBase also contains the reference to InputSocket and OutputSocket.
  * @ingroup Model
  */
class NodeBase {
private:
	/**
	  * @brief the list of actual inputsockets @see InputSocket
	  */
	vector<InputSocket*> inputsockets;

	/**
	  * @brief the list of actual outputsockets @see OutputSocket
	  */
	vector<OutputSocket*> outputsockets;

protected:
	/**
	  * @brief get access to the vector of input sockets
	  */
	inline vector<InputSocket*>& getInputSockets() {return this->inputsockets;}
	
	/**
	  * @brief get access to the vector of input sockets
	  */
	inline vector<OutputSocket*>& getOutputSockets() {return this->outputsockets;}


public:
	/**
	  * @brief destructor
	  * clean up memory related to this NodeBase.
	  */
	virtual ~NodeBase();
	
	/**
	  * @brief determine the actual socket data types that will go through the system
	  */
	virtual void determineActualSocketDataTypes();
	
	/**
	  * @brief determine the actual socket data types of a specific outputsocket
	  *
	  * @param outputsocket
	  * a reference to the actual outputsocket where the datatype must be determined from
	  *
	  * @return
	  * COM_DT_VALUE if it is a value (1 float buffer)
	  * COM_DT_COLOR if it is a value (4 float buffer)
	  * COM_DT_VECTOR if it is a value (3 float buffer)
	  */
	virtual DataType determineActualDataType(OutputSocket *outputsocket);
	
	/**
	  * @brief is this node an operation?
	  * This is true when the instance is of the subclass NodeOperation.
	  * @return [true:false]
	  * @see NodeOperation
	  */
	virtual const int isOperation() const {return false;}
	
	/**
	  * @brief check if this is an input node
	  * An input node is a node that only has output sockets and no input sockets
	  * @return [false..true]
	  */
	const bool isInputNode() const;
	
	/**
	  * @brief Return the number of input sockets of this node.
	  */
		const unsigned int getNumberOfInputSockets() const {return this->inputsockets.size();}
	
	/**
	  * @brief Return the number of output sockets of this node.
	  */
		const unsigned int getNumberOfOutputSockets() const {return this->outputsockets.size();}
	
	/**
	  * after the data has been determined of an outputsocket that has a connection with an inputsocket this method is called on the
	  * node that contains the inputsocket.
	  * @param socket
	  * the reference of the inputsocket where connected data type is found
	  * @param actualType [COM_DT_VALUE, COM_DT_VECTOR, COM_DT_COLOR]
	  * the actual data type that is coming from the connected output socket
	  */
	virtual void notifyActualDataTypeSet(InputSocket *socket, const DataType actualType);
	
	/**
	  * get the reference to a certain outputsocket
	  * @param index
	  * the index of the needed outputsocket
	  */
	OutputSocket *getOutputSocket(const int index);
	
	/**
	  * get the reference to the first outputsocket
	  * @param index
	  * the index of the needed outputsocket
	  */
	inline OutputSocket *getOutputSocket() {return getOutputSocket(0);}
	
	/**
	  * get the reference to a certain inputsocket
	  * @param index
	  * the index of the needed inputsocket
	  */
	InputSocket *getInputSocket(const int index);
	
	
	virtual bool isStatic() const {return false;}
	void getStaticValues(float *result) const {}
protected:
	NodeBase();
	
	/**
	  * @brief add an InputSocket to the collection of inputsockets
	  * @note may only be called in an constructor
	  * @param socket the InputSocket to add
	  */
	void addInputSocket(DataType datatype);
	void addInputSocket(DataType datatype, InputSocketResizeMode resizeMode);
	void addInputSocket(DataType datatype, InputSocketResizeMode resizeMode, bNodeSocket *socket);
	
	/**
	  * @brief add an OutputSocket to the collection of outputsockets
	  * @note may only be called in an constructor
	  * @param socket the OutputSocket to add
	  */
	void addOutputSocket(DataType datatype);
	void addOutputSocket(DataType datatype, bNodeSocket *socket);
};

#endif
