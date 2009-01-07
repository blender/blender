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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef SM_FHOBJECT_H
#define SM_FHOBJECT_H

#include "SM_Object.h"

class SM_FhObject : public SM_Object {
public:
	virtual ~SM_FhObject();
	SM_FhObject(DT_ShapeHandle rayshape, MT_Vector3 ray, SM_Object *parent_object);

	const MT_Vector3&  getRay()          const { return m_ray; }
	MT_Point3          getSpot()         const { return getPosition() + m_ray; }
	const MT_Vector3&  getRayDirection() const { return m_ray_direction; }
	SM_Object         *getParentObject() const { return m_parent_object; }

	static DT_Bool ray_hit(void *client_data,  
		void *object1,
		void *object2,
		const DT_CollData *coll_data);

private:
	MT_Vector3      m_ray;
	MT_Vector3      m_ray_direction;
	SM_Object      *m_parent_object;
};

#endif

