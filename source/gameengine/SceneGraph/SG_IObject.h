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

#include "SG_QList.h"
#include <vector>

// used for debugging: stage of the game engine main loop at which a Scenegraph modification is done
enum SG_Stage
{
	SG_STAGE_UNKNOWN = 0,
	SG_STAGE_NETWORK,
	SG_STAGE_NETWORK_UPDATE,
	SG_STAGE_PHYSICS1,
	SG_STAGE_PHYSICS1_UPDATE,
	SG_STAGE_CONTROLLER,
	SG_STAGE_CONTROLLER_UPDATE,
	SG_STAGE_ACTUATOR,
	SG_STAGE_ACTUATOR_UPDATE,
	SG_STAGE_PHYSICS2,
	SG_STAGE_PHYSICS2_UPDATE,
	SG_STAGE_SCENE,
	SG_STAGE_RENDER,
	SG_STAGE_CONVERTER,
	SG_STAGE_CULLING,
	SG_STAGE_MAX
};

extern SG_Stage gSG_Stage;

inline void SG_SetActiveStage(SG_Stage stage)
{
	gSG_Stage = stage;
}
	


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

typedef bool  (*SG_ScheduleUpdateCallback)(
	SG_IObject* sgobject,
	void*	clientobj,
	void*	clientinfo
);

typedef bool  (*SG_RescheduleUpdateCallback)(
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
		m_updatefunc(NULL),
		m_schedulefunc(NULL),
		m_reschedulefunc(NULL)
	{
	};
		
	SG_Callbacks(
		SG_ReplicationNewCallback repfunc,
		SG_DestructionNewCallback destructfunc,
		SG_UpdateTransformCallback updatefunc,
		SG_ScheduleUpdateCallback schedulefunc,
		SG_RescheduleUpdateCallback reschedulefunc
	): 
		m_replicafunc(repfunc),
		m_destructionfunc(destructfunc),
		m_updatefunc(updatefunc),
		m_schedulefunc(schedulefunc),
		m_reschedulefunc(reschedulefunc)
	{
	};

	SG_ReplicationNewCallback	m_replicafunc;
	SG_DestructionNewCallback	m_destructionfunc;
	SG_UpdateTransformCallback	m_updatefunc;
	SG_ScheduleUpdateCallback	m_schedulefunc;
	SG_RescheduleUpdateCallback m_reschedulefunc;
};

/**
base object that can be part of the scenegraph.
*/
class SG_IObject : public SG_QList
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

	SGControllerList& GetSGControllerList()
	{ 
		return m_SGcontrollers; 
	}

	/**
	 * 
	 */
	SG_Callbacks& GetCallBackFunctions()
	{
		return m_callbacks;
	}
	
	/**
	 * Get the client object associated with this
	 * node. This interface allows you to associate
	 * arbitray external objects with this node. They are
	 * passed to the callback functions when they are 
	 * activated so you can syncronise these external objects
	 * upon replication and destruction
	 * This may be NULL.
	 */

	inline const void* GetSGClientObject() const	
	{
		return m_SGclientObject;
	}

	inline void* GetSGClientObject()
	{ 
		return m_SGclientObject;
	}

	/**
	 * Set the client object for this node. This is just a 
	 * pointer to an object allocated that should exist for 
	 * the duration of the lifetime of this object, or untill
	 * this function is called again.
	 */
	
	void SetSGClientObject(void* clientObject)
	{
		m_SGclientObject = clientObject;
	}

	/** 
	 * Set the current simulation time for this node.
	 * The implementation of this function runs through
	 * the nodes list of controllers and calls their SetSimulatedTime methods
	 */
 
	void SetControllerTime(double time);
	
	virtual 
		void		
	Destruct(
	) = 0;

protected :

		bool
	ActivateReplicationCallback(
		SG_IObject *replica
	)
	{
		if (m_callbacks.m_replicafunc)
		{
			// Call client provided replication func
			if (m_callbacks.m_replicafunc(replica,m_SGclientObject,m_SGclientInfo) == NULL)
				return false;
		}
		return true;
	}


		void
	ActivateDestructionCallback(
	)
	{
		if (m_callbacks.m_destructionfunc)
		{
			// Call client provided destruction function on this!
			m_callbacks.m_destructionfunc(this,m_SGclientObject,m_SGclientInfo);
		}
		else
		{
			// no callback but must still destroy the node to avoid memory leak
			delete this;
		}
	}
	
		void
	ActivateUpdateTransformCallback(
	)
	{
		if (m_callbacks.m_updatefunc)
		{
			// Call client provided update func.
			m_callbacks.m_updatefunc(this, m_SGclientObject, m_SGclientInfo);
		}
	}

		bool
	ActivateScheduleUpdateCallback(
	)
	{
		// HACK, this check assumes that the scheduled nodes are put on a DList (see SG_Node.h)
		// The early check on Empty() allows up to avoid calling the callback function
		// when the node is already scheduled for update.
		if (Empty() && m_callbacks.m_schedulefunc)
		{
			// Call client provided update func.
			return m_callbacks.m_schedulefunc(this, m_SGclientObject, m_SGclientInfo);
		}
		return false;
	}

		void
	ActivateRecheduleUpdateCallback(
	)
	{
		if (m_callbacks.m_reschedulefunc)
		{
			// Call client provided update func.
			m_callbacks.m_reschedulefunc(this, m_SGclientObject, m_SGclientInfo);
		}
	}


	SG_IObject(
		void* clientobj,
		void* clientinfo,
		SG_Callbacks& callbacks
	);

	SG_IObject(
		const SG_IObject &other
	);


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SG_IObject"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__SG_IOBJECT

