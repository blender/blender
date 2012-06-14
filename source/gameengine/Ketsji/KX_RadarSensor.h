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

/** \file KX_RadarSensor.h
 *  \ingroup ketsji
 */

#ifndef __KX_RADARSENSOR_H__
#define __KX_RADARSENSOR_H__

#include "KX_NearSensor.h"
#include "MT_Point3.h"

/**
 * Radar 'cone' sensor. Very similar to a near-sensor, but instead of a sphere, a cone is used.
 */
class KX_RadarSensor : public KX_NearSensor
{
 protected:
	Py_Header
		
	float		m_coneradius;

	/**
	 * Height of the cone.
	 */
	float		m_coneheight;
	int				m_axis;

	/**
	 * The previous position of the origin of the cone.
	 */
	float       m_cone_origin[3];

	/**
	 * The previous direction of the cone (origin to bottom plane).
	 */
	float       m_cone_target[3];
	
public:

	KX_RadarSensor(SCA_EventManager* eventmgr,
		KX_GameObject* gameobj,
			PHY_IPhysicsController* physCtrl,
			double coneradius,
			double coneheight,
			int	axis,
			double margin,
			double resetmargin,
			bool bFindMaterial,
			const STR_String& touchedpropname);
	KX_RadarSensor();
	virtual ~KX_RadarSensor();
	virtual void SynchronizeTransform();
	virtual CValue* GetReplica();

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
	enum RadarAxis {
		KX_RADAR_AXIS_POS_X = 0,
		KX_RADAR_AXIS_POS_Y,
		KX_RADAR_AXIS_POS_Z,
		KX_RADAR_AXIS_NEG_X,
		KX_RADAR_AXIS_NEG_Y,
		KX_RADAR_AXIS_NEG_Z
	};

	/* python */
	virtual sensortype GetSensorType() { return ST_RADAR; }

	static PyObject*	pyattr_get_angle(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
};

#endif //__KX_RADARSENSOR_H__

