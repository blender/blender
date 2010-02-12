/**
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "SG_Node.h"
#include "SG_ParentRelation.h"
#include <algorithm>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

using namespace std;


SG_Node::SG_Node(
	void* clientobj,
	void* clientinfo,
	SG_Callbacks& callbacks

)
	: SG_Spatial(clientobj,clientinfo,callbacks),
	m_SGparent(NULL)
{
	m_modified = true;
}

SG_Node::SG_Node(
	const SG_Node & other
) :
	SG_Spatial(other),
	m_children(other.m_children),
	m_SGparent(other.m_SGparent)
{
	m_modified = true;
}

SG_Node::~SG_Node()
{
}


SG_Node* SG_Node::GetSGReplica()
{
	SG_Node* replica = new SG_Node(*this);
	if (replica == NULL) return NULL;

	ProcessSGReplica(&replica);
	
	return replica;
}

	void 
SG_Node::
ProcessSGReplica(
	SG_Node** replica
){
	// Apply the replication call back function.
	if (!ActivateReplicationCallback(*replica)) 
	{
		delete (*replica);
		*replica = NULL;
		return;
	}

	// clear the replica node of it's parent.
	static_cast<SG_Node*>(*replica)->m_SGparent = NULL;

	if (m_children.begin() != m_children.end())
	{
		// if this node has children, the replica has too, so clear and clone children
		(*replica)->ClearSGChildren();
	
		NodeList::iterator childit;
		for (childit = m_children.begin();childit!=m_children.end();++childit)
		{
			SG_Node* childnode = (*childit)->GetSGReplica();
			if (childnode)
				(*replica)->AddChild(childnode);
		}
	}
	// Nodes without children and without client object are
	// not worth to keep, they will just take up CPU
	// This can happen in partial replication of hierarchy
	// during group duplication.
	if ((*replica)->m_children.empty() && 
		(*replica)->GetSGClientObject() == NULL)
	{
		delete (*replica);
		*replica = NULL;
	}
}


	void 
SG_Node::
Destruct()
{
	// Not entirely sure what Destruct() expects to happen.
	// I think it probably means just to call the DestructionCallback
	// in the right order on all the children - rather than free any memory
	
	// We'll delete m_parent_relation now anyway.
	
	delete(m_parent_relation);
	m_parent_relation = NULL;		

 	if (m_children.begin() != m_children.end())
	{
		NodeList::iterator childit;
		for (childit = m_children.begin();childit!=m_children.end();++childit)
		{
			// call the SG_Node destruct method on each of our children }-)
			(*childit)->Destruct();
		}
	}

	ActivateDestructionCallback();
}

const 
	SG_Node*	
SG_Node::
GetRootSGParent(
) const {
	return (m_SGparent ? (const SG_Node*) m_SGparent->GetRootSGParent() : (const SG_Node*) this);
}

	void 
SG_Node::
DisconnectFromParent(
){
	if (m_SGparent)
	{
		m_SGparent->RemoveChild(this);
		m_SGparent = NULL;
	}

}

void SG_Node::AddChild(SG_Node* child)
{
	m_children.push_back(child);
	child->SetSGParent(this); // this way ?
}

void SG_Node::RemoveChild(SG_Node* child)
{
	NodeList::iterator childfound = find(m_children.begin(),m_children.end(),child);

	if (childfound != m_children.end())
	{
		m_children.erase(childfound);
	}
}



void SG_Node::UpdateWorldData(double time, bool parentUpdated)
{
	//if (!GetSGParent())
	//	return;

	if (UpdateSpatialData(GetSGParent(),time,parentUpdated))
		// to update the 
		ActivateUpdateTransformCallback();

	// The node is updated, remove it from the update list
	Delink();

	// update children's worlddata
	for (NodeList::iterator it = m_children.begin();it!=m_children.end();++it)
	{
		(*it)->UpdateWorldData(time, parentUpdated);
	}
}



void SG_Node::SetSimulatedTime(double time,bool recurse)
{

	// update the controllers of this node.
	SetControllerTime(time);

	// update children's simulate time.
	if (recurse)
	{
		for (NodeList::iterator it = m_children.begin();it!=m_children.end();++it)
		{
			(*it)->SetSimulatedTime(time,recurse);
		}
	}
}



