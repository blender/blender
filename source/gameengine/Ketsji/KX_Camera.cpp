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
 * Camera in the gameengine. Cameras are also used for views.
 */

/** \file gameengine/Ketsji/KX_Camera.cpp
 *  \ingroup ketsji
 */

 
#include "GL/glew.h"
#include "KX_Camera.h"
#include "KX_Scene.h"
#include "KX_PythonInit.h"
#include "KX_Python.h"
#include "KX_PyMath.h"
KX_Camera::KX_Camera(void* sgReplicationInfo,
					 SG_Callbacks callbacks,
					 const RAS_CameraData& camdata,
					 bool frustum_culling,
					 bool delete_node)
					:
					KX_GameObject(sgReplicationInfo,callbacks),
					m_camdata(camdata),
					m_dirty(true),
					m_normalized(false),
					m_frustum_culling(frustum_culling),
					m_set_projection_matrix(false),
					m_set_frustum_center(false),
					m_delete_node(delete_node)
{
	// setting a name would be nice...
	m_name = "cam";
	m_projection_matrix.setIdentity();
	m_modelview_matrix.setIdentity();
}


KX_Camera::~KX_Camera()
{
	if (m_delete_node && m_pSGNode)
	{
		// for shadow camera, avoids memleak
		delete m_pSGNode;
		m_pSGNode = NULL;
	}
}	


CValue*	KX_Camera::GetReplica()
{
	KX_Camera* replica = new KX_Camera(*this);
	
	// this will copy properties and so on...
	replica->ProcessReplica();
	
	return replica;
}

void KX_Camera::ProcessReplica()
{
	KX_GameObject::ProcessReplica();
	// replicated camera are always registered in the scene
	m_delete_node = false;
}

MT_Transform KX_Camera::GetWorldToCamera() const
{ 
	MT_Transform camtrans;
	camtrans.invert(MT_Transform(NodeGetWorldPosition(), NodeGetWorldOrientation()));
	
	return camtrans;
}


	 
MT_Transform KX_Camera::GetCameraToWorld() const
{
	return MT_Transform(NodeGetWorldPosition(), NodeGetWorldOrientation());
}



void KX_Camera::CorrectLookUp(MT_Scalar speed)
{
}



const MT_Point3 KX_Camera::GetCameraLocation() const
{
	/* this is the camera locatio in cam coords... */
	//return m_trans1.getOrigin();
	//return MT_Point3(0,0,0);   <-----
	/* .... I want it in world coords */
	//MT_Transform trans;
	//trans.setBasis(NodeGetWorldOrientation());
	
	return NodeGetWorldPosition();		
}



/* I want the camera orientation as well. */
const MT_Quaternion KX_Camera::GetCameraOrientation() const
{
	return NodeGetWorldOrientation().getRotation();
}



/**
 * Sets the projection matrix that is used by the rasterizer.
 */
void KX_Camera::SetProjectionMatrix(const MT_Matrix4x4 & mat)
{
	m_projection_matrix = mat;
	m_dirty = true;
	m_set_projection_matrix = true;
	m_set_frustum_center = false;
}



/**
 * Sets the modelview matrix that is used by the rasterizer.
 */
void KX_Camera::SetModelviewMatrix(const MT_Matrix4x4 & mat)
{
	m_modelview_matrix = mat;
	m_dirty = true;
	m_set_frustum_center = false;
}



/**
 * Gets the projection matrix that is used by the rasterizer.
 */
const MT_Matrix4x4& KX_Camera::GetProjectionMatrix() const
{
	return m_projection_matrix;
}



/**
 * Gets the modelview matrix that is used by the rasterizer.
 */
const MT_Matrix4x4& KX_Camera::GetModelviewMatrix() const
{
	return m_modelview_matrix;
}


bool KX_Camera::hasValidProjectionMatrix() const
{
	return m_set_projection_matrix;
}

void KX_Camera::InvalidateProjectionMatrix(bool valid)
{
	m_set_projection_matrix = valid;
}


/**
 * These getters retrieve the clip data and the focal length
 */
float KX_Camera::GetLens() const
{
	return m_camdata.m_lens;
}

float KX_Camera::GetScale() const
{
	return m_camdata.m_scale;
}

/**
 * Gets the horizontal size of the sensor - for camera matching.
 */
float KX_Camera::GetSensorWidth() const
{
	return m_camdata.m_sensor_x;
}

/**
 * Gets the vertical size of the sensor - for camera matching.
 */
float KX_Camera::GetSensorHeight() const
{
	return m_camdata.m_sensor_y;
}
/** Gets the mode FOV is calculating from sensor dimensions */
short KX_Camera::GetSensorFit() const
{
	return m_camdata.m_sensor_fit;
}

float KX_Camera::GetCameraNear() const
{
	return m_camdata.m_clipstart;
}



float KX_Camera::GetCameraFar() const
{
	return m_camdata.m_clipend;
}

float KX_Camera::GetFocalLength() const
{
	return m_camdata.m_focallength;
}



RAS_CameraData*	KX_Camera::GetCameraData()
{
	return &m_camdata; 
}

void KX_Camera::ExtractClipPlanes()
{
	if (!m_dirty)
		return;

	MT_Matrix4x4 m = m_projection_matrix * m_modelview_matrix;
	// Left clip plane
	m_planes[0] = m[3] + m[0];
	// Right clip plane
	m_planes[1] = m[3] - m[0];
	// Top clip plane
	m_planes[2] = m[3] - m[1];
	// Bottom clip plane
	m_planes[3] = m[3] + m[1];
	// Near clip plane
	m_planes[4] = m[3] + m[2];
	// Far clip plane
	m_planes[5] = m[3] - m[2];
	
	m_dirty = false;
	m_normalized = false;
}

void KX_Camera::NormalizeClipPlanes()
{
	if (m_normalized)
		return;
	
	for (unsigned int p = 0; p < 6; p++)
	{
		MT_Scalar factor = sqrt(m_planes[p][0]*m_planes[p][0] + m_planes[p][1]*m_planes[p][1] + m_planes[p][2]*m_planes[p][2]);
		if (!MT_fuzzyZero(factor))
			m_planes[p] /= factor;
	}
	
	m_normalized = true;
}

void KX_Camera::ExtractFrustumSphere()
{
	if (m_set_frustum_center)
		return;

	// compute sphere for the general case and not only symmetric frustum:
	// the mirror code in ImageRender can use very asymmetric frustum.
	// We will put the sphere center on the line that goes from origin to the center of the far clipping plane
	// This is the optimal position if the frustum is symmetric or very asymmetric and probably close
	// to optimal for the general case. The sphere center position is computed so that the distance to
	// the near and far extreme frustum points are equal.

	// get the transformation matrix from device coordinate to camera coordinate
	MT_Matrix4x4 clip_camcs_matrix = m_projection_matrix;
	clip_camcs_matrix.invert();

	if (m_projection_matrix[3][3] == MT_Scalar(0.0))
	{
		// frustrum projection
		// detect which of the corner of the far clipping plane is the farthest to the origin
		MT_Vector4 nfar;    // far point in device normalized coordinate
		MT_Point3 farpoint; // most extreme far point in camera coordinate
		MT_Point3 nearpoint;// most extreme near point in camera coordinate
		MT_Point3 farcenter(0.,0.,0.);// center of far cliping plane in camera coordinate
		MT_Scalar F=-1.0, N; // square distance of far and near point to origin
		MT_Scalar f, n;     // distance of far and near point to z axis. f is always > 0 but n can be < 0
		MT_Scalar e, s;     // far and near clipping distance (<0)
		MT_Scalar c;        // slope of center line = distance of far clipping center to z axis / far clipping distance
		MT_Scalar z;        // projection of sphere center on z axis (<0)
		// tmp value
		MT_Vector4 npoint(1., 1., 1., 1.);
		MT_Vector4 hpoint;
		MT_Point3 point;
		MT_Scalar len;
		for (int i=0; i<4; i++)
		{
			hpoint = clip_camcs_matrix*npoint;
			point.setValue(hpoint[0]/hpoint[3], hpoint[1]/hpoint[3], hpoint[2]/hpoint[3]);
			len = point.dot(point);
			if (len > F)
			{
				nfar = npoint;
				farpoint = point;
				F = len;
			}
			// rotate by 90 degree along the z axis to walk through the 4 extreme points of the far clipping plane
			len = npoint[0];
			npoint[0] = -npoint[1];
			npoint[1] = len;
			farcenter += point;
		}
		// the far center is the average of the far clipping points
		farcenter *= 0.25;
		// the extreme near point is the opposite point on the near clipping plane
		nfar.setValue(-nfar[0], -nfar[1], -1., 1.);
		nfar = clip_camcs_matrix*nfar;
		nearpoint.setValue(nfar[0]/nfar[3], nfar[1]/nfar[3], nfar[2]/nfar[3]);
		// this is a frustrum projection
		N = nearpoint.dot(nearpoint);
		e = farpoint[2];
		s = nearpoint[2];
		// projection on XY plane for distance to axis computation
		MT_Point2 farxy(farpoint[0], farpoint[1]);
		// f is forced positive by construction
		f = farxy.length();
		// get corresponding point on the near plane
		farxy *= s/e;
		// this formula preserve the sign of n
		n = f*s/e - MT_Point2(nearpoint[0]-farxy[0], nearpoint[1]-farxy[1]).length();
		c = MT_Point2(farcenter[0], farcenter[1]).length()/e;
		// the big formula, it simplifies to (F-N)/(2(e-s)) for the symmetric case
		z = (F-N)/(2.0*(e-s+c*(f-n)));
		m_frustum_center = MT_Point3(farcenter[0]*z/e, farcenter[1]*z/e, z);
		m_frustum_radius = m_frustum_center.distance(farpoint);
	}
	else
	{
		// orthographic projection
		// The most extreme points on the near and far plane. (normalized device coords)
		MT_Vector4 hnear(1., 1., 1., 1.), hfar(-1., -1., -1., 1.);
		
		// Transform to hom camera local space
		hnear = clip_camcs_matrix*hnear;
		hfar = clip_camcs_matrix*hfar;
		
		// Tranform to 3d camera local space.
		MT_Point3 nearpoint(hnear[0]/hnear[3], hnear[1]/hnear[3], hnear[2]/hnear[3]);
		MT_Point3 farpoint(hfar[0]/hfar[3], hfar[1]/hfar[3], hfar[2]/hfar[3]);
		
		// just use mediant point
		m_frustum_center = (farpoint + nearpoint)*0.5;
		m_frustum_radius = m_frustum_center.distance(farpoint);
	}
	// Transform to world space.
	m_frustum_center = GetCameraToWorld()(m_frustum_center);
	m_frustum_radius /= fabs(NodeGetWorldScaling()[NodeGetWorldScaling().closestAxis()]);
	
	m_set_frustum_center = true;
}

bool KX_Camera::PointInsideFrustum(const MT_Point3& x)
{
	ExtractClipPlanes();
	
	for ( unsigned int i = 0; i < 6 ; i++ )
	{
		if (m_planes[i][0]*x[0] + m_planes[i][1]*x[1] + m_planes[i][2]*x[2] + m_planes[i][3] < 0.)
			return false;
	}
	return true;
}

int KX_Camera::BoxInsideFrustum(const MT_Point3 *box)
{
	ExtractClipPlanes();
	
	unsigned int insideCount = 0;
	// 6 view frustum planes
	for ( unsigned int p = 0; p < 6 ; p++ )
	{
		unsigned int behindCount = 0;
		// 8 box vertices.
		for (unsigned int v = 0; v < 8 ; v++)
		{
			if (m_planes[p][0]*box[v][0] + m_planes[p][1]*box[v][1] + m_planes[p][2]*box[v][2] + m_planes[p][3] < 0.)
				behindCount++;
		}
		
		// 8 points behind this plane
		if (behindCount == 8)
			return OUTSIDE;

		// Every box vertex is on the front side of this plane
		if (!behindCount)
			insideCount++;
	}
	
	// All box vertices are on the front side of all frustum planes.
	if (insideCount == 6)
		return INSIDE;
	
	return INTERSECT;
}

int KX_Camera::SphereInsideFrustum(const MT_Point3& center, const MT_Scalar &radius)
{
	ExtractFrustumSphere();
	if (center.distance2(m_frustum_center) > (radius + m_frustum_radius)*(radius + m_frustum_radius))
		return OUTSIDE;

	unsigned int p;
	ExtractClipPlanes();
	NormalizeClipPlanes();
		
	MT_Scalar distance;
	int intersect = INSIDE;
	// distance:  <-------- OUTSIDE -----|----- INTERSECT -----0----- INTERSECT -----|----- INSIDE -------->
	//                                -radius                                      radius
	for (p = 0; p < 6; p++)
	{
		distance = m_planes[p][0]*center[0] + m_planes[p][1]*center[1] + m_planes[p][2]*center[2] + m_planes[p][3];
		if (fabs(distance) <= radius)
			intersect = INTERSECT;
		else if (distance < -radius)
			return OUTSIDE;
	}
	
	return intersect;
}

bool KX_Camera::GetFrustumCulling() const
{
	return m_frustum_culling;
}
 
void KX_Camera::EnableViewport(bool viewport)
{
	m_camdata.m_viewport = viewport;
}

void KX_Camera::SetViewport(int left, int bottom, int right, int top)
{
	m_camdata.m_viewportleft = left;
	m_camdata.m_viewportbottom = bottom;
	m_camdata.m_viewportright = right;
	m_camdata.m_viewporttop = top;
}

bool KX_Camera::GetViewport() const
{
	return m_camdata.m_viewport;
}

int KX_Camera::GetViewportLeft() const
{
	return m_camdata.m_viewportleft;
}

int KX_Camera::GetViewportBottom() const
{
	return m_camdata.m_viewportbottom;
}

int KX_Camera::GetViewportRight() const
{
	return m_camdata.m_viewportright;
}

int KX_Camera::GetViewportTop() const
{
	return m_camdata.m_viewporttop;
}

#ifdef WITH_PYTHON
//----------------------------------------------------------------------------
//Python


PyMethodDef KX_Camera::Methods[] = {
	KX_PYMETHODTABLE(KX_Camera, sphereInsideFrustum),
	KX_PYMETHODTABLE_O(KX_Camera, boxInsideFrustum),
	KX_PYMETHODTABLE_O(KX_Camera, pointInsideFrustum),
	KX_PYMETHODTABLE_NOARGS(KX_Camera, getCameraToWorld),
	KX_PYMETHODTABLE_NOARGS(KX_Camera, getWorldToCamera),
	KX_PYMETHODTABLE(KX_Camera, setViewport),
	KX_PYMETHODTABLE_NOARGS(KX_Camera, setOnTop),
	KX_PYMETHODTABLE_O(KX_Camera, getScreenPosition),
	KX_PYMETHODTABLE(KX_Camera, getScreenVect),
	KX_PYMETHODTABLE(KX_Camera, getScreenRay),
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_Camera::Attributes[] = {
	
	KX_PYATTRIBUTE_BOOL_RW("frustum_culling", KX_Camera, m_frustum_culling),
	KX_PYATTRIBUTE_RW_FUNCTION("perspective", KX_Camera, pyattr_get_perspective, pyattr_set_perspective),
	
	KX_PYATTRIBUTE_RW_FUNCTION("lens",	KX_Camera,	pyattr_get_lens, pyattr_set_lens),
	KX_PYATTRIBUTE_RW_FUNCTION("ortho_scale",	KX_Camera,	pyattr_get_ortho_scale, pyattr_set_ortho_scale),
	KX_PYATTRIBUTE_RW_FUNCTION("near",	KX_Camera,	pyattr_get_near, pyattr_set_near),
	KX_PYATTRIBUTE_RW_FUNCTION("far",	KX_Camera,	pyattr_get_far,  pyattr_set_far),
	
	KX_PYATTRIBUTE_RW_FUNCTION("useViewport",	KX_Camera,	pyattr_get_use_viewport,  pyattr_set_use_viewport),
	
	KX_PYATTRIBUTE_RW_FUNCTION("projection_matrix",	KX_Camera,	pyattr_get_projection_matrix, pyattr_set_projection_matrix),
	KX_PYATTRIBUTE_RO_FUNCTION("modelview_matrix",	KX_Camera,	pyattr_get_modelview_matrix),
	KX_PYATTRIBUTE_RO_FUNCTION("camera_to_world",	KX_Camera,	pyattr_get_camera_to_world),
	KX_PYATTRIBUTE_RO_FUNCTION("world_to_camera",	KX_Camera,	pyattr_get_world_to_camera),
	
	/* Grrr, functions for constants? */
	KX_PYATTRIBUTE_RO_FUNCTION("INSIDE",	KX_Camera, pyattr_get_INSIDE),
	KX_PYATTRIBUTE_RO_FUNCTION("OUTSIDE",	KX_Camera, pyattr_get_OUTSIDE),
	KX_PYATTRIBUTE_RO_FUNCTION("INTERSECT",	KX_Camera, pyattr_get_INTERSECT),
	
	{ NULL }	//Sentinel
};

PyTypeObject KX_Camera::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_Camera",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	&KX_GameObject::Sequence,
	&KX_GameObject::Mapping,
	0,0,0,
	NULL,
	NULL,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&KX_GameObject::Type,
	0,0,0,0,0,0,
	py_base_new
};

KX_PYMETHODDEF_DOC_VARARGS(KX_Camera, sphereInsideFrustum,
"sphereInsideFrustum(center, radius) -> Integer\n"
"\treturns INSIDE, OUTSIDE or INTERSECT if the given sphere is\n"
"\tinside/outside/intersects this camera's viewing frustum.\n\n"
"\tcenter = the center of the sphere (in world coordinates.)\n"
"\tradius = the radius of the sphere\n\n"
"\tExample:\n"
"\timport bge.logic\n\n"
"\tco = bge.logic.getCurrentController()\n"
"\tcam = co.GetOwner()\n\n"
"\t# A sphere of radius 4.0 located at [x, y, z] = [1.0, 1.0, 1.0]\n"
"\tif (cam.sphereInsideFrustum([1.0, 1.0, 1.0], 4) != cam.OUTSIDE):\n"
"\t\t# Sphere is inside frustum !\n"
"\t\t# Do something useful !\n"
"\telse:\n"
"\t\t# Sphere is outside frustum\n"
)
{
	PyObject *pycenter;
	float radius;
	if (PyArg_ParseTuple(args, "Of:sphereInsideFrustum", &pycenter, &radius))
	{
		MT_Point3 center;
		if (PyVecTo(pycenter, center))
		{
			return PyLong_FromSsize_t(SphereInsideFrustum(center, radius)); /* new ref */
		}
	}

	PyErr_SetString(PyExc_TypeError, "camera.sphereInsideFrustum(center, radius): KX_Camera, expected arguments: (center, radius)");
	
	return NULL;
}

KX_PYMETHODDEF_DOC_O(KX_Camera, boxInsideFrustum,
"boxInsideFrustum(box) -> Integer\n"
"\treturns INSIDE, OUTSIDE or INTERSECT if the given box is\n"
"\tinside/outside/intersects this camera's viewing frustum.\n\n"
"\tbox = a list of the eight (8) corners of the box (in world coordinates.)\n\n"
"\tExample:\n"
"\timport bge.logic\n\n"
"\tco = bge.logic.getCurrentController()\n"
"\tcam = co.GetOwner()\n\n"
"\tbox = []\n"
"\tbox.append([-1.0, -1.0, -1.0])\n"
"\tbox.append([-1.0, -1.0,  1.0])\n"
"\tbox.append([-1.0,  1.0, -1.0])\n"
"\tbox.append([-1.0,  1.0,  1.0])\n"
"\tbox.append([ 1.0, -1.0, -1.0])\n"
"\tbox.append([ 1.0, -1.0,  1.0])\n"
"\tbox.append([ 1.0,  1.0, -1.0])\n"
"\tbox.append([ 1.0,  1.0,  1.0])\n\n"
"\tif (cam.boxInsideFrustum(box) != cam.OUTSIDE):\n"
"\t\t# Box is inside/intersects frustum !\n"
"\t\t# Do something useful !\n"
"\telse:\n"
"\t\t# Box is outside the frustum !\n"
)
{
	unsigned int num_points = PySequence_Size(value);
	if (num_points != 8)
	{
		PyErr_Format(PyExc_TypeError, "camera.boxInsideFrustum(box): KX_Camera, expected eight (8) points, got %d", num_points);
		return NULL;
	}
	
	MT_Point3 box[8];
	for (unsigned int p = 0; p < 8 ; p++)
	{
		PyObject *item = PySequence_GetItem(value, p); /* new ref */
		bool error = !PyVecTo(item, box[p]);
		Py_DECREF(item);
		if (error)
			return NULL;
	}
	
	return PyLong_FromSsize_t(BoxInsideFrustum(box)); /* new ref */
}

KX_PYMETHODDEF_DOC_O(KX_Camera, pointInsideFrustum,
"pointInsideFrustum(point) -> Bool\n"
"\treturns 1 if the given point is inside this camera's viewing frustum.\n\n"
"\tpoint = The point to test (in world coordinates.)\n\n"
"\tExample:\n"
"\timport bge.logic\n\n"
"\tco = bge.logic.getCurrentController()\n"
"\tcam = co.GetOwner()\n\n"
"\t# Test point [0.0, 0.0, 0.0]"
"\tif (cam.pointInsideFrustum([0.0, 0.0, 0.0])):\n"
"\t\t# Point is inside frustum !\n"
"\t\t# Do something useful !\n"
"\telse:\n"
"\t\t# Box is outside the frustum !\n"
)
{
	MT_Point3 point;
	if (PyVecTo(value, point))
	{
		return PyLong_FromSsize_t(PointInsideFrustum(point)); /* new ref */
	}
	
	PyErr_SetString(PyExc_TypeError, "camera.pointInsideFrustum(point): KX_Camera, expected point argument.");
	return NULL;
}

KX_PYMETHODDEF_DOC_NOARGS(KX_Camera, getCameraToWorld,
"getCameraToWorld() -> Matrix4x4\n"
"\treturns the camera to world transformation matrix, as a list of four lists of four values.\n\n"
"\tie: [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]])\n"
)
{
	return PyObjectFrom(GetCameraToWorld()); /* new ref */
}

KX_PYMETHODDEF_DOC_NOARGS(KX_Camera, getWorldToCamera,
"getWorldToCamera() -> Matrix4x4\n"
"\treturns the world to camera transformation matrix, as a list of four lists of four values.\n\n"
"\tie: [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]])\n"
)
{
	return PyObjectFrom(GetWorldToCamera()); /* new ref */
}

KX_PYMETHODDEF_DOC_VARARGS(KX_Camera, setViewport,
"setViewport(left, bottom, right, top)\n"
"Sets this camera's viewport\n")
{
	int left, bottom, right, top;
	if (!PyArg_ParseTuple(args,"iiii:setViewport",&left, &bottom, &right, &top))
		return NULL;
	
	SetViewport(left, bottom, right, top);
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC_NOARGS(KX_Camera, setOnTop,
"setOnTop()\n"
"Sets this camera's viewport on top\n")
{
	class KX_Scene* scene = KX_GetActiveScene();
	scene->SetCameraOnTop(this);
	Py_RETURN_NONE;
}

PyObject* KX_Camera::pyattr_get_perspective(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyBool_FromLong(self->m_camdata.m_perspective);
}

int KX_Camera::pyattr_set_perspective(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	int param = PyObject_IsTrue( value );
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "camera.perspective = bool: KX_Camera, expected True/False or 0/1");
		return PY_SET_ATTR_FAIL;
	}
	
	self->m_camdata.m_perspective= param;
	self->InvalidateProjectionMatrix();
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_Camera::pyattr_get_lens(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyFloat_FromDouble(self->m_camdata.m_lens);
}

int KX_Camera::pyattr_set_lens(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	float param = PyFloat_AsDouble(value);
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "camera.lens = float: KX_Camera, expected a float greater then zero");
		return PY_SET_ATTR_FAIL;
	}
	
	self->m_camdata.m_lens= param;
	self->m_set_projection_matrix = false;
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_Camera::pyattr_get_ortho_scale(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyFloat_FromDouble(self->m_camdata.m_scale);
}

int KX_Camera::pyattr_set_ortho_scale(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	float param = PyFloat_AsDouble(value);
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "camera.ortho_scale = float: KX_Camera, expected a float greater then zero");
		return PY_SET_ATTR_FAIL;
	}
	
	self->m_camdata.m_scale= param;
	self->m_set_projection_matrix = false;
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_Camera::pyattr_get_near(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyFloat_FromDouble(self->m_camdata.m_clipstart);
}

int KX_Camera::pyattr_set_near(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	float param = PyFloat_AsDouble(value);
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "camera.near = float: KX_Camera, expected a float greater then zero");
		return PY_SET_ATTR_FAIL;
	}
	
	self->m_camdata.m_clipstart= param;
	self->m_set_projection_matrix = false;
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_Camera::pyattr_get_far(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyFloat_FromDouble(self->m_camdata.m_clipend);
}

int KX_Camera::pyattr_set_far(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	float param = PyFloat_AsDouble(value);
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "camera.far = float: KX_Camera, expected a float greater then zero");
		return PY_SET_ATTR_FAIL;
	}
	
	self->m_camdata.m_clipend= param;
	self->m_set_projection_matrix = false;
	return PY_SET_ATTR_SUCCESS;
}


PyObject* KX_Camera::pyattr_get_use_viewport(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyBool_FromLong(self->GetViewport());
}

int KX_Camera::pyattr_set_use_viewport(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	int param = PyObject_IsTrue( value );
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "camera.useViewport = bool: KX_Camera, expected True or False");
		return PY_SET_ATTR_FAIL;
	}
	self->EnableViewport((bool)param);
	return PY_SET_ATTR_SUCCESS;
}


PyObject* KX_Camera::pyattr_get_projection_matrix(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyObjectFrom(self->GetProjectionMatrix()); 
}

int KX_Camera::pyattr_set_projection_matrix(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	MT_Matrix4x4 mat;
	if (!PyMatTo(value, mat)) 
		return PY_SET_ATTR_FAIL;
	
	self->SetProjectionMatrix(mat);
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_Camera::pyattr_get_modelview_matrix(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyObjectFrom(self->GetModelviewMatrix()); 
}

PyObject* KX_Camera::pyattr_get_camera_to_world(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyObjectFrom(self->GetCameraToWorld());
}

PyObject* KX_Camera::pyattr_get_world_to_camera(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Camera* self= static_cast<KX_Camera*>(self_v);
	return PyObjectFrom(self->GetWorldToCamera()); 
}


PyObject* KX_Camera::pyattr_get_INSIDE(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{	return PyLong_FromSsize_t(INSIDE); }
PyObject* KX_Camera::pyattr_get_OUTSIDE(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{	return PyLong_FromSsize_t(OUTSIDE); }
PyObject* KX_Camera::pyattr_get_INTERSECT(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{	return PyLong_FromSsize_t(INTERSECT); }


bool ConvertPythonToCamera(PyObject * value, KX_Camera **object, bool py_none_ok, const char *error_prefix)
{
	if (value==NULL) {
		PyErr_Format(PyExc_TypeError, "%s, python pointer NULL, should never happen", error_prefix);
		*object = NULL;
		return false;
	}
		
	if (value==Py_None) {
		*object = NULL;
		
		if (py_none_ok) {
			return true;
		} else {
			PyErr_Format(PyExc_TypeError, "%s, expected KX_Camera or a KX_Camera name, None is invalid", error_prefix);
			return false;
		}
	}
	
	if (PyUnicode_Check(value)) {
		STR_String value_str = _PyUnicode_AsString(value);
		*object = KX_GetActiveScene()->FindCamera(value_str);
		
		if (*object) {
			return true;
		} else {
			PyErr_Format(PyExc_ValueError,
			             "%s, requested name \"%s\" did not match any KX_Camera in this scene",
			             error_prefix, _PyUnicode_AsString(value));
			return false;
		}
	}
	
	if (PyObject_TypeCheck(value, &KX_Camera::Type)) {
		*object = static_cast<KX_Camera*>BGE_PROXY_REF(value);
		
		/* sets the error */
		if (*object==NULL) {
			PyErr_Format(PyExc_SystemError, "%s, " BGE_PROXY_ERROR_MSG, error_prefix);
			return false;
		}
		
		return true;
	}
	
	*object = NULL;
	
	if (py_none_ok) {
		PyErr_Format(PyExc_TypeError, "%s, expect a KX_Camera, a string or None", error_prefix);
	} else {
		PyErr_Format(PyExc_TypeError, "%s, expect a KX_Camera or a string", error_prefix);
	}
	
	return false;
}

KX_PYMETHODDEF_DOC_O(KX_Camera, getScreenPosition,
"getScreenPosition()\n"
)

{
	MT_Vector3 vect;
	KX_GameObject *obj = NULL;

	if (!PyVecTo(value, vect))
	{
		PyErr_Clear();

		if (ConvertPythonToGameObject(value, &obj, true, ""))
		{
			PyErr_Clear();
			vect = MT_Vector3(obj->NodeGetWorldPosition());
		}
		else
		{
			PyErr_SetString(PyExc_TypeError, "Error in getScreenPosition. Expected a Vector3 or a KX_GameObject or a string for a name of a KX_GameObject");
			return NULL;
		}
	}

	GLint viewport[4];
	GLdouble win[3];
	GLdouble modelmatrix[16];
	GLdouble projmatrix[16];

	MT_Matrix4x4 m_modelmatrix = this->GetModelviewMatrix();
	MT_Matrix4x4 m_projmatrix = this->GetProjectionMatrix();

	m_modelmatrix.getValue(modelmatrix);
	m_projmatrix.getValue(projmatrix);

	glGetIntegerv(GL_VIEWPORT, viewport);

	gluProject(vect[0], vect[1], vect[2], modelmatrix, projmatrix, viewport, &win[0], &win[1], &win[2]);

	vect[0] =  (win[0] - viewport[0]) / viewport[2];
	vect[1] =  (win[1] - viewport[1]) / viewport[3];

	vect[1] = 1.0 - vect[1]; //to follow Blender window coordinate system (Top-Down)

	PyObject* ret = PyTuple_New(2);
	if (ret) {
		PyTuple_SET_ITEM(ret, 0, PyFloat_FromDouble(vect[0]));
		PyTuple_SET_ITEM(ret, 1, PyFloat_FromDouble(vect[1]));
		return ret;
	}

	return NULL;
}

KX_PYMETHODDEF_DOC_VARARGS(KX_Camera, getScreenVect,
"getScreenVect()\n"
)
{
	double x,y;
	if (!PyArg_ParseTuple(args,"dd:getScreenVect",&x,&y))
		return NULL;

	y = 1.0 - y; //to follow Blender window coordinate system (Top-Down)

	MT_Vector3 vect;
	MT_Point3 campos, screenpos;

	GLint viewport[4];
	GLdouble win[3];
	GLdouble modelmatrix[16];
	GLdouble projmatrix[16];

	MT_Matrix4x4 m_modelmatrix = this->GetModelviewMatrix();
	MT_Matrix4x4 m_projmatrix = this->GetProjectionMatrix();

	m_modelmatrix.getValue(modelmatrix);
	m_projmatrix.getValue(projmatrix);

	glGetIntegerv(GL_VIEWPORT, viewport);

	vect[0] = x * viewport[2];
	vect[1] = y * viewport[3];

	vect[0] += viewport[0];
	vect[1] += viewport[1];

	glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &vect[2]);
	gluUnProject(vect[0], vect[1], vect[2], modelmatrix, projmatrix, viewport, &win[0], &win[1], &win[2]);

	campos = this->GetCameraLocation();
	screenpos = MT_Point3(win[0], win[1], win[2]);
	vect = campos-screenpos;

	vect.normalize();
	return PyObjectFrom(vect);
}

KX_PYMETHODDEF_DOC_VARARGS(KX_Camera, getScreenRay,
"getScreenRay()\n"
)
{
	MT_Vector3 vect;
	double x,y,dist;
	char *propName = NULL;

	if (!PyArg_ParseTuple(args,"ddd|s:getScreenRay",&x,&y,&dist,&propName))
		return NULL;

	PyObject* argValue = PyTuple_New(2);
	PyTuple_SET_ITEM(argValue, 0, PyFloat_FromDouble(x));
	PyTuple_SET_ITEM(argValue, 1, PyFloat_FromDouble(y));

	if (!PyVecTo(PygetScreenVect(argValue), vect))
	{
		Py_DECREF(argValue);
		PyErr_SetString(PyExc_TypeError,
			"Error in getScreenRay. Invalid 2D coordinate. Expected a normalized 2D screen coordinate, a distance and an optional property argument");
		return NULL;
	}
	Py_DECREF(argValue);

	dist = -dist;
	vect += this->GetCameraLocation();

	argValue = (propName?PyTuple_New(3):PyTuple_New(2));
	if (argValue) {
		PyTuple_SET_ITEM(argValue, 0, PyObjectFrom(vect));
		PyTuple_SET_ITEM(argValue, 1, PyFloat_FromDouble(dist));
		if (propName)
			PyTuple_SET_ITEM(argValue, 2, PyUnicode_FromString(propName));

		PyObject* ret= this->PyrayCastTo(argValue,NULL);
		Py_DECREF(argValue);
		return ret;
	}

	return NULL;
}
#endif
