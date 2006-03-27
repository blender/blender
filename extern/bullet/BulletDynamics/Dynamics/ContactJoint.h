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

#ifndef CONTACT_JOINT_H
#define CONTACT_JOINT_H

#include "BU_Joint.h"
class RigidBody;
class PersistentManifold;

class ContactJoint : public BU_Joint
{
	PersistentManifold* m_manifold;
	int		m_index;
	bool	m_swapBodies;
	RigidBody* m_body0;
	RigidBody* m_body1;


public:

	ContactJoint() {};

	ContactJoint(PersistentManifold* manifold,int index,bool swap,RigidBody* body0,RigidBody* body1);

	//BU_Joint interface for solver

	virtual void GetInfo1(Info1 *info);

	virtual void GetInfo2(Info2 *info);


	

};

#endif //CONTACT_JOINT_H

