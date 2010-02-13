/**
 * @mainpage KX_SG_NodeRelationships   

 * @section 
 *
 * This file provides common concrete implementations of 
 * SG_ParentRelation used by the game engine. These are
 * KX_SlowParentRelation a slow parent relationship.
 * KX_NormalParentRelation a normal parent relationship where 
 * orientation and position are inherited from the parent by
 * the child.
 * KX_VertexParentRelation only location information is 
 * inherited by the child. 
 *
 * interface	  
 *
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
 * 
 */

#ifndef __KX_SG_BONEPARENTRELATION_H__
#define __KX_SG_BONEPARENTRELATION_H__
 
#include "SG_Spatial.h"
#include "SG_ParentRelation.h"

struct Bone;

/**
 *  Bone parent relationship parents a child SG_Spatial frame to a 
 *  bone in an armature object.
 */
class KX_BoneParentRelation : public SG_ParentRelation
{

public :
	/**
	 * Allocate and construct a new KX_SG_BoneParentRelation
	 * on the heap.
	 *
	 * bone is the bone id to use.  Currently it is a pointer
	 * to a Blender struct Bone - this should be fixed if
	 */

	static 
		KX_BoneParentRelation *
	New(Bone* bone
	);		

	/**
	 *  Updates the childs world coordinates relative to the parent's
	 *  world coordinates.
	 *
	 *  Parent should be a BL_ArmatureObject.
	 */
		bool
	UpdateChildCoordinates(
		SG_Spatial * child,
		const SG_Spatial * parent,
		bool& parentUpdated
	);

	/**
	 *  Create a copy of this relationship
	 */
		SG_ParentRelation *
	NewCopy(
	);

	~KX_BoneParentRelation(
	);

private :
	Bone* m_bone;
	KX_BoneParentRelation(Bone* bone
	);


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_BoneParentRelation"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif
