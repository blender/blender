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

#ifndef __KX_MOUSEFOCUSSENSOR
#define __KX_MOUSEFOCUSSENSOR

#include "SCA_MouseSensor.h"

/**
 * The mouse focus sensor extends the basic SCA_MouseSensor. It has
 * been placed in KX because it needs access to the rasterizer and
 * SuMO.
 *
 * - extend the valid modes?
 * - */
class KX_MouseFocusSensor : public SCA_MouseSensor
{

	Py_Header;
	
 public:
	
	KX_MouseFocusSensor(class SCA_MouseManager* keybdmgr,
						int startx,
						int starty,
						short int mousemode,
						bool focusmode,
						RAS_ICanvas* canvas,
						KX_Scene* kxscene,
						SCA_IObject* gameobj,
						PyTypeObject* T=&Type );

	virtual ~KX_MouseFocusSensor() { ; };
	virtual CValue* GetReplica() {
		CValue* replica = new KX_MouseFocusSensor(*this);
		// this will copy properties and so on...
		CValue::AddDataToReplica(replica);
		return replica;
	};
	/**
	 * @attention Overrides default evaluate. 
	 */
	virtual bool Evaluate(CValue* event);

	virtual bool IsPositiveTrigger() {
		bool result = m_positive_event;
		if (m_invert) result = !result;
		return result;
	};

	
	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
	virtual PyObject*  _getattr(char *attr);

	KX_PYMETHOD_DOC(KX_MouseFocusSensor,GetRayTarget);
	KX_PYMETHOD_DOC(KX_MouseFocusSensor,GetRaySource);

	/* --------------------------------------------------------------------- */

 private:
	/**
	 * The focus mode. True for handling focus, false for not handling
	 * it. */
	bool m_focusmode;

	/**
	 * Flags whether the previous test showed a mouse-over.
	 */
	bool m_mouse_over_in_previous_frame;

	/**
	 * Flags whether the previous test evaluated positive.
	 */
	bool m_positive_event;

	
	/**
	 * Tests whether the object is in mouse focus in this frame.
	 */
	bool ParentObjectHasFocus(void);

	/**
	 * (in game world coordinates) the place where the object was hit.
	 */
	MT_Point3		 m_hitPosition;

	/**
	 * (in game world coordinates) the position to which to shoot the ray.
	 */
	MT_Point3		 m_prevTargetPoint;

	/**
	 * (in game world coordinates) the position from which to shoot the ray.
	 */
	MT_Point3		 m_prevSourcePoint;
	
	/**
	 * (in game world coordinates) the face normal of the vertex where
	 * the object was hit.  */
	MT_Vector3		 m_hitNormal;

	/**
	 * Ref to the engine, for retrieving a reference to the current
	 * scene.  */
	class KX_KetsjiEngine* m_engine;

	/**
	 * The active canvas. The size of this canvas determines a part of
	 * the start position of the picking ray.  */
	RAS_ICanvas* m_gp_canvas;

	/**
	 * The KX scene that holds the camera. The camera position
	 * determines a part of the start location of the picking ray.  */
	KX_Scene* m_kxscene;

};

#endif //__KX_MOUSESENSOR

