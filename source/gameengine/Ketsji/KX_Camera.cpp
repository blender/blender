/*
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Camera in the gameengine. Cameras are also used for views.
 */

#include "KX_Camera.h"

KX_Camera::KX_Camera(void* sgReplicationInfo,
					 SG_Callbacks callbacks,
					 const RAS_CameraData& camdata)
					:
					KX_GameObject(sgReplicationInfo,callbacks)
{
	// setting a name would be nice...
	m_name = "cam";
	m_camdata = camdata;
	SetProperty("camera",new CIntValue(1));
}



KX_Camera::~KX_Camera()
{
}	


	
MT_Transform KX_Camera::GetWorldToCamera() const
{ 
	MT_Transform camtrans;
	MT_Transform trans;
	
	trans.setBasis(NodeGetWorldOrientation());
	trans.setOrigin(NodeGetWorldPosition());

	camtrans.invert(trans);
	
	return camtrans;
}


	 
MT_Transform KX_Camera::GetCameraToWorld() const
{
	MT_Transform trans;
	trans.setBasis(NodeGetWorldOrientation());
	trans.setOrigin(NodeGetWorldPosition());
	
	return trans;
}



void KX_Camera::CorrectLookUp(MT_Scalar speed)
{
}



const MT_Point3 KX_Camera::GetCameraLocation()
{
	/* this is the camera locatio in cam coords... */
	//return m_trans1.getOrigin();
	//return MT_Point3(0,0,0);   <-----
	/* .... I want it in world coords */
	MT_Transform trans;
	trans.setBasis(NodeGetWorldOrientation());
	
	return NodeGetWorldPosition();		
}



/* I want the camera orientation as well. */
const MT_Quaternion KX_Camera::GetCameraOrientation()
{
	MT_Transform trans;
	trans.setBasis(NodeGetWorldOrientation());
	trans.setOrigin(NodeGetWorldPosition());

	return trans.getRotation();
}



/**
* Sets the projection matrix that is used by the rasterizer.
*/
void KX_Camera::SetProjectionMatrix(const MT_Matrix4x4 & mat)
{
	m_projection_matrix = mat;
}



/**
* Sets the modelview matrix that is used by the rasterizer.
*/
void KX_Camera::SetModelviewMatrix(const MT_Matrix4x4 & mat)
{
	m_modelview_matrix = mat;
}



/**
* Gets the projection matrix that is used by the rasterizer.
*/
void KX_Camera::GetProjectionMatrix(MT_Matrix4x4 & mat)
{
	mat = m_projection_matrix;
}



/**
* Gets the modelview matrix that is used by the rasterizer.
*/
void KX_Camera::GetModelviewMatrix(MT_Matrix4x4 & mat)
{
	mat = m_modelview_matrix;
}



/*
* These getters retrieve the clip data and the focal length
*/
float KX_Camera::GetLens()
{
	return m_camdata.m_lens;
}



float KX_Camera::GetCameraNear()
{
	return m_camdata.m_clipstart;
}



float KX_Camera::GetCameraFar()
{
	return m_camdata.m_clipend;
}



RAS_CameraData*	KX_Camera::GetCameraData()
{
	return &m_camdata; 
}
