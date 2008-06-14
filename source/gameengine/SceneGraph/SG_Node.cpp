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
	SG_Callbacks callbacks

)
	: SG_Spatial(clientobj,clientinfo,callbacks),
	m_SGparent(NULL)
{
}

SG_Node::SG_Node(
	const SG_Node & other
) :
	SG_Spatial(other),
	m_children(other.m_children),
	m_SGparent(other.m_SGparent)
{
	// nothing to do
}

SG_Node::~SG_Node()
{
}


SG_Node* SG_Node::GetSGReplica()
{
	SG_Node* replica = new SG_Node(*this);
	if (replica == NULL) return NULL;

	ProcessSGReplica(replica);
	
	return replica;
}

	void 
SG_Node::
ProcessSGReplica(
	SG_Node* replica
){
	// Apply the replication call back function.
	ActivateReplicationCallback(replica);

	// clear the replica node of it's parent.
	static_cast<SG_Node*>(replica)->m_SGparent = NULL;

	if (m_children.begin() != m_children.end())
	{
		// if this node has children, the replica has too, so clear and clone children
		replica->ClearSGChildren();
	
		NodeList::iterator childit;
		for (childit = m_children.begin();childit!=m_children.end();++childit)
		{
			replica->AddChild((*childit)->GetSGReplica());
		}
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


	SG_Node*			
SG_Node::
GetSGParent(
) const { 
	return m_SGparent;
}

	void				
SG_Node::
SetSGParent(
	SG_Node* parent
){
	m_SGparent = parent;
}

const 
	SG_Node*	
SG_Node::
GetRootSGParent(
) const {
	return (m_SGparent ? (const SG_Node*) m_SGparent->GetRootSGParent() : (const SG_Node*) this);
}

	bool
SG_Node::
IsVertexParent()
{
	if (m_parent_relation)
	{
		return m_parent_relation->IsVertexRelation();
	}
	return false;
}

	bool
SG_Node::
IsSlowParent()
{
	if (m_parent_relation)
	{
		return m_parent_relation->IsSlowRelation();
	}
	return false;
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



void SG_Node::UpdateWorldData(double time)
{
	//if (!GetSGParent())
	//	return;

	if (UpdateSpatialData(GetSGParent(),time))
		ActivateUpdateTransformCallback();

	// update children's worlddata
	for (NodeList::iterator it = m_children.begin();it!=m_children.end();++it)
	{
		(*it)->UpdateWorldData(time);
	}
}


NodeList& SG_Node::GetSGChildren()
{
	return this->m_children;
}


const NodeList& SG_Node::GetSGChildren() const
{
	return this->m_children;
}


void SG_Node::ClearSGChildren()
{
	m_children.clear();
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



