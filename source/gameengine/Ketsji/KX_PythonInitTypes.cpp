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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */



#ifndef _adr_py_init_types_h_				// only process once,
#define _adr_py_init_types_h_				// even if multiply included

/* Only for Class::Parents */
#include "BL_BlenderShader.h"
#include "BL_ShapeActionActuator.h"
#include "KX_BlenderMaterial.h"
#include "KX_CDActuator.h"
#include "KX_CameraActuator.h"
#include "KX_ConstraintActuator.h"
#include "KX_ConstraintWrapper.h"
#include "KX_GameActuator.h"
#include "KX_Light.h"
#include "KX_MeshProxy.h"
#include "KX_MouseFocusSensor.h"
#include "KX_NetworkMessageActuator.h"
#include "KX_NetworkMessageSensor.h"
#include "KX_ObjectActuator.h"
#include "KX_ParentActuator.h"
#include "KX_PhysicsObjectWrapper.h"
#include "KX_PolyProxy.h"
#include "KX_PolygonMaterial.h"
#include "KX_SCA_AddObjectActuator.h"
#include "KX_SCA_EndObjectActuator.h"
#include "KX_SCA_ReplaceMeshActuator.h"
#include "KX_SceneActuator.h"
#include "KX_StateActuator.h"
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


void initPyObjectPlusType(PyTypeObject **parents)
{
	int i;

	for (i=0; parents[i]; i++) {
		if(PyType_Ready(parents[i]) < 0) {
			/* This is very very unlikely */
			printf("Error, pytype could not initialize, Blender may crash \"%s\"\n", parents[i]->tp_name);
			return;
		}

#if 0
		PyObject_Print(reinterpret_cast<PyObject *>parents[i], stderr, 0);
		fprintf(stderr, "\n");
		PyObject_Print(parents[i]->tp_dict, stderr, 0);
		fprintf(stderr, "\n\n");
#endif

	}

	 PyObject *dict= NULL;

	 while(i) {
		 i--;

		 if (dict) {
			PyDict_Update(parents[i]->tp_dict, dict);
		 }
		 dict= parents[i]->tp_dict;

#if 1
		PyObject_Print(reinterpret_cast<PyObject *>(parents[i]), stderr, 0);
		fprintf(stderr, "\n");
		PyObject_Print(parents[i]->tp_dict, stderr, 0);
		fprintf(stderr, "\n\n");
#endif

	}
}




static void PyType_Ready_ADD(PyObject *dict, PyTypeObject *tp, PyAttributeDef *attributes)
{
	PyAttributeDef *attr;
	PyObject *item;
	
	PyType_Ready(tp);
	PyDict_SetItemString(dict, tp->tp_name, reinterpret_cast<PyObject *>(tp));
	
	/* store attr defs in the tp_dict for to avoid string lookups */
	for(attr= attributes; attr->m_name; attr++) {
		item= PyCObject_FromVoidPtr(attr, NULL);
		PyDict_SetItemString(tp->tp_dict, attr->m_name, item);
		Py_DECREF(item);
	}
	
}


#define PyType_Ready_Attr(d, n)   PyType_Ready_ADD(d, &n::Type, n::Attributes)

void initPyTypes(void)
{
	
/*
	initPyObjectPlusType(BL_ActionActuator::Parents);
	.....
*/

	/* For now just do PyType_Ready */
	PyObject *mod= PyModule_New("GameTypes");
	PyObject *dict= PyModule_GetDict(mod);
	PyDict_SetItemString(PySys_GetObject((char *)"modules"), (char *)"GameTypes", mod);
	Py_DECREF(mod);
	
	PyType_Ready_Attr(dict, BL_ActionActuator);
	PyType_Ready_Attr(dict, BL_Shader);
	PyType_Ready_Attr(dict, BL_ShapeActionActuator);
	PyType_Ready_Attr(dict, CListValue);
	PyType_Ready_Attr(dict, CValue);
	PyType_Ready_Attr(dict, KX_BlenderMaterial);
	PyType_Ready_Attr(dict, KX_CDActuator);
	PyType_Ready_Attr(dict, KX_Camera);
	PyType_Ready_Attr(dict, KX_CameraActuator);
	PyType_Ready_Attr(dict, KX_ConstraintActuator);
	PyType_Ready_Attr(dict, KX_ConstraintWrapper);
	PyType_Ready_Attr(dict, KX_GameActuator);
	PyType_Ready_Attr(dict, KX_GameObject);
	PyType_Ready_Attr(dict, KX_IpoActuator);
	PyType_Ready_Attr(dict, KX_LightObject);
	PyType_Ready_Attr(dict, KX_MeshProxy);
	PyType_Ready_Attr(dict, KX_MouseFocusSensor);
	PyType_Ready_Attr(dict, KX_NearSensor);
	PyType_Ready_Attr(dict, KX_NetworkMessageActuator);
	PyType_Ready_Attr(dict, KX_NetworkMessageSensor);
	PyType_Ready_Attr(dict, KX_ObjectActuator);
	PyType_Ready_Attr(dict, KX_ParentActuator);
	PyType_Ready_Attr(dict, KX_PhysicsObjectWrapper);
	PyType_Ready_Attr(dict, KX_PolyProxy);
	PyType_Ready_Attr(dict, KX_PolygonMaterial);
	PyType_Ready_Attr(dict, KX_RadarSensor);
	PyType_Ready_Attr(dict, KX_RaySensor);
	PyType_Ready_Attr(dict, KX_SCA_AddObjectActuator);
	PyType_Ready_Attr(dict, KX_SCA_DynamicActuator);
	PyType_Ready_Attr(dict, KX_SCA_EndObjectActuator);
	PyType_Ready_Attr(dict, KX_SCA_ReplaceMeshActuator);
	PyType_Ready_Attr(dict, KX_Scene);
	PyType_Ready_Attr(dict, KX_SceneActuator);
	PyType_Ready_Attr(dict, KX_SoundActuator);
	PyType_Ready_Attr(dict, KX_StateActuator);
	PyType_Ready_Attr(dict, KX_TouchSensor);
	PyType_Ready_Attr(dict, KX_TrackToActuator);
	PyType_Ready_Attr(dict, KX_VehicleWrapper);
	PyType_Ready_Attr(dict, KX_VertexProxy);
	PyType_Ready_Attr(dict, KX_VisibilityActuator);
	PyType_Ready_Attr(dict, PyObjectPlus);
	PyType_Ready_Attr(dict, SCA_2DFilterActuator);
	PyType_Ready_Attr(dict, SCA_ANDController);
	PyType_Ready_Attr(dict, SCA_ActuatorSensor);
	PyType_Ready_Attr(dict, SCA_AlwaysSensor);
	PyType_Ready_Attr(dict, SCA_DelaySensor);
	PyType_Ready_Attr(dict, SCA_ILogicBrick);
	PyType_Ready_Attr(dict, SCA_IObject);
	PyType_Ready_Attr(dict, SCA_ISensor);
	PyType_Ready_Attr(dict, SCA_JoystickSensor);
	PyType_Ready_Attr(dict, SCA_KeyboardSensor);
	PyType_Ready_Attr(dict, SCA_MouseSensor);
	PyType_Ready_Attr(dict, SCA_NANDController);
	PyType_Ready_Attr(dict, SCA_NORController);
	PyType_Ready_Attr(dict, SCA_ORController);
	PyType_Ready_Attr(dict, SCA_PropertyActuator);
	PyType_Ready_Attr(dict, SCA_PropertySensor);
	PyType_Ready_Attr(dict, SCA_PythonController);
	PyType_Ready_Attr(dict, SCA_RandomActuator);
	PyType_Ready_Attr(dict, SCA_RandomSensor);
	PyType_Ready_Attr(dict, SCA_XNORController);
	PyType_Ready_Attr(dict, SCA_XORController);
	
	
	
}

#endif