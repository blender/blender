/**
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
 * KX_MouseFocusSensor determines mouse in/out/over events.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "MT_Point3.h"
#include "KX_ClientObjectInfo.h"
#include "RAS_FramingManager.h"
#include "RAS_ICanvas.h"
#include "RAS_IRasterizer.h"
#include "SCA_IScene.h"
#include "KX_Scene.h"
#include "KX_Camera.h"
#include "KX_MouseFocusSensor.h"

#include "KX_ClientObjectInfo.h"
#include "SM_Object.h"
#include "SM_Scene.h"
#include "SumoPhysicsEnvironment.h"
#include "KX_SumoPhysicsController.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_MouseFocusSensor::KX_MouseFocusSensor(SCA_MouseManager* eventmgr, 
										 int startx,
										 int starty,
										 short int mousemode,
										 bool focusmode,
										 RAS_ICanvas* canvas,
										 KX_Scene* kxscene,
										 SCA_IObject* gameobj, 
										 PyTypeObject* T)
    : SCA_MouseSensor(eventmgr, startx, starty, mousemode, gameobj, T),
	  m_focusmode(focusmode),
	  m_gp_canvas(canvas),
	  m_kxscene(kxscene)
{
	/* Or postpone? I think a sumo scene and kx scene go pretty much
	 * together, so it should be safe to do it here. */
	m_mouse_over_in_previous_frame = false;
	m_positive_event = false;
}

bool KX_MouseFocusSensor::Evaluate(CValue* event)
{
	bool result = false;
	bool obHasFocus = false;

//  	cout << "evaluate focus mouse sensor "<<endl;

	if (m_focusmode) {
		/* Focus behaviour required. Test mouse-on. The rest is
		 * equivalent to handling a key. */
		obHasFocus = ParentObjectHasFocus();
		
		if (!obHasFocus) {
			if (m_mouse_over_in_previous_frame) {
					m_positive_event = false;
					result = true;
			} 
		} else {
			if (!m_mouse_over_in_previous_frame) {
				m_positive_event = true;
				result = true;
			} 
		} 
	} else {
		/* No focus behaviour required: revert to the basic mode. This
         * mode is never used, because the converter never makes this
         * sensor for a mouse-key event. It is here for
         * completeness. */
		result = SCA_MouseSensor::Evaluate(event);
		m_positive_event = (m_val!=0);
	}

	m_mouse_over_in_previous_frame = obHasFocus;

	return result;
}

bool KX_MouseFocusSensor::ParentObjectHasFocus(void)
{
	
  	bool res = false;
	m_hitPosition = MT_Vector3(0,0,0);
	m_hitNormal =	MT_Vector3(1,0,0);
	MT_Point3 resultpoint;
	MT_Vector3 resultnormal;

	/* All screen handling in the gameengine is done by GL,
	 * specifically the model/view and projection parts. The viewport
	 * part is in the creator. 
	 *
	 * The theory is this:
	 * WCS - world coordinates
	 * -> wcs_camcs_trafo ->
	 * camCS - camera coordinates
	 * -> camcs_clip_trafo ->
	 * clipCS - normalised device coordinates?
	 * -> normview_win_trafo
	 * winCS - window coordinates
	 *
	 * The first two transforms are respectively the model/view and
	 * the projection matrix. These are passed to the rasterizer, and
	 * we store them in the camera for easy access.
	 *
	 * For normalised device coords (xn = x/w, yn = y/w/zw) the
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

	/**
	 * Get the scenes current viewport.
	 */

	const RAS_Rect & viewport = m_kxscene->GetSceneViewport();

	float height = float(viewport.m_y2 - viewport.m_y1 + 1);
	float width  = float(viewport.m_x2 - viewport.m_x1 + 1);
	
	float x_lb = float(viewport.m_x1);
	float y_lb = float(viewport.m_y1);

	KX_Camera* cam = m_kxscene->GetActiveCamera();
	/* There's some strangeness I don't fully get here... These values
	 * _should_ be wrong! */

	/* old: */
	float nearclip = 0.0;
	float farclip = 1.0;

	/*	build the from and to point in normalised device coordinates 
	 *	Looks like normailized device coordinates are [-1,1] in x [-1,1] in y
	 *	[0,-1] in z 
	 *	
	 *	The actual z coordinates used don't have to be exact just infront and 
	 *	behind of the near and far clip planes.
	 */ 
	MT_Vector4 frompoint = MT_Vector4( 
		(2 * (m_x-x_lb) / width) - 1.0,
		1.0 - (2 * (m_y - y_lb) / height),
		nearclip,
		1.0
	);
	MT_Vector4 topoint = MT_Vector4( 
		(2 * (m_x-x_lb) / width) - 1.0,
		1.0 - (2 * (m_y-y_lb) / height),
		farclip,
		1.0
	);

	/* camera to world  */
	MT_Matrix4x4 camcs_wcs_matrix = MT_Matrix4x4(cam->GetCameraToWorld());

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
	MT_Point3 frompoint3 = MT_Point3(frompoint[0]/frompoint[3], 
									 frompoint[1]/frompoint[3], 
									 frompoint[2]/frompoint[3]); 
	MT_Point3 topoint3 = MT_Point3(topoint[0]/topoint[3], 
								   topoint[1]/topoint[3], 
								   topoint[2]/topoint[3]); 
	m_prevTargetPoint = topoint3;
	
	/* 2. Get the object from SuMO*/
	/* Shoot! Beware that the first argument here is an
	 * ignore-object. We don't ignore anything... */
	KX_GameObject* thisObj = (KX_GameObject*) GetParent();
	
	SumoPhysicsEnvironment *spe = dynamic_cast<SumoPhysicsEnvironment* > (m_kxscene->GetPhysicsEnvironment());
	SM_Scene *sumoScene = spe->GetSumoScene();
	KX_SumoPhysicsController *spc = dynamic_cast<KX_SumoPhysicsController*> (cam->GetPhysicsController());
	SM_Object *sumoCam = spc?spc->GetSumoObject():NULL;
	MT_Vector3 todir = topoint3 - frompoint3;
	if (todir.dot(todir) < MT_EPSILON)
		return false;
	todir.normalize();

	while (true)
	{	
		SM_Object* hitSMObj = sumoScene->rayTest(sumoCam, 
							frompoint3,
							topoint3,
							resultpoint, 
							resultnormal);
		
		if (!hitSMObj)
			return false;
			
		/* all this casting makes me nervous... */
		KX_ClientObjectInfo* client_info 
			= ( hitSMObj ?
				(KX_ClientObjectInfo*) hitSMObj->getClientObject() :
				NULL);
		
		if (!client_info)
		{
			std::cout<< "WARNING:  MouseOver sensor " << GetName() << " cannot sense SM_Object " << hitSMObj << " - no client info.\n" << std::endl;
			return false;
		} 
	
		KX_GameObject* hitKXObj = (KX_GameObject*)client_info->m_clientobject;
		
		if (client_info->m_type > KX_ClientObjectInfo::ACTOR)
		{
			// false hit
			KX_SumoPhysicsController *hitspc = dynamic_cast<KX_SumoPhysicsController *> (static_cast<KX_GameObject*> (hitKXObj) ->GetPhysicsController());
			if (hitspc)
			{
				/* We add 0.01 of fudge, so that if the margin && radius == 0., we don't endless loop. */
				MT_Scalar marg = 0.01 + hitspc->GetSumoObject()->getMargin();
				if (hitspc->GetSumoObject()->getShapeProps())
				{
					marg += 2*hitspc->GetSumoObject()->getShapeProps()->m_radius;
				}
				
				/* Calculate the other side of this object */
				MT_Point3 hitObjPos;
				hitspc->GetWorldPosition(hitObjPos);
				MT_Vector3 hitvector = hitObjPos - resultpoint;
				if (hitvector.dot(hitvector) > MT_EPSILON)
				{
					hitvector.normalize();
					marg *= 2.*todir.dot(hitvector);
				}
				frompoint3 = resultpoint + marg * todir;
			} else {
				return false;
			}
			continue;
		}
		/* Is this me? In the ray test, there are a lot of extra checks
		* for aliasing artefacts from self-hits. That doesn't happen
		* here, so a simple test suffices. Or does the camera also get
		* self-hits? (No, and the raysensor shouldn't do it either, since
		* self-hits are excluded by setting the correct ignore-object.)
		* Hitspots now become valid. */
		if (hitKXObj == thisObj)
		{
			m_hitPosition = resultpoint;
			m_hitNormal = resultnormal;
			return true;
		}
		
		return false;
	}

	return false;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_MouseFocusSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_MouseFocusSensor",
	sizeof(KX_MouseFocusSensor),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0, //&MyPyCompare,
	__repr,
	0, //&cvalue_as_number,
	0,
	0,
	0,
	0
};

PyParentObject KX_MouseFocusSensor::Parents[] = {
	&KX_MouseFocusSensor::Type,
	&SCA_MouseSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_MouseFocusSensor::Methods[] = {
	{"getRayTarget", (PyCFunction) KX_MouseFocusSensor::sPyGetRayTarget, 
	 METH_VARARGS, GetRayTarget_doc},
	{"getRaySource", (PyCFunction) KX_MouseFocusSensor::sPyGetRaySource, 
	 METH_VARARGS, GetRaySource_doc},
	{NULL,NULL} //Sentinel
};

PyObject* KX_MouseFocusSensor::_getattr(char* attr) {
	_getattr_up(SCA_MouseSensor);
}

/*  getRayTarget                                                */
char KX_MouseFocusSensor::GetRayTarget_doc[] = 
"getRayTarget()\n"
"\tReturns the target of the ray that seeks the focus object,\n"
"\tin worldcoordinates.";
PyObject* KX_MouseFocusSensor::PyGetRayTarget(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds) {
	PyObject *retVal = PyList_New(3);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_prevTargetPoint[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_prevTargetPoint[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_prevTargetPoint[2]));
	
	return retVal;
}

/*  getRayTarget                                                */
char KX_MouseFocusSensor::GetRaySource_doc[] = 
"getRaySource()\n"
"\tReturns the source of the ray that seeks the focus object,\n"
"\tin worldcoordinates.";
PyObject* KX_MouseFocusSensor::PyGetRaySource(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds) {
	PyObject *retVal = PyList_New(3);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_prevSourcePoint[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_prevSourcePoint[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_prevSourcePoint[2]));
	
	return retVal;
}

/* eof */

