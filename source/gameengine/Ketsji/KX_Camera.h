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
 */

/** \file KX_Camera.h
 *  \ingroup ketsji
 *  \brief Camera in the gameengine. Cameras are also used for views.
 */

#ifndef __KX_CAMERA_H__
#define __KX_CAMERA_H__


#include "MT_Transform.h"
#include "MT_Matrix3x3.h"
#include "MT_Matrix4x4.h"
#include "MT_Vector3.h"
#include "MT_Point3.h"
#include "KX_GameObject.h"
#include "IntValue.h"
#include "RAS_CameraData.h"

#ifdef WITH_PYTHON
/* utility conversion function */
bool ConvertPythonToCamera(PyObject * value, KX_Camera **object, bool py_none_ok, const char *error_prefix);
#endif

class KX_Camera : public KX_GameObject
{
	Py_Header
protected:
	friend class KX_Scene;
	/** Camera parameters (clips distances, focal length). These
	 * params are closely tied to Blender. In the gameengine, only the
	 * projection and modelview matrices are relevant. There's a
	 * conversion being done in the engine class. Why is it stored
	 * here? It doesn't really have a function here. */
	RAS_CameraData	m_camdata;

	// Never used, I think...
//	void MoveTo(const MT_Point3& movevec)
//	{
		/*MT_Transform camtrans;
		camtrans.invert(m_trans1);
		MT_Matrix3x3 camorient = camtrans.getBasis();
		camtrans.translate(camorient.inverse()*movevec);
		m_trans1.invert(camtrans);
		*/
//	}

	/**
	 * Storage for the projection matrix that is passed to the
	 * rasterizer. */
	MT_Matrix4x4 m_projection_matrix;
	//MT_Matrix4x4 m_projection_matrix1;

	/**
	 * Storage for the modelview matrix that is passed to the
	 * rasterizer. */
	MT_Matrix4x4 m_modelview_matrix;
	
	/**
	 * true if the view frustum (modelview/projection matrix)
	 * has changed - the clip planes (m_planes) will have to be
	 * regenerated.
	 */
	bool         m_dirty;
	/**
	 * true if the frustum planes have been normalized.
	 */
	bool         m_normalized;
	
	/**
	 * View Frustum clip planes.
	 */
	MT_Vector4   m_planes[6];
	
	/**
	 * This camera is frustum culling.
	 * Some cameras (ie if the game was started from a non camera view should not cull.)
	 */
	bool         m_frustum_culling;
	
	/**
	 * true if this camera has a valid projection matrix.
	 */
	bool         m_set_projection_matrix;
	
	/**
	 * The center point of the frustum.
	 */
	MT_Point3    m_frustum_center;
	MT_Scalar    m_frustum_radius;
	bool         m_set_frustum_center;

	/**
	 * whether the camera should delete the node itself (only for shadow camera)
	 */
	bool		 m_delete_node;

	/**
	 * Extracts the camera clip frames from the projection and world-to-camera matrices.
	 */
	void ExtractClipPlanes();
	/**
	 * Normalize the camera clip frames.
	 */
	void NormalizeClipPlanes();
	/**
	 * Extracts the bound sphere of the view frustum.
	 */
	void ExtractFrustumSphere();
	/**
	 * return the clip plane
	 */
	MT_Vector4 *GetNormalizedClipPlanes()
	{
		ExtractClipPlanes();
		NormalizeClipPlanes();
		return m_planes;
	}

public:

	enum { INSIDE, INTERSECT, OUTSIDE };

	KX_Camera(void* sgReplicationInfo,SG_Callbacks callbacks,const RAS_CameraData& camdata, bool frustum_culling = true, bool delete_node = false);
	virtual ~KX_Camera();
	
	/** 
	 * Inherited from CValue -- return a new copy of this
	 * instance allocated on the heap. Ownership of the new 
	 * object belongs with the caller.
	 */
	virtual	CValue*				
	GetReplica(
	);
	virtual void ProcessReplica();

	MT_Transform		GetWorldToCamera() const;
	MT_Transform		GetCameraToWorld() const;

	/**
	 * Not implemented.
	 */
	void				CorrectLookUp(MT_Scalar speed);
	const MT_Point3		GetCameraLocation() const;

	/* I want the camera orientation as well. */
	const MT_Quaternion GetCameraOrientation() const;
		
	/** Sets the projection matrix that is used by the rasterizer. */
	void				SetProjectionMatrix(const MT_Matrix4x4 & mat);

	/** Sets the modelview matrix that is used by the rasterizer. */
	void				SetModelviewMatrix(const MT_Matrix4x4 & mat);
		
	/** Gets the projection matrix that is used by the rasterizer. */
	const MT_Matrix4x4&		GetProjectionMatrix() const;
	
	/** returns true if this camera has been set a projection matrix. */
	bool				hasValidProjectionMatrix() const;
	
	/** Sets the validity of the projection matrix.  Call this if you change camera
	    data (eg lens, near plane, far plane) and require the projection matrix to be
	    recalculated.
	 */
	void				InvalidateProjectionMatrix(bool valid = false);
	
	/** Gets the modelview matrix that is used by the rasterizer. 
	 *  @warning If the Camera is a dynamic object then this method may return garbage.  Use GetCameraToWorld() instead.
	 */
	const MT_Matrix4x4&		GetModelviewMatrix() const;

	/** Gets the aperture. */
	float				GetLens() const;
	/** Gets the ortho scale. */
	float				GetScale() const;
	/** Gets the horizontal size of the sensor - for camera matching */
	float				GetSensorWidth() const;
	/** Gets the vertical size of the sensor - for camera matching */
	float				GetSensorHeight() const;
	/** Gets the mode FOV is calculating from sensor dimensions */
	short				GetSensorFit() const;
	/** Gets the near clip distance. */
	float				GetCameraNear() const;
	/** Gets the far clip distance. */
	float				GetCameraFar() const;
	/** Gets the focal length (only used for stereo rendering) */
	float				GetFocalLength() const;
	/** Gets all camera data. */
	RAS_CameraData*		GetCameraData();
	
	/**
	 * Tests if the given sphere is inside this camera's view frustum.
	 *
	 * @param center The center of the sphere, in world coordinates.
	 * @param radius The radius of the sphere.
	 * @return INSIDE, INTERSECT, or OUTSIDE depending on the sphere's relation to the frustum.
	 */
	int SphereInsideFrustum(const MT_Point3& center, const MT_Scalar &radius);
	/**
	 * Tests the given eight corners of a box with the view frustum.
	 *
	 * @param box a pointer to eight MT_Point3 representing the world coordinates of the corners of the box.
	 * @return INSIDE, INTERSECT, or OUTSIDE depending on the box's relation to the frustum.
	 */
	int BoxInsideFrustum(const MT_Point3 *box);
	/**
	 * Tests the given point against the view frustum.
	 * @return true if the given point is inside or on the view frustum; false if it is outside.
	 */
	bool PointInsideFrustum(const MT_Point3& x);
	
	/**
	 * Gets this camera's culling status.
	 */
	bool GetFrustumCulling() const;
	
	/**
	 * Sets this camera's viewport status.
	 */
	void EnableViewport(bool viewport);
	
	/**
	 * Sets this camera's viewport.
	 */
	void SetViewport(int left, int bottom, int right, int top);
	
	/**
	 * Gets this camera's viewport status.
	 */
	bool GetViewport() const;
	
	/**
	 * Gets this camera's viewport left.
	 */
	int GetViewportLeft() const;
	
	/**
	 * Gets this camera's viewport bottom.
	 */
	int GetViewportBottom() const;
	
	/**
	 * Gets this camera's viewport right.
	 */
	int GetViewportRight() const;
	
	/**
	 * Gets this camera's viewport top.
	 */
	int GetViewportTop() const;

	virtual int GetGameObjectType() { return OBJ_CAMERA; }

#ifdef WITH_PYTHON
	KX_PYMETHOD_DOC_VARARGS(KX_Camera, sphereInsideFrustum);
	KX_PYMETHOD_DOC_O(KX_Camera, boxInsideFrustum);
	KX_PYMETHOD_DOC_O(KX_Camera, pointInsideFrustum);
	
	KX_PYMETHOD_DOC_NOARGS(KX_Camera, getCameraToWorld);
	KX_PYMETHOD_DOC_NOARGS(KX_Camera, getWorldToCamera);
	
	KX_PYMETHOD_DOC_VARARGS(KX_Camera, setViewport);	
	KX_PYMETHOD_DOC_NOARGS(KX_Camera, setOnTop);	

	KX_PYMETHOD_DOC_O(KX_Camera, getScreenPosition);
	KX_PYMETHOD_DOC_VARARGS(KX_Camera, getScreenVect);
	KX_PYMETHOD_DOC_VARARGS(KX_Camera, getScreenRay);
	
	static PyObject*	pyattr_get_perspective(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_perspective(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

	static PyObject*	pyattr_get_lens(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_lens(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_ortho_scale(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_ortho_scale(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_near(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_near(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_far(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_far(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	
	static PyObject*	pyattr_get_use_viewport(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_use_viewport(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	
	static PyObject*	pyattr_get_projection_matrix(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_projection_matrix(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	
	static PyObject*	pyattr_get_modelview_matrix(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_camera_to_world(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_world_to_camera(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	
	static PyObject*	pyattr_get_INSIDE(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_OUTSIDE(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_INTERSECT(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
#endif
};

#endif //__KX_CAMERA_H__

