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

#include "SG_Node.h"
#include "SG_Spatial.h"
#include "SG_Controller.h"
#include "SG_ParentRelation.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SG_Spatial::
SG_Spatial(
	void* clientobj,
	void* clientinfo,
	SG_Callbacks callbacks
): 

	SG_IObject(clientobj,clientinfo,callbacks),
	m_localPosition(MT_Point3(0.0,0.0,0.0)),
	m_localRotation(MT_Matrix3x3(1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)),
	m_localScaling(MT_Vector3(1.f,1.f,1.f)),
	
	m_worldPosition(MT_Point3(0.0,0.0,0.0)),
	m_worldRotation(MT_Matrix3x3(1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)),
	m_worldScaling(MT_Vector3(1.f,1.f,1.f)),

	m_parent_relation (NULL),
	
	m_bbox(MT_Point3(-1.0, -1.0, -1.0), MT_Point3(1.0, 1.0, 1.0)),
	m_radius(1.0)
{
}

SG_Spatial::
SG_Spatial(
	const SG_Spatial& other
) : 
	SG_IObject(other),
	m_localPosition(other.m_localPosition),
	m_localRotation(other.m_localRotation),
	m_localScaling(other.m_localScaling),
	
	m_worldPosition(other.m_worldPosition),
	m_worldRotation(other.m_worldRotation),
	m_worldScaling(other.m_worldScaling),
	
	m_parent_relation(NULL),
	
	m_bbox(other.m_bbox),
	m_radius(other.m_radius)
{
	// duplicate the parent relation for this object
	m_parent_relation = other.m_parent_relation->NewCopy();
}
	
SG_Spatial::
~SG_Spatial()
{
	delete (m_parent_relation);
}

	SG_ParentRelation *
SG_Spatial::
GetParentRelation(
){
	return m_parent_relation;
}

	void
SG_Spatial::
SetParentRelation(
	SG_ParentRelation *relation
){
	delete (m_parent_relation);
	m_parent_relation = relation;
}


/**
 * Update Spatial Data.
 * Calculates WorldTransform., (either doing itsself or using the linked SGControllers)
 */


	bool 
SG_Spatial::
UpdateSpatialData(
	const SG_Spatial *parent,
	double time
){

    bool bComputesWorldTransform = false;

	// update spatial controllers
	
	SGControllerList::iterator cit = GetSGControllerList().begin();
	SGControllerList::const_iterator c_end = GetSGControllerList().end();

	for (;cit!=c_end;++cit)
	{
		if ((*cit)->Update(time))
			bComputesWorldTransform = true;
	}

	// If none of the objects updated our values then we ask the
	// parent_relation object owned by this class to update 
	// our world coordinates.

	if (!bComputesWorldTransform)
		bComputesWorldTransform = ComputeWorldTransforms(parent);

	return bComputesWorldTransform;
}

bool	SG_Spatial::ComputeWorldTransforms(const SG_Spatial *parent)
{
	return m_parent_relation->UpdateChildCoordinates(this,parent);
}

/**
 * Position and translation methods
 */


	void 
SG_Spatial::
RelativeTranslate(
	const MT_Vector3& trans,
	const SG_Spatial *parent,
	bool local
){
	if (local) {
			m_localPosition += m_localRotation * trans;
	} else {
		if (parent) {
			m_localPosition += trans * parent->GetWorldOrientation();
		} else {
			m_localPosition += trans;
		}
	}
}	
	
	void 
SG_Spatial::
SetLocalPosition(
	const MT_Point3& trans
){
	m_localPosition = trans;
}

	void				
SG_Spatial::
SetWorldPosition(
	const MT_Point3& trans
) {
	m_worldPosition = trans;
}

/**
 * Scaling methods.
 */ 

	void 
SG_Spatial::
RelativeScale(
	const MT_Vector3& scale
){
	m_localScaling = m_localScaling * scale;
}

	void 
SG_Spatial::
SetLocalScale(
	const MT_Vector3& scale
){
	m_localScaling = scale;
}


	void				
SG_Spatial::
SetWorldScale(
	const MT_Vector3& scale
){ 
	m_worldScaling = scale;
}

/**
 * Orientation and rotation methods.
 */


	void 
SG_Spatial::
RelativeRotate(
	const MT_Matrix3x3& rot,
	bool local
){
	m_localRotation = m_localRotation * (
	local ? 
		rot 
	:
	(GetWorldOrientation().inverse() * rot * GetWorldOrientation()));
}

	void 
SG_Spatial::
SetLocalOrientation(const MT_Matrix3x3& rot)
{
	m_localRotation = rot;
}



	void				
SG_Spatial::
SetWorldOrientation(
	const MT_Matrix3x3& rot
) {
	m_worldRotation = rot;
}

const 
	MT_Point3&
SG_Spatial::
GetLocalPosition(
) const	{
	 return m_localPosition;
}

const 
	MT_Matrix3x3&
SG_Spatial::
GetLocalOrientation(
) const	{
	return m_localRotation;
}

const 
	MT_Vector3&	
SG_Spatial::
GetLocalScale(
) const{
	return m_localScaling;
}


const 
	MT_Point3&
SG_Spatial::
GetWorldPosition(
) const	{
	return m_worldPosition;
}

const 
	MT_Matrix3x3&	
SG_Spatial::
GetWorldOrientation(
) const	{
	return m_worldRotation;
}

const 
	MT_Vector3&	
SG_Spatial::
GetWorldScaling(
) const	{
	return m_worldScaling;
}

SG_BBox& SG_Spatial::BBox()
{
	return m_bbox;
}

void SG_Spatial::SetBBox(SG_BBox& bbox)
{
	m_bbox = bbox;
}

MT_Transform SG_Spatial::GetWorldTransform() const
{
	return MT_Transform(m_worldPosition, 
		m_worldRotation.scaled(
		m_worldScaling[0], m_worldScaling[1], m_worldScaling[2]));
}

bool SG_Spatial::inside(const MT_Point3 &point) const
{
	MT_Scalar radius = m_worldScaling[m_worldScaling.closestAxis()]*m_radius;
	return (m_worldPosition.distance2(point) <= radius*radius) ?
		m_bbox.transform(GetWorldTransform()).inside(point) :
		false;
}

void SG_Spatial::getBBox(MT_Point3 *box) const
{
	m_bbox.get(box, GetWorldTransform());
}

void SG_Spatial::getAABBox(MT_Point3 *box) const
{
	m_bbox.getaa(box, GetWorldTransform());
}

