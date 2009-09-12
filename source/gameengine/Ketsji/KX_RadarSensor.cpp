/**
 * $Id$
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

#include "KX_RadarSensor.h"
#include "KX_GameObject.h"
#include "KX_PyMath.h"
#include "PHY_IPhysicsController.h"
#include "PHY_IMotionState.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * 	RadarSensor constructor. Creates a near-sensor derived class, with a cone collision shape.
 */
KX_RadarSensor::KX_RadarSensor(SCA_EventManager* eventmgr,
		KX_GameObject* gameobj,
		PHY_IPhysicsController* physCtrl,
			double coneradius,
			double coneheight,
			int	axis,
			double margin,
			double resetmargin,
			bool bFindMaterial,
			const STR_String& touchedpropname)

			: KX_NearSensor(
				eventmgr,
				gameobj,
				//DT_NewCone(coneradius,coneheight),
				margin,
				resetmargin,
				bFindMaterial,
				touchedpropname,
				physCtrl),

				m_coneradius(coneradius),
				m_coneheight(coneheight),
				m_axis(axis)
{
	m_client_info->m_type = KX_ClientObjectInfo::SENSOR;
	//m_client_info->m_clientobject = gameobj;
	//m_client_info->m_auxilary_info = NULL;
	//sumoObj->setClientObject(&m_client_info);
}
			
KX_RadarSensor::~KX_RadarSensor()
{
	
}

CValue* KX_RadarSensor::GetReplica()
{
	KX_RadarSensor* replica = new KX_RadarSensor(*this);
	replica->ProcessReplica();
	return replica;
}

/**
 *	Transforms the collision object. A cone is not correctly centered
 *	for usage.  */
void KX_RadarSensor::SynchronizeTransform()
{
	// Getting the parent location was commented out. Why?
	MT_Transform trans;
	trans.setOrigin(((KX_GameObject*)GetParent())->NodeGetWorldPosition());
	trans.setBasis(((KX_GameObject*)GetParent())->NodeGetWorldOrientation());
	// What is the default orientation? pointing in the -y direction?
	// is the geometry correctly converted?

	// a collision cone is oriented
	// center the cone correctly 
	// depends on the radar 'axis'
	switch (m_axis)
	{
	case 0: // +X Axis
		{
			MT_Quaternion rotquatje(MT_Vector3(0,0,1),MT_radians(90));
			trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	case 1: // +Y Axis
		{
			MT_Quaternion rotquatje(MT_Vector3(1,0,0),MT_radians(-180));
			trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	case 2: // +Z Axis
		{
			MT_Quaternion rotquatje(MT_Vector3(1,0,0),MT_radians(-90));
			trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	case 3: // -X Axis
		{
			MT_Quaternion rotquatje(MT_Vector3(0,0,1),MT_radians(-90));
			trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	case 4: // -Y Axis
		{
			//MT_Quaternion rotquatje(MT_Vector3(1,0,0),MT_radians(-180));
			//trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	case 5: // -Z Axis
		{
			MT_Quaternion rotquatje(MT_Vector3(1,0,0),MT_radians(90));
			trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	default:
		{
		}
	}
	
	//Using a temp variable to translate MT_Point3 to float[3].
	//float[3] works better for the Python interface.
	MT_Point3 temp = trans.getOrigin();
	m_cone_origin[0] = temp[0];
	m_cone_origin[1] = temp[1];
	m_cone_origin[2] = temp[2];

	temp = trans(MT_Point3(0, -m_coneheight/2.0 ,0));
	m_cone_target[0] = temp[0];
	m_cone_target[1] = temp[1];
	m_cone_target[2] = temp[2];


	if (m_physCtrl)
	{
		PHY_IMotionState* motionState = m_physCtrl->GetMotionState();
		const MT_Point3& pos = trans.getOrigin();
		float ori[12];
		trans.getBasis().getValue(ori);
		motionState->setWorldPosition(pos[0], pos[1], pos[2]);
		motionState->setWorldOrientation(ori);
		m_physCtrl->WriteMotionStateToDynamics(true);
	}

}

/* ------------------------------------------------------------------------- */
/* Python Functions															 */
/* ------------------------------------------------------------------------- */

/* none */

/* ------------------------------------------------------------------------- */
/* Python Integration Hooks                                                  */
/* ------------------------------------------------------------------------- */
PyTypeObject KX_RadarSensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_RadarSensor",
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
	&KX_NearSensor::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_RadarSensor::Methods[] = {
	{NULL} //Sentinel
};

PyAttributeDef KX_RadarSensor::Attributes[] = {
	KX_PYATTRIBUTE_FLOAT_ARRAY_RO("coneOrigin", KX_RadarSensor, m_cone_origin, 3),
	KX_PYATTRIBUTE_FLOAT_ARRAY_RO("coneTarget", KX_RadarSensor, m_cone_target, 3),
	KX_PYATTRIBUTE_FLOAT_RO("distance", KX_RadarSensor, m_coneheight),
	KX_PYATTRIBUTE_FLOAT_RW("angle", 0, 360, KX_RadarSensor, m_coneradius),
	KX_PYATTRIBUTE_INT_RW("axis", 0, 5, true, KX_RadarSensor, m_axis),
	{NULL} //Sentinel
};

