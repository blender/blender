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
#ifndef __SG_NODE_H
#define __SG_NODE_H

#include "SG_Spatial.h"
#include <vector>

typedef std::vector<SG_Node*> NodeList;

/**
 * Scenegraph node.
 */
class SG_Node : public SG_Spatial
{
public:
	SG_Node(
		void* clientobj,
		void* clientinfo,
		SG_Callbacks& callbacks
	);

	SG_Node(
		const SG_Node & other
	);

	virtual ~SG_Node();


	/**
	 * Add a child to this object. This also informs the child of
	 * it's parent.
	 * This just stores a pointer to the child and does not
	 * make a deep copy.
	 */

		void	
	AddChild(
		SG_Node* child
	);

	/** 
	 * Remove a child node from this object. This just removes the child
	 * pointer from the list of children - it does not destroy the child.
	 * This does not inform the child that this node is no longer it's parent.
	 * If the node was not a child of this object no action is performed.
	 */

		void	
	RemoveChild(
		SG_Node* child
	);

	/** 
	 * Get the current list of children. Do not use this interface for
	 * adding or removing children please use the methods of this class for
	 * that.
	 * @return a reference to the list of children of this node.
	 */
	
	NodeList& GetSGChildren()
	{
		return this->m_children;
	}

	/**
	 * Get the current list of children.
	 * @return a const reference to the current list of children of this node.
	 */

	const NodeList& GetSGChildren() const
	{
		return this->m_children;
	}

	/** 
	 * Clear the list of children associated with this node
	 */

	void ClearSGChildren()
	{
		m_children.clear();
	}

	/**
	 * return the parent of this node if it exists.
	 */
		
	SG_Node* GetSGParent() const 
	{ 
		return m_SGparent;
	}

	/**
	 * Set the parent of this node. 
	 */

	void SetSGParent(SG_Node* parent)
	{
		m_SGparent = parent;
	}

	/**
	 * Return the top node in this node's Scene graph hierarchy
	 */
	
	const 
		SG_Node* 
	GetRootSGParent(
	) const;

	/**
	 * Disconnect this node from it's parent
	 */

		void				
	DisconnectFromParent(
	);

	/**
	 * Return vertex parent status.
	 */
	bool IsVertexParent()
	{
		if (m_parent_relation)
		{
			return m_parent_relation->IsVertexRelation();
		}
		return false;
	}


	/**
	 * Return slow parent status.
	 */

	bool IsSlowParent()
	{
		if (m_parent_relation)
		{
			return m_parent_relation->IsSlowRelation();
		}
		return false;
	}




	/**		
	 * Update the spatial data of this node. Iterate through
	 * the children of this node and update their world data.
	 */

		void		
	UpdateWorldData(
		double time,
		bool parentUpdated=false
	);

	/**
	 * Update the simulation time of this node. Iterate through
	 * the children nodes and update their simulated time.
	 */

		void		
	SetSimulatedTime(
		double time,
		bool recurse
	);

	/**
	 * Schedule this node for update by placing it in head queue
	 */
	bool Schedule(SG_QList& head)
	{
		// Put top parent in front of list to make sure they are updated before their
		// children => the children will be udpated and removed from the list before
		// we get to them, should they be in the list too.
		return (m_SGparent)?head.AddBack(this):head.AddFront(this);
	}

	/**
	 * Used during Scenegraph update
	 */
	static SG_Node* GetNextScheduled(SG_QList& head)
	{
		return static_cast<SG_Node*>(head.Remove());
	}

	/**
	 * Make this node ready for schedule on next update. This is needed for nodes
	 * that must always be updated (slow parent, bone parent)
	 */
	bool Reschedule(SG_QList& head)
	{
		return head.QAddBack(this);
	}

	/**
	 * Used during Scenegraph update
	 */
	static SG_Node* GetNextRescheduled(SG_QList& head)
	{
		return static_cast<SG_Node*>(head.QRemove());
	}

	/**
	 * Node replication functions.
	 */

		SG_Node*	
	GetSGReplica(
	);

		void		
	Destruct(
	);
	
private:

		void		
	ProcessSGReplica(
		SG_Node** replica
	);

	/**
	 * The list of children of this node.
	 */
	NodeList m_children;

	/**
	 * The parent of this node may be NULL
	 */
	SG_Node* m_SGparent;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SG_Node"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__SG_NODE_H

