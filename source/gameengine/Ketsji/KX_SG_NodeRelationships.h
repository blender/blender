/*
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
 
/** \file KX_SG_NodeRelationships.h
 *  \ingroup ketsji
 *  \section KX_SG_NodeRelationships   
 * This file provides common concrete implementations of 
 * SG_ParentRelation used by the game engine. These are
 * KX_SlowParentRelation a slow parent relationship.
 * KX_NormalParentRelation a normal parent relationship where 
 * orientation and position are inherited from the parent by
 * the child.
 * KX_VertexParentRelation only location information is 
 * inherited by the child. 
 */

#ifndef __KX_SG_NODERELATIONSHIPS_H__
#define __KX_SG_NODERELATIONSHIPS_H__

#include "SG_Spatial.h"
#include "SG_ParentRelation.h"

class KX_NormalParentRelation : public SG_ParentRelation
{

public :
	/**
	 * Allocate and construct a new KX_NormalParentRelation
	 * on the heap.
	 */

	static 
		KX_NormalParentRelation *
	New(
	);		

	/** 
	 * Method inherited from KX_ParentRelation
	 */

		bool
	UpdateChildCoordinates(
		SG_Spatial * child,
		const SG_Spatial * parent,
		bool& parentUpdated	
	);

	/** 
	 * Method inherited from KX_ParentRelation
	 */
	
		SG_ParentRelation *
	NewCopy(
	);

	~KX_NormalParentRelation(
	);

private :

	KX_NormalParentRelation(
	);


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_NormalParentRelation"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};


class KX_VertexParentRelation : public SG_ParentRelation
{

public :

	/**
	 * Allocate and construct a new KX_VertexParentRelation
	 * on the heap.
	 */

	static 
		KX_VertexParentRelation *
	New(
	);		

	/** 
	 * Method inherited from KX_ParentRelation
	 */

		bool
	UpdateChildCoordinates(
		SG_Spatial * child,
		const SG_Spatial * parent,
		bool& parentUpdated	
	);

	/** 
	 * Method inherited from KX_ParentRelation
	 */
	
		SG_ParentRelation *
	NewCopy(
	);

	~KX_VertexParentRelation(
	);

		bool
	IsVertexRelation(
	) { 
		return true;
	}

private :

	KX_VertexParentRelation(
	);


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_VertexParentRelation"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};


class KX_SlowParentRelation : public SG_ParentRelation
{

public :

	/**
	 * Allocate and construct a new KX_VertexParentRelation
	 * on the heap.
	 */

	static 
		KX_SlowParentRelation *
	New(
		MT_Scalar relaxation
	);		

	/** 
	 * Method inherited from KX_ParentRelation
	 */

		bool
	UpdateChildCoordinates(
		SG_Spatial * child,
		const SG_Spatial * parent,
		bool& parentUpdated	
	);

	/** 
	 * Method inherited from KX_ParentRelation
	 */
	
		SG_ParentRelation *
	NewCopy(
	);

		MT_Scalar
	GetTimeOffset(
	) { return m_relax; }
	
		void
	SetTimeOffset(
		MT_Scalar relaxation
	) { m_relax = relaxation; }

	~KX_SlowParentRelation(
	);
	
		bool
	IsSlowRelation(
	) { 
		return true;
	}

private :

	KX_SlowParentRelation(
		MT_Scalar relaxation
	);

	// the relaxation coefficient.

	MT_Scalar m_relax;

	/**
	 * Looks like a hack flag to me.
	 * We need to compute valid world coordinates the first
	 * time we update spatial data of the child. This is done
	 * by just doing a normal parent relation the first time
	 * UpdateChildCoordinates is called and then doing the
	 * slow parent relation 
	 */

	bool m_initialized;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_SlowParentRelation"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif
