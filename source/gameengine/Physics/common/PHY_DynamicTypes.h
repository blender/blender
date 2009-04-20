/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#ifndef __PHY_DYNAMIC_TYPES
#define __PHY_DYNAMIC_TYPES



struct KX_ClientObjectInfo;
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

struct	PHY__Vector4
{
	float	m_vec[4];
	PHY__Vector4() {}
	void setValue(const float *value)
	{
		m_vec[0] = *value++;
		m_vec[1] = *value++;
		m_vec[2] = *value++;
		m_vec[3] = *value++;
	}
	void setValue(const double *value)
	{
		m_vec[0] = (float)(*value++);
		m_vec[1] = (float)(*value++);
		m_vec[2] = (float)(*value++);
		m_vec[3] = (float)(*value++);
	}

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

enum
{
	PHY_FH_RESPONSE,
	PHY_SENSOR_RESPONSE,	/* Touch Sensors */
	PHY_CAMERA_RESPONSE,	/* Visibility Culling */
	PHY_OBJECT_RESPONSE,	/* Object Dynamic Geometry Response */
	PHY_STATIC_RESPONSE,	/* Static Geometry Response */
	PHY_BROADPH_RESPONSE,	/* broadphase Response */
	
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
	typedef void (*PHY_CullingCallback)(KX_ClientObjectInfo* info, void* param);


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
	PHY_LINEHINGE_CONSTRAINT=2,
	PHY_ANGULAR_CONSTRAINT = 3,//hinge without ball socket
	PHY_CONE_TWIST_CONSTRAINT = 4,
	PHY_VEHICLE_CONSTRAINT=11,//complex 'constraint' that turns a rigidbody into a vehicle
	PHY_GENERIC_6DOF_CONSTRAINT=12,//can leave any of the 6 degree of freedom 'free' or 'locked'

} PHY_ConstraintType;

typedef enum PHY_ShapeType {
	PHY_SHAPE_NONE,
	PHY_SHAPE_BOX,
	PHY_SHAPE_SPHERE,
	PHY_SHAPE_CYLINDER,
	PHY_SHAPE_CONE,
	PHY_SHAPE_MESH,
	PHY_SHAPE_POLYTOPE,
	PHY_SHAPE_COMPOUND,
	PHY_SHAPE_PROXY
} PHY_ShapeType;


typedef float	PHY_Vector3[3];

#endif //__PHY_DYNAMIC_TYPES

