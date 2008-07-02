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
		SG_Callbacks callbacks
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
	
		NodeList&		
	GetSGChildren(
	);

	/**
	 * Get the current list of children.
	 * @return a const reference to the current list of children of this node.
	 */

	const 
		NodeList&	
	GetSGChildren(
	) const;

	/** 
	 * Clear the list of children associated with this node
	 */

		void				
	ClearSGChildren(
	);

	/**
	 * return the parent of this node if it exists.
	 */
		
		SG_Node*			
	GetSGParent(
	) const ;


	/**
	 * Set the parent of this node. 
	 */

		void				
	SetSGParent(
		SG_Node* parent
	);

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
	 * Tell this node to treat it's parent as a vertex parent.
	 */

		void	
	SetVertexParent(
		bool isvertexparent
	) ;


	/**
	 * Return vertex parent status.
	 */

		bool	
	IsVertexParent(
	) ;
	
	/**
	 * Return slow parent status.
	 */

		bool	
	IsSlowParent(
	) ;

	/**		
	 * Update the spatial data of this node. Iterate through
	 * the children of this node and update their world data.
	 */

		void		
	UpdateWorldData(
		double time
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
		SG_Node* replica
	);

	/**
	 * The list of children of this node.
	 */
	NodeList m_children;

	/**
	 * The parent of this node may be NULL
	 */
	SG_Node* m_SGparent;

};

#endif //__SG_NODE_H

