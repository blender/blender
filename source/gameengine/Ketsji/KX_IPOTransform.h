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

/** \file KX_IPOTransform.h
 *  \ingroup ketsji
 *  \brief An abstract object you can move around in a 3d world, and has some logic
 */

#ifndef __KX_IPOTRANSFORM_H__
#define __KX_IPOTRANSFORM_H__

#include "MT_Transform.h"

class KX_IPOTransform {
public:
	KX_IPOTransform() :
		m_position(0.0, 0.0, 0.0),
		m_eulerAngles(0.0, 0.0, 0.0),
		m_scaling(1.0, 1.0, 1.0),
		m_deltaPosition(0.0, 0.0, 0.0),
		m_deltaEulerAngles(0.0, 0.0, 0.0),
		m_deltaScaling(0.0, 0.0, 0.0)
		{}

	MT_Transform         GetTransform() const {
		return MT_Transform(m_position + m_deltaPosition,
							MT_Matrix3x3(m_eulerAngles + m_deltaEulerAngles,
										 m_scaling + m_deltaScaling));
	}

	MT_Point3&	         GetPosition()          { return m_position; 	}
	MT_Vector3&          GetEulerAngles()       { return m_eulerAngles;	}
	MT_Vector3&          GetScaling()           { return m_scaling;	}

	const MT_Point3&	 GetPosition()    const { return m_position; 	}
	const MT_Vector3&    GetEulerAngles() const { return m_eulerAngles;	}
	const MT_Vector3&    GetScaling()     const { return m_scaling;	}
	
	MT_Vector3&          GetDeltaPosition()     { return m_deltaPosition; }
	MT_Vector3&          GetDeltaEulerAngles()  { return m_deltaEulerAngles; }
	MT_Vector3&          GetDeltaScaling()      { return m_deltaScaling; }
	
	void SetPosition(const MT_Point3& pos)      { m_position = pos; 	}
	void SetEulerAngles(const MT_Vector3& eul)  { m_eulerAngles = eul;	}
	void SetScaling(const MT_Vector3& scaling)  { m_scaling = scaling;	}
	
	void ClearDeltaStuff() { 
		m_deltaPosition.setValue(0.0, 0.0, 0.0);
		m_deltaEulerAngles.setValue(0.0, 0.0, 0.0);
		m_deltaScaling.setValue(0.0, 0.0, 0.0);
	}

protected:
	MT_Point3              m_position;
	MT_Vector3             m_eulerAngles;
	MT_Vector3             m_scaling;
	MT_Vector3             m_deltaPosition;
	MT_Vector3             m_deltaEulerAngles;
	MT_Vector3             m_deltaScaling;
};

#endif

