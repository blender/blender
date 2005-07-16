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