/*
 * Copyright (c) 2001-2005 Erwin Coumans <phy@erwincoumans.com>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Erwin Coumans makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef __PHY_DYNAMIC_TYPES
#define __PHY_DYNAMIC_TYPES



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
	PHY_LINEHINGE_CONSTRAINT,
	PHY_VEHICLE_CONSTRAINT

} PHY_ConstraintType;

typedef float	PHY_Vector3[3];

#endif //__PHY_DYNAMIC_TYPES

