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

/** \file DummyPhysicsEnvironment.h
 *  \ingroup physdummy
 */

#ifndef __DUMMYPHYSICSENVIRONMENT_H__
#define __DUMMYPHYSICSENVIRONMENT_H__

#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IMotionState.h"

/**
 * DummyPhysicsEnvironment  is an empty placeholder
 * Alternatives are ODE,Sumo and Dynamo PhysicsEnvironments
 * Use DummyPhysicsEnvironment as a base to integrate your own physics engine
 * Physics Environment takes care of stepping the simulation and is a container for physics entities (rigidbodies,constraints, materials etc.)
 *
 * A derived class may be able to 'construct' entities by loading and/or converting
 */
class DummyPhysicsEnvironment  : public PHY_IPhysicsEnvironment
{

public:
	DummyPhysicsEnvironment ();
	virtual		~DummyPhysicsEnvironment ();
	virtual void		BeginFrame();
	virtual void		EndFrame();
// Perform an integration step of duration 'timeStep'.
	virtual	bool		ProceedDeltaTime(double  curTime,float timeStep,float interval);
	virtual	void		SetFixedTimeStep(bool useFixedTimeStep,float fixedTimeStep);
	virtual	float		GetFixedTimeStep();

	virtual	void		SetGravity(float x,float y,float z);
	virtual	void		GetGravity(class MT_Vector3& grav);

	virtual int			CreateConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
			float pivotX,float pivotY,float pivotZ,
			float axisX,float axisY,float axisZ,
			float axis1X=0,float axis1Y=0,float axis1Z=0,
			float axis2X=0,float axis2Y=0,float axis2Z=0,int flag=0
			);

	virtual void		RemoveConstraint(int	constraintid);

		//complex constraint for vehicles
	virtual PHY_IVehicle*	GetVehicleConstraint(int constraintId)
	{
		return 0;
	}

		// Character physics wrapper
	virtual PHY_ICharacter*	GetCharacterController(class KX_GameObject* ob)
	{
		return 0;
	}

	virtual PHY_IPhysicsController* RayTest(PHY_IRayCastFilterCallback &filterCallback, float fromX,float fromY,float fromZ, float toX,float toY,float toZ);
	virtual bool CullingTest(PHY_CullingCallback callback, void* userData, class MT_Vector4* planes, int nplanes, int occlusionRes, const int *viewport, double modelview[16], double projection[16]) { return false; }


	//gamelogic callbacks
		virtual void AddSensor(PHY_IPhysicsController* ctrl) {}
		virtual void RemoveSensor(PHY_IPhysicsController* ctrl) {}
		virtual void AddTouchCallback(int response_class, PHY_ResponseCallback callback, void *user)
		{
		}
		virtual bool RequestCollisionCallback(PHY_IPhysicsController* ctrl) { return false; }
		virtual bool RemoveCollisionCallback(PHY_IPhysicsController* ctrl) { return false;}
		virtual PHY_IPhysicsController*	CreateSphereController(float radius,const class MT_Vector3& position) {return 0;}
		virtual PHY_IPhysicsController* CreateConeController(float coneradius,float coneheight) { return 0;}

		virtual void	SetConstraintParam(int constraintId,int param,float value,float value1)
		{
		}

		virtual float	GetConstraintParam(int constraintId,int param)
		{
			return 0.f;
		}

	virtual void MergeEnvironment(PHY_IPhysicsEnvironment *other_env)
	{
		// Dummy, nothing to do here
	}

	virtual void ConvertObject(KX_GameObject* gameobj,
						RAS_MeshObject* meshobj,
						DerivedMesh* dm,
						KX_Scene* kxscene,
						PHY_ShapeProps* shapeprops,
						PHY_MaterialProps*	smmaterial,
						PHY_IMotionState *motionstate,
						int activeLayerBitInfo,
						bool isCompoundChild,
						bool hasCompoundChildren)
	{
		// All we need to do is handle the motionstate (we're supposed to own it)
		delete motionstate;
	}
		
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:DummyPhysicsEnvironment")
#endif
};

#endif  /* __DUMMYPHYSICSENVIRONMENT_H__ */
