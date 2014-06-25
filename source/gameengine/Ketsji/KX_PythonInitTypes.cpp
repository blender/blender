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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_PythonInitTypes.cpp
 *  \ingroup ketsji
 */

#ifdef WITH_PYTHON

#include "KX_PythonInitTypes.h"

/* Only for Class::Parents */
#include "BL_BlenderShader.h"
#include "BL_ShapeActionActuator.h"
#include "BL_ArmatureActuator.h"
#include "BL_ArmatureConstraint.h"
#include "BL_ArmatureObject.h"
#include "BL_ArmatureChannel.h"
#include "KX_ArmatureSensor.h"
#include "KX_BlenderMaterial.h"
#include "KX_CameraActuator.h"
#include "KX_CharacterWrapper.h"
#include "KX_ConstraintActuator.h"
#include "KX_ConstraintWrapper.h"
#include "KX_GameActuator.h"
#include "KX_LibLoadStatus.h"
#include "KX_Light.h"
#include "KX_FontObject.h"
#include "KX_MeshProxy.h"
#include "KX_MouseFocusSensor.h"
#include "KX_NetworkMessageActuator.h"
#include "KX_NetworkMessageSensor.h"
#include "KX_ObjectActuator.h"
#include "KX_ParentActuator.h"
#include "KX_PolyProxy.h"
#include "KX_PythonSeq.h"
#include "KX_SCA_AddObjectActuator.h"
#include "KX_SCA_EndObjectActuator.h"
#include "KX_SCA_ReplaceMeshActuator.h"
#include "KX_SceneActuator.h"
#include "KX_StateActuator.h"
#include "KX_SteeringActuator.h"
#include "KX_TrackToActuator.h"
#include "KX_VehicleWrapper.h"
#include "KX_VertexProxy.h"
#include "SCA_2DFilterActuator.h"
#include "SCA_ANDController.h"
#include "SCA_ActuatorSensor.h"
#include "SCA_AlwaysSensor.h"
#include "SCA_DelaySensor.h"
#include "SCA_JoystickSensor.h"
#include "SCA_KeyboardSensor.h"
#include "SCA_MouseSensor.h"
#include "SCA_NANDController.h"
#include "SCA_NORController.h"
#include "SCA_ORController.h"
#include "SCA_RandomSensor.h"
#include "SCA_XNORController.h"
#include "SCA_XORController.h"
#include "SCA_PythonJoystick.h"
#include "SCA_PythonKeyboard.h"
#include "SCA_PythonMouse.h"
#include "KX_IpoActuator.h"
#include "KX_NearSensor.h"
#include "KX_RadarSensor.h"
#include "KX_RaySensor.h"
#include "KX_SCA_DynamicActuator.h"
#include "KX_SoundActuator.h"
#include "KX_TouchSensor.h"
#include "KX_VisibilityActuator.h"
#include "SCA_PropertySensor.h"
#include "SCA_PythonController.h"
#include "SCA_RandomActuator.h"
#include "SCA_IController.h"
#include "KX_NavMeshObject.h"
#include "KX_MouseActuator.h"

static void PyType_Attr_Set(PyGetSetDef *attr_getset, PyAttributeDef *attr)
{
	attr_getset->name= (char *)attr->m_name;
	attr_getset->doc= NULL;

	attr_getset->get= reinterpret_cast<getter>(PyObjectPlus::py_get_attrdef);

	if (attr->m_access==KX_PYATTRIBUTE_RO)
		attr_getset->set= NULL;
	else
		attr_getset->set= reinterpret_cast<setter>(PyObjectPlus::py_set_attrdef);

	attr_getset->closure= reinterpret_cast<void *>(attr);
}

static void PyType_Ready_ADD(PyObject *dict, PyTypeObject *tp, PyAttributeDef *attributes, PyAttributeDef *attributesPtr, int init_getset)
{
	PyAttributeDef *attr;

	if (init_getset) {
		/* we need to do this for all types before calling PyType_Ready
		 * since they will call the parents PyType_Ready and those might not have initialized vars yet */

		//if (tp->tp_base==NULL)
		//	printf("Debug: No Parents - '%s'\n" , tp->tp_name);

		if (tp->tp_getset==NULL && ((attributes && attributes->m_name) || (attributesPtr && attributesPtr->m_name))) {
			PyGetSetDef *attr_getset;
			int attr_tot= 0;

			if (attributes) {
				for (attr= attributes; attr->m_name; attr++, attr_tot++)
					attr->m_usePtr = false;
			}
			if (attributesPtr) {
				for (attr= attributesPtr; attr->m_name; attr++, attr_tot++)
					attr->m_usePtr = true;
			}

			tp->tp_getset = attr_getset = reinterpret_cast<PyGetSetDef *>(PyMem_Malloc((attr_tot+1) * sizeof(PyGetSetDef))); // XXX - Todo, free

			if (attributes) {
				for (attr= attributes; attr->m_name; attr++, attr_getset++) {
					PyType_Attr_Set(attr_getset, attr);
				}
			}
			if (attributesPtr) {
				for (attr= attributesPtr; attr->m_name; attr++, attr_getset++) {
					PyType_Attr_Set(attr_getset, attr);
				}
			}
			memset(attr_getset, 0, sizeof(PyGetSetDef));
		}
	} else {
		PyType_Ready(tp);
		PyDict_SetItemString(dict, tp->tp_name, reinterpret_cast<PyObject *>(tp));
	}
	
}


#define PyType_Ready_Attr(d, n, i)   PyType_Ready_ADD(d, &n::Type, n::Attributes, NULL, i)
#define PyType_Ready_AttrPtr(d, n, i)   PyType_Ready_ADD(d, &n::Type, n::Attributes, n::AttributesPtr, i)

void initPyTypes(void)
{

/*
 * initPyObjectPlusType(BL_ActionActuator::Parents);
 * .....
 */

	/* For now just do PyType_Ready */
	PyObject *mod = PyModule_New("GameTypes");
	PyObject *dict = PyModule_GetDict(mod);
	PyDict_SetItemString(PySys_GetObject("modules"), "GameTypes", mod);
	Py_DECREF(mod);
	
	
	for (int init_getset= 1; init_getset > -1; init_getset--) { /* run twice, once to init the getsets another to run PyType_Ready */
		PyType_Ready_Attr(dict, BL_ActionActuator, init_getset);
		PyType_Ready_Attr(dict, BL_Shader, init_getset);
		PyType_Ready_Attr(dict, BL_ShapeActionActuator, init_getset);
		PyType_Ready_Attr(dict, BL_ArmatureObject, init_getset);
		PyType_Ready_Attr(dict, BL_ArmatureActuator, init_getset);
		PyType_Ready_Attr(dict, BL_ArmatureConstraint, init_getset);
		PyType_Ready_AttrPtr(dict, BL_ArmatureBone, init_getset);
		PyType_Ready_AttrPtr(dict, BL_ArmatureChannel, init_getset);
		// PyType_Ready_Attr(dict, CPropValue, init_getset);  // doesn't use Py_Header
		PyType_Ready_Attr(dict, CListValue, init_getset);
		PyType_Ready_Attr(dict, CValue, init_getset);
		PyType_Ready_Attr(dict, KX_ArmatureSensor, init_getset);
		PyType_Ready_Attr(dict, KX_BlenderMaterial, init_getset);
		PyType_Ready_Attr(dict, KX_Camera, init_getset);
		PyType_Ready_Attr(dict, KX_CameraActuator, init_getset);
		PyType_Ready_Attr(dict, KX_CharacterWrapper, init_getset);
		PyType_Ready_Attr(dict, KX_ConstraintActuator, init_getset);
		PyType_Ready_Attr(dict, KX_ConstraintWrapper, init_getset);
		PyType_Ready_Attr(dict, KX_GameActuator, init_getset);
		PyType_Ready_Attr(dict, KX_GameObject, init_getset);
		PyType_Ready_Attr(dict, KX_IpoActuator, init_getset);
		PyType_Ready_Attr(dict, KX_LibLoadStatus, init_getset);
		PyType_Ready_Attr(dict, KX_LightObject, init_getset);
		PyType_Ready_Attr(dict, KX_FontObject, init_getset);
		PyType_Ready_Attr(dict, KX_MeshProxy, init_getset);
		PyType_Ready_Attr(dict, KX_MouseFocusSensor, init_getset);
		PyType_Ready_Attr(dict, KX_NearSensor, init_getset);
		PyType_Ready_Attr(dict, KX_NetworkMessageActuator, init_getset);
		PyType_Ready_Attr(dict, KX_NetworkMessageSensor, init_getset);
		PyType_Ready_Attr(dict, KX_ObjectActuator, init_getset);
		PyType_Ready_Attr(dict, KX_ParentActuator, init_getset);
		PyType_Ready_Attr(dict, KX_PolyProxy, init_getset);
		PyType_Ready_Attr(dict, KX_RadarSensor, init_getset);
		PyType_Ready_Attr(dict, KX_RaySensor, init_getset);
		PyType_Ready_Attr(dict, KX_SCA_AddObjectActuator, init_getset);
		PyType_Ready_Attr(dict, KX_SCA_DynamicActuator, init_getset);
		PyType_Ready_Attr(dict, KX_SCA_EndObjectActuator, init_getset);
		PyType_Ready_Attr(dict, KX_SCA_ReplaceMeshActuator, init_getset);
		PyType_Ready_Attr(dict, KX_Scene, init_getset);
		PyType_Ready_Attr(dict, KX_NavMeshObject, init_getset);
		PyType_Ready_Attr(dict, KX_SceneActuator, init_getset);
		PyType_Ready_Attr(dict, KX_SoundActuator, init_getset);
		PyType_Ready_Attr(dict, KX_StateActuator, init_getset);
		PyType_Ready_Attr(dict, KX_SteeringActuator, init_getset);
		PyType_Ready_Attr(dict, KX_TouchSensor, init_getset);
		PyType_Ready_Attr(dict, KX_TrackToActuator, init_getset);
		PyType_Ready_Attr(dict, KX_VehicleWrapper, init_getset);
		PyType_Ready_Attr(dict, KX_VertexProxy, init_getset);
		PyType_Ready_Attr(dict, KX_VisibilityActuator, init_getset);
		PyType_Ready_Attr(dict, KX_MouseActuator, init_getset);
		PyType_Ready_Attr(dict, PyObjectPlus, init_getset);
		PyType_Ready_Attr(dict, SCA_2DFilterActuator, init_getset);
		PyType_Ready_Attr(dict, SCA_ANDController, init_getset);
		// PyType_Ready_Attr(dict, SCA_Actuator, init_getset);  // doesn't use Py_Header
		PyType_Ready_Attr(dict, SCA_ActuatorSensor, init_getset);
		PyType_Ready_Attr(dict, SCA_AlwaysSensor, init_getset);
		PyType_Ready_Attr(dict, SCA_DelaySensor, init_getset);
		PyType_Ready_Attr(dict, SCA_ILogicBrick, init_getset);
		PyType_Ready_Attr(dict, SCA_IObject, init_getset);
		PyType_Ready_Attr(dict, SCA_ISensor, init_getset);
		PyType_Ready_Attr(dict, SCA_JoystickSensor, init_getset);
		PyType_Ready_Attr(dict, SCA_KeyboardSensor, init_getset);
		PyType_Ready_Attr(dict, SCA_MouseSensor, init_getset);
		PyType_Ready_Attr(dict, SCA_NANDController, init_getset);
		PyType_Ready_Attr(dict, SCA_NORController, init_getset);
		PyType_Ready_Attr(dict, SCA_ORController, init_getset);
		PyType_Ready_Attr(dict, SCA_PropertyActuator, init_getset);
		PyType_Ready_Attr(dict, SCA_PropertySensor, init_getset);
		PyType_Ready_Attr(dict, SCA_PythonController, init_getset);
		PyType_Ready_Attr(dict, SCA_RandomActuator, init_getset);
		PyType_Ready_Attr(dict, SCA_RandomSensor, init_getset);
		PyType_Ready_Attr(dict, SCA_XNORController, init_getset);
		PyType_Ready_Attr(dict, SCA_XORController, init_getset);
		PyType_Ready_Attr(dict, SCA_IController, init_getset);
		PyType_Ready_Attr(dict, SCA_PythonJoystick, init_getset);
		PyType_Ready_Attr(dict, SCA_PythonKeyboard, init_getset);
		PyType_Ready_Attr(dict, SCA_PythonMouse, init_getset);
	}


	/* Normal python type */
	PyType_Ready(&KX_PythonSeq_Type);

#ifdef USE_MATHUTILS
	/* Init mathutils callbacks */
	KX_GameObject_Mathutils_Callback_Init();
	KX_ObjectActuator_Mathutils_Callback_Init();
#endif
}

#endif // WITH_PYTHON
