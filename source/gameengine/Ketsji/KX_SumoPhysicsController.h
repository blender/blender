#ifndef __KX_SUMOPHYSICSCONTROLLER_H
#define __KX_SUMOPHYSICSCONTROLLER_H

#include "PHY_IPhysicsController.h"
#include "SM_Object.h" // for SM_Callback

/**
	Physics Controller, a special kind of Scene Graph Transformation Controller.
	It get's callbacks from Sumo in case a transformation change took place.
	Each time the scene graph get's updated, the controller get's a chance
	in the 'Update' method to reflect changed.
*/

#include "SumoPhysicsController.h"
#include "KX_IPhysicsController.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

class KX_SumoPhysicsController : public KX_IPhysicsController,	
									public SumoPhysicsController

{


public:
	KX_SumoPhysicsController(
		class SM_Scene* sumoScene,
		DT_SceneHandle solidscene,
		class SM_Object* sumoObj,	
		class PHY_IMotionState* motionstate
		,bool dyna) 
		: SumoPhysicsController(sumoScene,solidscene,sumoObj,motionstate,dyna),
	KX_IPhysicsController(dyna,NULL) 
	{
	};
	virtual ~KX_SumoPhysicsController();

	void	applyImpulse(const MT_Point3& attach, const MT_Vector3& impulse);
	virtual void	SetObject (SG_IObject* object);

	
	void	RelativeTranslate(const MT_Vector3& dloc,bool local);
	void	RelativeRotate(const MT_Matrix3x3& drot,bool local);
	void	ApplyTorque(const MT_Vector3& torque,bool local);
	void	ApplyForce(const MT_Vector3& force,bool local);
	MT_Vector3 GetLinearVelocity();
	MT_Vector3 GetVelocity(const MT_Point3& pos);
	void	SetAngularVelocity(const MT_Vector3& ang_vel,bool local);
	void	SetLinearVelocity(const MT_Vector3& lin_vel,bool local);

	void	SuspendDynamics();
	void	RestoreDynamics();
	virtual	void	getOrientation(MT_Quaternion& orn);
	virtual	void setOrientation(const MT_Quaternion& orn);
	
	virtual	void setPosition(const MT_Point3& pos);
	virtual	void setScaling(const MT_Vector3& scaling);
	virtual	MT_Scalar	GetMass();
	virtual	MT_Vector3	getReactionForce();
	virtual	void	setRigidBody(bool rigid);
	

	virtual	SG_Controller*	GetReplica(class SG_Node* destnode);

	
	void	SetSumoTransform(bool nondynaonly);
	// todo: remove next line !
	virtual void	SetSimulatedTime(double time);
	
	// call from scene graph to update
	virtual bool Update(double time);

		void
	SetOption(
		int option,
		int value
	){
		// intentionally empty
	};


};

#endif //__KX_SUMOPHYSICSCONTROLLER_H

