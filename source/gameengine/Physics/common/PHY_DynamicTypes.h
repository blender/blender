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
 */
#ifndef __PHY_DYNAMIC_TYPES
#define __PHY_DYNAMIC_TYPES

/// PHY_ScalarType enumerates possible scalar types.
/// See the PHY_IMeshInterface for its use
typedef enum PHY_ScalarType {
	PHY_FLOAT,
	PHY_DOUBLE,
	PHY_INTEGER,
	PHY_SHORT,
	PHY_FIXEDPOINT88
} PHY_ScalarType;

class	PHY_ResponseTable;

class PHY_Shape;

struct	PHY__Vector3
{
	float	m_vec[4];
	operator const float* () const 
	{ 
		return &m_vec[0];
	}	
	operator float* () 
	{ 
		return &m_vec[0];
	}	
};
//typedef 	float	PHY__Vector3[4];

typedef enum
{
	PHY_FH_RESPONSE,
	PHY_SENSOR_RESPONSE,		/* Touch Sensors */
	PHY_CAMERA_RESPONSE,	/* Visibility Culling */
	PHY_OBJECT_RESPONSE,	/* Object Dynamic Geometry Response */
	PHY_STATIC_RESPONSE,	/* Static Geometry Response */
	
	PHY_NUM_RESPONSE
};

	typedef struct PHY_CollData {
		PHY__Vector3 m_point1;               /* Point in object1 in world coordinates */ 
		PHY__Vector3 m_point2;               /* Point in object2 in world coordinates */
		PHY__Vector3 m_normal;               /* point2 - point1 */ 
	} PHY_CollData;

/* A response callback is called by SOLID for each pair of collding objects. 'client-data'
   is a pointer to an arbitrary structure in the client application. The client objects are
   pointers to structures in the client application associated with the coliding objects.
   'coll_data' is the collision data computed by SOLID.
*/

	typedef bool (*PHY_ResponseCallback)(void *client_data,
										   void *client_object1,
										   void *client_object2,
										   const PHY_CollData *coll_data);
		


/// PHY_PhysicsType enumerates all possible Physics Entities.
/// It is mainly used to create/add Physics Objects

typedef enum PHY_PhysicsType {
	PHY_CONVEX_RIGIDBODY=16386,
	PHY_CONCAVE_RIGIDBODY=16399,
	PHY_CONVEX_FIXEDBODY=16388,//'collision object'
	PHY_CONCAVE_FIXEDBODY=16401,
	PHY_CONVEX_KINEMATICBODY=16387,// 
	PHY_CONCAVE_KINEMATICBODY=16400,
	PHY_CONVEX_PHANTOMBODY=16398,
	PHY_CONCAVE_PHANTOMBODY=16402
} PHY_PhysicsType;

/// PHY_ConstraintType enumerates all supported Constraint Types
typedef enum PHY_ConstraintType {
	PHY_POINT2POINT_CONSTRAINT=1,
	PHY_LINEHINGE_CONSTRAINT

} PHY_ConstraintType;

typedef float	PHY_Vector3[3];

#endif //__PHY_DYNAMIC_TYPES

