/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * The contents of this file may be used under the terms of either the GNU
 * General Public License Version 2 or later (the "GPL", see
 * http://www.gnu.org/licenses/gpl.html ), or the Blender License 1.0 or
 * later (the "BL", see http://www.blender.org/BL/ ) which has to be
 * bought from the Blender Foundation to become active, in which case the
 * above mentioned GPL option does not apply.
 *
 * The Original Code is Copyright (C) 2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef _ODEPHYSICSENVIRONMENT
#define _ODEPHYSICSENVIRONMENT


#include "PHY_IPhysicsEnvironment.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/**
*	Physics Environment takes care of stepping the simulation and is a container for physics entities (rigidbodies,constraints, materials etc.)
*	A derived class may be able to 'construct' entities by loading and/or converting
*/
class ODEPhysicsEnvironment : public PHY_IPhysicsEnvironment
{

public:
	ODEPhysicsEnvironment();
	virtual		~ODEPhysicsEnvironment();
// Perform an integration step of duration 'timeStep'.
	virtual	void		proceed(double	timeStep);
	virtual	void		setGravity(float x,float y,float z);
	virtual int			createConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
			float pivotX,float pivotY,float pivotZ,
			float axisX,float axisY,float axisZ);

	virtual void		removeConstraint(int constraintid);
	virtual PHY_IPhysicsController* rayTest(void* ignoreClient,float fromX,float fromY,float fromZ, float toX,float toY,float toZ, 
									float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ);

	struct dxWorld*	GetOdeWorld() { return m_OdeWorld;	};
	struct	dxSpace* GetOdeSpace() { return m_OdeSpace;};

private:


	// ODE physics response
	struct	dxWorld*				m_OdeWorld;
	// ODE collision detection
	struct	dxSpace*				m_OdeSpace;
	void	ClearOdeContactGroup();
	struct dxJointGroup*		m_OdeContactGroup;
	struct dxJointGroup*		m_JointGroup;

	static void OdeNearCallback(void *data, struct dxGeom* o1, struct dxGeom* o2);
	int	GetNumOdeContacts();

};

#endif //_ODEPHYSICSENVIRONMENT

