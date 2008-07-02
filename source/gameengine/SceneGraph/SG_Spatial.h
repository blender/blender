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

#ifndef __SG_SPATIAL_H
#define __SG_SPATIAL_H

#include <MT_Vector3.h>
#include <MT_Point3.h>
#include <MT_Matrix3x3.h> // or Quaternion later ?
#include "SG_IObject.h"
#include "SG_BBox.h"


class SG_Node;
class SG_ParentRelation;

/**
 * SG_Spatial contains spatial information (local & world position, rotation 
 * and scaling) for a Scene graph node.
 * It also contains a link to the node's parent.
 */
class SG_Spatial : public SG_IObject
{

protected:
	MT_Point3		m_localPosition;
	MT_Matrix3x3		m_localRotation;
	MT_Vector3		m_localScaling;

	MT_Point3		m_worldPosition;
	MT_Matrix3x3		m_worldRotation;
	MT_Vector3		m_worldScaling;
	
	SG_ParentRelation *	m_parent_relation;
	
	SG_BBox			m_bbox;
	MT_Scalar		m_radius;
	

public:

	/** 
	 * Define the realtionship this node has with it's parent
	 * node. You should pass an unshared instance of an SG_ParentRelation
	 * allocated on the heap to this method. Ownership of this
	 * instance is assumed by this class. 
	 * You may call this function several times in the lifetime 
	 * of a node to change the relationship dynamically. 
	 * You must call this method before the first call to UpdateSpatialData().
	 * An assertion willl be fired at run-time in debug if this is not 
	 * the case.
	 * The relation is activated only if no controllers of this object
	 * updated the coordinates of the child.
	 */

		void
	SetParentRelation(
		SG_ParentRelation *relation
	);
	
		SG_ParentRelation *
	GetParentRelation(
	);


	/**
	 * Apply a translation relative to the current position.
	 * if local then the translation is assumed to be in the 
	 * local coordinates of this object. If not then the translation
	 * is assumed to be in global coordinates. In this case 
	 * you must provide a pointer to the parent of this object if it 
	 * exists otherwise if there is no parent set it to NULL
	 */ 

		void
	RelativeTranslate(
		const MT_Vector3& trans,
		const SG_Spatial *parent,
		bool local
	);

		void				
	SetLocalPosition(
		const MT_Point3& trans
	);

		void				
	SetWorldPosition(
		const MT_Point3& trans
	);
	
		void				
	RelativeRotate(
		const MT_Matrix3x3& rot,
		bool local
	);

		void				
	SetLocalOrientation(
		const MT_Matrix3x3& rot
	);

		void				
	SetWorldOrientation(
		const MT_Matrix3x3& rot
	);

		void				
	RelativeScale(
		const MT_Vector3& scale
	);

		void				
	SetLocalScale(
		const MT_Vector3& scale
	);

		void				
	SetWorldScale(
		const MT_Vector3& scale
	);

	const 
		MT_Point3&
	GetLocalPosition(
	) const	;

	const 
		MT_Matrix3x3&
	GetLocalOrientation(
	) const	;

	const 
		MT_Vector3&	
	GetLocalScale(
	) const;

	const 
		MT_Point3&
	GetWorldPosition(
	) const	;

	const 
		MT_Matrix3x3&	
	GetWorldOrientation(
	) const	;

	const 
		MT_Vector3&	
	GetWorldScaling(
	) const	;

	MT_Transform GetWorldTransform() const;

	bool	ComputeWorldTransforms(		const SG_Spatial *parent);

	/**
	 * Bounding box functions.
	 */
	SG_BBox& BBox();
	void SetBBox(SG_BBox & bbox);
	bool inside(const MT_Point3 &point) const;
	void getBBox(MT_Point3 *box) const;
	void getAABBox(MT_Point3 *box) const;
	
	MT_Scalar Radius() const { return m_radius; }
	void SetRadius(MT_Scalar radius) { m_radius = radius; }
	
protected:
	friend class SG_Controller;
	
	/** 
	 * Protected constructor this class is not
	 * designed for direct instantiation
	 */

	SG_Spatial(
		void* clientobj,
		void* clientinfo,
		SG_Callbacks callbacks
	);

	SG_Spatial(
		const SG_Spatial& other
	);


	virtual ~SG_Spatial();

	/** 
	 * Update the world coordinates of this spatial node. This also informs
	 * any controllers to update this object. 
	 */ 

		bool 
	UpdateSpatialData(
		const SG_Spatial *parent,
		double time
	);

};

#endif //__SG_SPATIAL_H

