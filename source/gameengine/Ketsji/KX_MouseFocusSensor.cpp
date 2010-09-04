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
 * KX_MouseFocusSensor determines mouse in/out/over events.
 */

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "MT_Point3.h"
#include "RAS_FramingManager.h"
#include "RAS_ICanvas.h"
#include "RAS_IRasterizer.h"
#include "SCA_IScene.h"
#include "KX_Scene.h"
#include "KX_Camera.h"
#include "KX_MouseFocusSensor.h"
#include "KX_PyMath.h"

#include "KX_RayCast.h"
#include "KX_IPhysicsController.h"
#include "PHY_IPhysicsController.h"
#include "PHY_IPhysicsEnvironment.h"


#include "KX_ClientObjectInfo.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_MouseFocusSensor::KX_MouseFocusSensor(SCA_MouseManager* eventmgr, 
										 int startx,
										 int starty,
										 short int mousemode,
										 int focusmode,
										 bool bTouchPulse,
										 KX_Scene* kxscene,
										 KX_KetsjiEngine *kxengine,
										 SCA_IObject* gameobj)
	: SCA_MouseSensor(eventmgr, startx, starty, mousemode, gameobj),
	  m_focusmode(focusmode),
	  m_bTouchPulse(bTouchPulse),
	  m_kxscene(kxscene),
	  m_kxengine(kxengine)
{
	Init();
}

void KX_MouseFocusSensor::Init()
{
	m_mouse_over_in_previous_frame = (m_invert)?true:false;
	m_positive_event = false;
	m_hitObject = 0;
	m_hitObject_Last = NULL;
	m_reset = true;
	
	m_hitPosition.setValue(0,0,0);
	m_prevTargetPoint.setValue(0,0,0);
	m_prevSourcePoint.setValue(0,0,0);
	m_hitNormal.setValue(0,0,1);
}

bool KX_MouseFocusSensor::Evaluate()
{
	bool result = false;
	bool obHasFocus = false;
	bool reset = m_reset && m_level;

//  	cout << "evaluate focus mouse sensor "<<endl;
	m_reset = false;
	if (m_focusmode) {
		/* Focus behaviour required. Test mouse-on. The rest is
		 * equivalent to handling a key. */
		obHasFocus = ParentObjectHasFocus();
		
		if (!obHasFocus) {
			m_positive_event = false;
			if (m_mouse_over_in_previous_frame) {
				result = true;
			} 
		} else {
			m_positive_event = true;
			if (!m_mouse_over_in_previous_frame) {
				result = true;
			}
			else if(m_bTouchPulse && (m_hitObject != m_hitObject_Last)) {
				result = true;
			}
		} 
		if (reset) {
			// force an event 
			result = true;
		}
	} else {
		/* No focus behaviour required: revert to the basic mode. This
         * mode is never used, because the converter never makes this
         * sensor for a mouse-key event. It is here for
         * completeness. */
		result = SCA_MouseSensor::Evaluate();
		m_positive_event = (m_val!=0);
	}

	m_mouse_over_in_previous_frame = obHasFocus;
	m_hitObject_Last = (void *)m_hitObject;
					   
	return result;
}

bool KX_MouseFocusSensor::RayHit(KX_ClientObjectInfo* client_info, KX_RayCast* result, void * const data)
{
	KX_GameObject* hitKXObj = client_info->m_gameobject;
	
	/* Is this me? In the ray test, there are a lot of extra checks
	* for aliasing artefacts from self-hits. That doesn't happen
	* here, so a simple test suffices. Or does the camera also get
	* self-hits? (No, and the raysensor shouldn't do it either, since
	* self-hits are excluded by setting the correct ignore-object.)
	* Hitspots now become valid. */
	KX_GameObject* thisObj = (KX_GameObject*) GetParent();
	if ((m_focusmode == 2) || hitKXObj == thisObj)
	{
		m_hitObject = hitKXObj;
		m_hitPosition = result->m_hitPoint;
		m_hitNormal = result->m_hitNormal;
		m_hitUV = result->m_hitUV;
		return true;
	}
	
	return true;     // object must be visible to trigger
	//return false;  // occluded objects can trigger
}



bool KX_MouseFocusSensor::ParentObjectHasFocusCamera(KX_Camera *cam)
{
	/* All screen handling in the gameengine is done by GL,
	 * specifically the model/view and projection parts. The viewport
	 * part is in the creator. 
	 *
	 * The theory is this:
	 * WCS - world coordinates
	 * -> wcs_camcs_trafo ->
	 * camCS - camera coordinates
	 * -> camcs_clip_trafo ->
	 * clipCS - normalized device coordinates?
	 * -> normview_win_trafo
	 * winCS - window coordinates
	 *
	 * The first two transforms are respectively the model/view and
	 * the projection matrix. These are passed to the rasterizer, and
	 * we store them in the camera for easy access.
	 *
	 * For normalized device coords (xn = x/w, yn = y/w/zw) the
	 * windows coords become (lb = left bottom)
	 *
	 * xwin = [(xn + 1.0) * width]/2 + x_lb
	 * ywin = [(yn + 1.0) * height]/2 + y_lb
	 *
	 * Inverting (blender y is flipped!):
	 *
	 * xn = 2(xwin - x_lb)/width - 1.0
	 * yn = 2(ywin - y_lb)/height - 1.0 
	 *    = 2(height - y_blender - y_lb)/height - 1.0
	 *    = 1.0 - 2(y_blender - y_lb)/height
	 *
	 * */
	 
	
	/* Because we don't want to worry about resize events, camera
	 * changes and all that crap, we just determine this over and
	 * over. Stop whining. We have lots of other calculations to do
	 * here as well. These reads are not the main cost. If there is no
	 * canvas, the test is irrelevant. The 1.0 makes sure the
	 * calculations don't bomb. Maybe we should explicitly guard for
	 * division by 0.0...*/
	
	RAS_Rect area, viewport;
	short m_y_inv = m_kxengine->GetCanvas()->GetHeight()-m_y;
	
	m_kxengine->GetSceneViewport(m_kxscene, cam, area, viewport);
	
	/* Check if the mouse is in the viewport */
	if ((	m_x < viewport.m_x2 &&	// less then right
			m_x > viewport.m_x1 &&	// more then then left
			m_y_inv < viewport.m_y2 &&	// below top
			m_y_inv > viewport.m_y1) == 0)	// above bottom
	{
		return false;
	}

	float height = float(viewport.m_y2 - viewport.m_y1 + 1);
	float width  = float(viewport.m_x2 - viewport.m_x1 + 1);
	
	float x_lb = float(viewport.m_x1);
	float y_lb = float(viewport.m_y1);

	MT_Vector4 frompoint;
	MT_Vector4 topoint;
	
	/* m_y_inv - inverting for a bounds check is only part of it, now make relative to view bounds */
	m_y_inv = (viewport.m_y2 - m_y_inv) + viewport.m_y1;
	
	
	/* There's some strangeness I don't fully get here... These values
	 * _should_ be wrong! - see from point Z values */
	
	
	/*	build the from and to point in normalized device coordinates 
	 *	Looks like normailized device coordinates are [-1,1] in x [-1,1] in y
	 *	[0,-1] in z 
	 *	
	 *	The actual z coordinates used don't have to be exact just infront and 
	 *	behind of the near and far clip planes.
	 */ 
	frompoint.setValue(	(2 * (m_x-x_lb) / width) - 1.0,
						1.0 - (2 * (m_y_inv - y_lb) / height),
						/*cam->GetCameraData()->m_perspective ? 0.0:cdata->m_clipstart,*/ /* real clipstart is scaled in ortho for some reason, zero is ok */
						0.0, /* nearclip, see above comments */
						1.0 );
	
	topoint.setValue(	(2 * (m_x-x_lb) / width) - 1.0,
						1.0 - (2 * (m_y_inv-y_lb) / height),
						cam->GetCameraData()->m_perspective ? 1.0:cam->GetCameraData()->m_clipend, /* farclip, see above comments */
						1.0 );

	/* camera to world  */
	MT_Transform wcs_camcs_tranform = cam->GetWorldToCamera();
	MT_Transform cams_wcs_transform;
	cams_wcs_transform.invert(wcs_camcs_tranform);
	
	MT_Matrix4x4 camcs_wcs_matrix = MT_Matrix4x4(cams_wcs_transform);

	/* badly defined, the first time round.... I wonder why... I might
	 * want to guard against floating point errors here.*/
	MT_Matrix4x4 clip_camcs_matrix = MT_Matrix4x4(cam->GetProjectionMatrix());
	clip_camcs_matrix.invert();

	/* shoot-points: clip to cam to wcs . win to clip was already done.*/
	frompoint = clip_camcs_matrix * frompoint;
	topoint   = clip_camcs_matrix * topoint;
	frompoint = camcs_wcs_matrix * frompoint;
	topoint   = camcs_wcs_matrix * topoint;
	
	/* from hom wcs to 3d wcs: */
	m_prevSourcePoint.setValue(	frompoint[0]/frompoint[3],
								frompoint[1]/frompoint[3],
								frompoint[2]/frompoint[3]); 
	
	m_prevTargetPoint.setValue(	topoint[0]/topoint[3],
								topoint[1]/topoint[3],
								topoint[2]/topoint[3]); 
	
	/* 2. Get the object from PhysicsEnvironment */
	/* Shoot! Beware that the first argument here is an
	 * ignore-object. We don't ignore anything... */
	KX_IPhysicsController* physics_controller = cam->GetPhysicsController();
	PHY_IPhysicsEnvironment* physics_environment = m_kxscene->GetPhysicsEnvironment();

	// get UV mapping
	KX_RayCast::Callback<KX_MouseFocusSensor> callback(this,physics_controller,NULL,false,true);
	 
	KX_RayCast::RayTest(physics_environment, m_prevSourcePoint, m_prevTargetPoint, callback);
	
	if (m_hitObject)
		return true;
	
	return false;
}

bool KX_MouseFocusSensor::ParentObjectHasFocus()
{
	m_hitObject = 0;
	m_hitPosition.setValue(0,0,0);
	m_hitNormal.setValue(1,0,0);
	
	KX_Camera *cam= m_kxscene->GetActiveCamera();
	
	if(ParentObjectHasFocusCamera(cam))
		return true;

	list<class KX_Camera*>* cameras = m_kxscene->GetCameras();
	list<KX_Camera*>::iterator it = cameras->begin();
	
	while(it != cameras->end())
	{
		if(((*it) != cam) && (*it)->GetViewport())
			if (ParentObjectHasFocusCamera(*it))
				return true;
		
		it++;
	}
	
	return false;
}

const MT_Point3& KX_MouseFocusSensor::RaySource() const
{
	return m_prevSourcePoint;
}

const MT_Point3& KX_MouseFocusSensor::RayTarget() const
{
	return m_prevTargetPoint;
}

const MT_Point3& KX_MouseFocusSensor::HitPosition() const
{
	return m_hitPosition;
}

const MT_Vector3& KX_MouseFocusSensor::HitNormal() const
{
	return m_hitNormal;
}

const MT_Vector2& KX_MouseFocusSensor::HitUV() const
{
	return m_hitUV;
}

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_MouseFocusSensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_MouseFocusSensor",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_MouseSensor::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_MouseFocusSensor::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_MouseFocusSensor::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("raySource",		KX_MouseFocusSensor, pyattr_get_ray_source),
	KX_PYATTRIBUTE_RO_FUNCTION("rayTarget",		KX_MouseFocusSensor, pyattr_get_ray_target),
	KX_PYATTRIBUTE_RO_FUNCTION("rayDirection",	KX_MouseFocusSensor, pyattr_get_ray_direction),
	KX_PYATTRIBUTE_RO_FUNCTION("hitObject",		KX_MouseFocusSensor, pyattr_get_hit_object),
	KX_PYATTRIBUTE_RO_FUNCTION("hitPosition",	KX_MouseFocusSensor, pyattr_get_hit_position),
	KX_PYATTRIBUTE_RO_FUNCTION("hitNormal",		KX_MouseFocusSensor, pyattr_get_hit_normal),
	KX_PYATTRIBUTE_RO_FUNCTION("hitUV",		KX_MouseFocusSensor, pyattr_get_hit_uv),
	KX_PYATTRIBUTE_BOOL_RW("usePulseFocus",	KX_MouseFocusSensor,m_bTouchPulse),
	{ NULL }	//Sentinel
};

/* Attributes */
PyObject* KX_MouseFocusSensor::pyattr_get_ray_source(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseFocusSensor* self= static_cast<KX_MouseFocusSensor*>(self_v);
	return PyObjectFrom(self->RaySource());
}

PyObject* KX_MouseFocusSensor::pyattr_get_ray_target(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseFocusSensor* self= static_cast<KX_MouseFocusSensor*>(self_v);
	return PyObjectFrom(self->RayTarget());
}

PyObject* KX_MouseFocusSensor::pyattr_get_ray_direction(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseFocusSensor* self= static_cast<KX_MouseFocusSensor*>(self_v);
	MT_Vector3 dir = self->RayTarget() - self->RaySource();
	if(MT_fuzzyZero(dir))	dir.setValue(0,0,0);
	else					dir.normalize();
	return PyObjectFrom(dir);
}

PyObject* KX_MouseFocusSensor::pyattr_get_hit_object(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseFocusSensor* self= static_cast<KX_MouseFocusSensor*>(self_v);
	
	if(self->m_hitObject)
		return self->m_hitObject->GetProxy();
	
	Py_RETURN_NONE;
}

PyObject* KX_MouseFocusSensor::pyattr_get_hit_position(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseFocusSensor* self= static_cast<KX_MouseFocusSensor*>(self_v);
	return PyObjectFrom(self->HitPosition());
}

PyObject* KX_MouseFocusSensor::pyattr_get_hit_normal(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseFocusSensor* self= static_cast<KX_MouseFocusSensor*>(self_v);
	return PyObjectFrom(self->HitNormal());
}

PyObject* KX_MouseFocusSensor::pyattr_get_hit_uv(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseFocusSensor* self= static_cast<KX_MouseFocusSensor*>(self_v);
	return PyObjectFrom(self->HitUV());
}

#endif // DISABLE_PYTHON

/* eof */

