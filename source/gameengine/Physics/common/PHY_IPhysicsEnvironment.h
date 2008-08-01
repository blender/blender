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

#ifndef _IPHYSICSENVIRONMENT
#define _IPHYSICSENVIRONMENT

#include <vector>
#include "PHY_DynamicTypes.h"
class PHY_IVehicle;

/**
*	Physics Environment takes care of stepping the simulation and is a container for physics entities (rigidbodies,constraints, materials etc.)
*	A derived class may be able to 'construct' entities by loading and/or converting
*/
class PHY_IPhysicsEnvironment
{
	public:
		virtual		~PHY_IPhysicsEnvironment();
		virtual	void		beginFrame() = 0;
		virtual void		endFrame() = 0;
		/// Perform an integration step of duration 'timeStep'.
		virtual	bool		proceedDeltaTime(double curTime,float timeStep)=0;
		virtual	void		setFixedTimeStep(bool useFixedTimeStep,float fixedTimeStep)=0;
		//returns 0.f if no fixed timestep is used
		virtual	float		getFixedTimeStep()=0;

		///setDebugMode is used to support several ways of debug lines, contact point visualization
		virtual void		setDebugMode(int debugMode) {}
		///setNumIterations set the number of iterations for iterative solvers
		virtual void		setNumIterations(int numIter) {}
		///setNumTimeSubSteps set the number of divisions of the timestep. Tradeoff quality versus performance.
		virtual void		setNumTimeSubSteps(int numTimeSubSteps){}
		///setDeactivationTime sets the minimum time that an objects has to stay within the velocity tresholds until it gets fully deactivated
		virtual void		setDeactivationTime(float dTime) {}
		///setDeactivationLinearTreshold sets the linear velocity treshold, see setDeactivationTime
		virtual	void		setDeactivationLinearTreshold(float linTresh) {}
		///setDeactivationAngularTreshold sets the angular velocity treshold, see setDeactivationTime
		virtual	void		setDeactivationAngularTreshold(float angTresh) {}
		///setContactBreakingTreshold sets tresholds to do with contact point management
		virtual void		setContactBreakingTreshold(float contactBreakingTreshold) {}
		///continuous collision detection mode, very experimental for Bullet
		virtual void		setCcdMode(int ccdMode) {}
		///successive overrelaxation constant, in case PSOR is used, values in between 1 and 2 guarantee converging behaviour
		virtual void		setSolverSorConstant(float sor) {}
		///setSolverType, internal setting, chooses solvertype, PSOR, Dantzig, impulse based, penalty based
		virtual void		setSolverType(int solverType) {}
		///setTau sets the spring constant of a penalty based solver
		virtual void		setSolverTau(float tau) {}
		///setDamping sets the damper constant of a penalty based solver
		virtual void		setSolverDamping(float damping) {}
		///linear air damping for rigidbodies
		virtual void		setLinearAirDamping(float damping) {}
		/// penetrationdepth setting
		virtual void		setUseEpa(bool epa) {}

		virtual	void		setGravity(float x,float y,float z)=0;

		virtual int			createConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
			float pivotX,float pivotY,float pivotZ,
			float axis0X,float axis0Y,float axis0Z,
			float axis1X=0,float axis1Y=0,float axis1Z=0,
			float axis2X=0,float axis2Y=0,float axis2Z=0
		)=0;
		virtual void		removeConstraint(int	constraintid)=0;
		virtual float		getAppliedImpulse(int	constraintid){ return 0.f;}


		//complex constraint for vehicles
		virtual PHY_IVehicle*	getVehicleConstraint(int constraintId) =0;

		virtual PHY_IPhysicsController* rayTest(PHY_IPhysicsController* ignoreClient, float fromX,float fromY,float fromZ, float toX,float toY,float toZ, 
										float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ)=0;


		//Methods for gamelogic collision/physics callbacks
		//todo:
		virtual void addSensor(PHY_IPhysicsController* ctrl)=0;
		virtual void removeSensor(PHY_IPhysicsController* ctrl)=0;
		virtual void addTouchCallback(int response_class, PHY_ResponseCallback callback, void *user)=0;
		virtual void requestCollisionCallback(PHY_IPhysicsController* ctrl)=0;
		virtual void removeCollisionCallback(PHY_IPhysicsController* ctrl)=0;
		//These two methods are *solely* used to create controllers for sensor! Don't use for anything else
		virtual PHY_IPhysicsController*	CreateSphereController(float radius,const PHY__Vector3& position) =0;
		virtual PHY_IPhysicsController* CreateConeController(float coneradius,float coneheight)=0;
		
		virtual void	setConstraintParam(int constraintId,int param,float value,float value1) = 0;
};

#endif //_IPHYSICSENVIRONMENT

