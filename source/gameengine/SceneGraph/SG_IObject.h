/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __SG_IOBJECT
#define __SG_IOBJECT

#include <vector>

class SG_Controller;
class SG_IObject;

typedef std::vector<SG_Controller*> SGControllerList;

typedef void* (*SG_ReplicationNewCallback)(
	SG_IObject* sgobject,
	void*	clientobj,
	void*	clientinfo
);

typedef void* (*SG_DestructionNewCallback)(
	SG_IObject* sgobject,
	void*	clientobj,
	void*	clientinfo
);

typedef void  (*SG_UpdateTransformCallback)(
	SG_IObject* sgobject,
	void*	clientobj,
	void*	clientinfo
);


/**
 * SG_Callbacks hold 2 call backs to the outside world.
 * The first is meant to be called when objects are replicated.
 * And allows the outside world to syncronise external objects
 * with replicated nodes and their children.
 * The second is called when a node is detroyed and again
 * is their for synconisation purposes
 * These callbacks may both be NULL. 
 * The efficacy of this approach has not been proved some 
 * alternatives might be to perform all replication and destruction
 * externally. 
 * To define a class interface rather than a simple function
 * call back so that replication information can be transmitted from 
 * parent->child. 
 */
struct	SG_Callbacks
{
	SG_Callbacks(
	):
		m_replicafunc(NULL),
		m_destructionfunc(NULL),
		m_updatefunc(NULL)
	{
	};
		
	SG_Callbacks(
		SG_ReplicationNewCallback repfunc,
		SG_DestructionNewCallback destructfunc,
		SG_UpdateTransformCallback updatefunc
	): 
		m_replicafunc(repfunc),
		m_destructionfunc(destructfunc),
		m_updatefunc(updatefunc)
	{
	};

	SG_ReplicationNewCallback	m_replicafunc;
	SG_DestructionNewCallback	m_destructionfunc;
	SG_UpdateTransformCallback	m_updatefunc;
};

/**
base object that can be part of the scenegraph.
*/
class SG_IObject
{
private :

	void*	m_SGclientObject;
	void*	m_SGclientInfo;
	SG_Callbacks m_callbacks;
	SGControllerList	m_SGcontrollers;

public:


	virtual ~SG_IObject();


	/**
	 * Add a pointer to a controller allocated on the heap, to 
	 * this node. This memory for this controller becomes the 
	 * responsibility of this class. It will be deleted when
	 * this object is deleted.
	 */
	
		void				
	AddSGController(
		SG_Controller* cont
	);

	/** 
	 * Clear the array of pointers to controllers associated with 
	 * this node. This does not delete the controllers themselves!
     * This should be used very carefully to avoid memory
	 * leaks.
	 */
	
		void				
	RemoveAllControllers(
	); 

	/// Needed for replication

	/** 
	 * Return a reference to this node's controller list. 
	 * Whilst we don't wish to expose full control of the container
	 * to the user we do allow them to call non_const methods
	 * on pointers in the container. C++ topic: how to do this in
	 * using STL? 
	 */

		SGControllerList&	
	GetSGControllerList(
	);

	
	/**
	 * Get the client object associated with this
	 * node. This interface allows you to associate
	 * arbitray external objects with this node. They are
	 * passed to the callback functions when they are 
	 * activated so you can syncronise these external objects
	 * upon replication and destruction
	 * This may be NULL.
	 */

		void*				
	GetSGClientObject(
	);

	const 
		void*			
	GetSGClientObject(
	) const	;

	
	/**
	 * Set the client object for this node. This is just a 
	 * pointer to an object allocated that should exist for 
	 * the duration of the lifetime of this object, or untill
	 * this function is called again.
	 */
	
		void	
	SetSGClientObject(
		void* clientObject
	);

	/** 
	 * Set the current simulation time for this node.
	 * The implementation of this function runs through
	 * the nodes list of controllers and calls their SetSimulatedTime methods
	 */
 
		void		
	SetControllerTime(
		double time
	);
	
	virtual 
		void		
	Destruct(
	) = 0;

protected :

		bool
	ActivateReplicationCallback(
		SG_IObject *replica
	);

		void
	ActivateDestructionCallback(
	);
	
		void
	ActivateUpdateTransformCallback(
	);

	SG_IObject(
		void* clientobj,
		void* clientinfo,
		SG_Callbacks callbacks
	);

	SG_IObject(
		const SG_IObject &other
	);


};

#endif //__SG_IOBJECT

