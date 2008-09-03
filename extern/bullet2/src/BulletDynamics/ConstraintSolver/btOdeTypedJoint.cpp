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
#include "btOdeTypedJoint.h"
#include "btOdeSolverBody.h"
#include "btOdeMacros.h"
#include <stdio.h>

void btOdeTypedJoint::GetInfo1(Info1 *info)
{
    int joint_type = m_constraint->getConstraintType();
    switch (joint_type)
    {
    case POINT2POINT_CONSTRAINT_TYPE:
    {
        OdeP2PJoint p2pjoint(m_constraint,m_index,m_swapBodies,m_body0,m_body1);
        p2pjoint.GetInfo1(info);
    }
    break;
    case D6_CONSTRAINT_TYPE:
    {
        OdeD6Joint d6joint(m_constraint,m_index,m_swapBodies,m_body0,m_body1);
        d6joint.GetInfo1(info);
    }
    break;
    case SLIDER_CONSTRAINT_TYPE:
    {
        OdeSliderJoint sliderjoint(m_constraint,m_index,m_swapBodies,m_body0,m_body1);
        sliderjoint.GetInfo1(info);
    }
    break;
    };
}

void btOdeTypedJoint::GetInfo2(Info2 *info)
{
    int joint_type = m_constraint->getConstraintType();
    switch (joint_type)
    {
    case POINT2POINT_CONSTRAINT_TYPE:
    {
        OdeP2PJoint p2pjoint(m_constraint,m_index,m_swapBodies,m_body0,m_body1);
        p2pjoint.GetInfo2(info);
    }
    break;
    case D6_CONSTRAINT_TYPE:
    {
        OdeD6Joint d6joint(m_constraint,m_index,m_swapBodies,m_body0,m_body1);
        d6joint.GetInfo2(info);
    }
    break;
    case SLIDER_CONSTRAINT_TYPE:
    {
        OdeSliderJoint sliderjoint(m_constraint,m_index,m_swapBodies,m_body0,m_body1);
        sliderjoint.GetInfo2(info);
    }
    break;
    };
}


OdeP2PJoint::OdeP2PJoint(
    btTypedConstraint * constraint,
    int index,bool swap,btOdeSolverBody* body0,btOdeSolverBody* body1):
        btOdeTypedJoint(constraint,index,swap,body0,body1)
{
}


void OdeP2PJoint::GetInfo1(Info1 *info)
{
    info->m = 3;
    info->nub = 3;
}


void OdeP2PJoint::GetInfo2(Info2 *info)
{

    btPoint2PointConstraint * p2pconstraint = this->getP2PConstraint();

    //retrieve matrices
    btTransform body0_trans;
    if (m_body0)
    {
        body0_trans = m_body0->m_originalBody->getCenterOfMassTransform();
    }
//    btScalar body0_mat[12];
//    body0_mat[0] = body0_trans.getBasis()[0][0];
//    body0_mat[1] = body0_trans.getBasis()[0][1];
//    body0_mat[2] = body0_trans.getBasis()[0][2];
//    body0_mat[4] = body0_trans.getBasis()[1][0];
//    body0_mat[5] = body0_trans.getBasis()[1][1];
//    body0_mat[6] = body0_trans.getBasis()[1][2];
//    body0_mat[8] = body0_trans.getBasis()[2][0];
//    body0_mat[9] = body0_trans.getBasis()[2][1];
//    body0_mat[10] = body0_trans.getBasis()[2][2];

    btTransform body1_trans;

    if (m_body1)
    {
        body1_trans = m_body1->m_originalBody->getCenterOfMassTransform();
    }
//    btScalar body1_mat[12];
//    body1_mat[0] = body1_trans.getBasis()[0][0];
//    body1_mat[1] = body1_trans.getBasis()[0][1];
//    body1_mat[2] = body1_trans.getBasis()[0][2];
//    body1_mat[4] = body1_trans.getBasis()[1][0];
//    body1_mat[5] = body1_trans.getBasis()[1][1];
//    body1_mat[6] = body1_trans.getBasis()[1][2];
//    body1_mat[8] = body1_trans.getBasis()[2][0];
//    body1_mat[9] = body1_trans.getBasis()[2][1];
//    body1_mat[10] = body1_trans.getBasis()[2][2];




    // anchor points in global coordinates with respect to body PORs.


    int s = info->rowskip;

    // set jacobian
    info->J1l[0] = 1;
    info->J1l[s+1] = 1;
    info->J1l[2*s+2] = 1;


    btVector3 a1,a2;

    a1 = body0_trans.getBasis()*p2pconstraint->getPivotInA();
    //dMULTIPLY0_331 (a1, body0_mat,m_constraint->m_pivotInA);
    dCROSSMAT (info->J1a,a1,s,-,+);
    if (m_body1)
    {
        info->J2l[0] = -1;
        info->J2l[s+1] = -1;
        info->J2l[2*s+2] = -1;
        a2 = body1_trans.getBasis()*p2pconstraint->getPivotInB();
        //dMULTIPLY0_331 (a2,body1_mat,m_constraint->m_pivotInB);
        dCROSSMAT (info->J2a,a2,s,+,-);
    }


    // set right hand side
    btScalar k = info->fps * info->erp;
    if (m_body1)
    {
        for (int j=0; j<3; j++)
        {
            info->c[j] = k * (a2[j] + body1_trans.getOrigin()[j] -
                              a1[j] - body0_trans.getOrigin()[j]);
        }
    }
    else
    {
        for (int j=0; j<3; j++)
        {
            info->c[j] = k * (p2pconstraint->getPivotInB()[j] - a1[j] -
                              body0_trans.getOrigin()[j]);
        }
    }
}


///////////////////limit motor support

/*! \pre testLimitValue must be called on limot*/
int bt_get_limit_motor_info2(
	btRotationalLimitMotor * limot,
	btRigidBody * body0, btRigidBody * body1,
	btOdeJoint::Info2 *info, int row, btVector3& ax1, int rotational)
{


    int srow = row * info->rowskip;

    // if the joint is powered, or has joint limits, add in the extra row
    int powered = limot->m_enableMotor;
    int limit = limot->m_currentLimit;

    if (powered || limit)
    {
        btScalar *J1 = rotational ? info->J1a : info->J1l;
        btScalar *J2 = rotational ? info->J2a : info->J2l;

        J1[srow+0] = ax1[0];
        J1[srow+1] = ax1[1];
        J1[srow+2] = ax1[2];
        if (body1)
        {
            J2[srow+0] = -ax1[0];
            J2[srow+1] = -ax1[1];
            J2[srow+2] = -ax1[2];
        }

        // linear limot torque decoupling step:
        //
        // if this is a linear limot (e.g. from a slider), we have to be careful
        // that the linear constraint forces (+/- ax1) applied to the two bodies
        // do not create a torque couple. in other words, the points that the
        // constraint force is applied at must lie along the same ax1 axis.
        // a torque couple will result in powered or limited slider-jointed free
        // bodies from gaining angular momentum.
        // the solution used here is to apply the constraint forces at the point
        // halfway between the body centers. there is no penalty (other than an
        // extra tiny bit of computation) in doing this adjustment. note that we
        // only need to do this if the constraint connects two bodies.

        btVector3 ltd;	// Linear Torque Decoupling vector (a torque)
        if (!rotational && body1)
        {
            btVector3 c;
            c[0]=btScalar(0.5)*(body1->getCenterOfMassPosition()[0]
            				-body0->getCenterOfMassPosition()[0]);
            c[1]=btScalar(0.5)*(body1->getCenterOfMassPosition()[1]
            				-body0->getCenterOfMassPosition()[1]);
            c[2]=btScalar(0.5)*(body1->getCenterOfMassPosition()[2]
            				-body0->getCenterOfMassPosition()[2]);

			ltd = c.cross(ax1);

            info->J1a[srow+0] = ltd[0];
            info->J1a[srow+1] = ltd[1];
            info->J1a[srow+2] = ltd[2];
            info->J2a[srow+0] = ltd[0];
            info->J2a[srow+1] = ltd[1];
            info->J2a[srow+2] = ltd[2];
        }

        // if we're limited low and high simultaneously, the joint motor is
        // ineffective

        if (limit && (limot->m_loLimit == limot->m_hiLimit)) powered = 0;

        if (powered)
        {
            info->cfm[row] = 0.0f;//limot->m_normalCFM;
            if (! limit)
            {
                info->c[row] = limot->m_targetVelocity;
                info->lo[row] = -limot->m_maxMotorForce;
                info->hi[row] = limot->m_maxMotorForce;
            }
        }

        if (limit)
        {
            btScalar k = info->fps * limot->m_ERP;
            info->c[row] = -k * limot->m_currentLimitError;
            info->cfm[row] = 0.0f;//limot->m_stopCFM;

            if (limot->m_loLimit == limot->m_hiLimit)
            {
                // limited low and high simultaneously
                info->lo[row] = -dInfinity;
                info->hi[row] = dInfinity;
            }
            else
            {
                if (limit == 1)
                {
                    // low limit
                    info->lo[row] = 0;
                    info->hi[row] = SIMD_INFINITY;
                }
                else
                {
                    // high limit
                    info->lo[row] = -SIMD_INFINITY;
                    info->hi[row] = 0;
                }

                // deal with bounce
                if (limot->m_bounce > 0)
                {
                    // calculate joint velocity
                    btScalar vel;
                    if (rotational)
                    {
                        vel = body0->getAngularVelocity().dot(ax1);
                        if (body1)
                            vel -= body1->getAngularVelocity().dot(ax1);
                    }
                    else
                    {
                        vel = body0->getLinearVelocity().dot(ax1);
                        if (body1)
                            vel -= body1->getLinearVelocity().dot(ax1);
                    }

                    // only apply bounce if the velocity is incoming, and if the
                    // resulting c[] exceeds what we already have.
                    if (limit == 1)
                    {
                        // low limit
                        if (vel < 0)
                        {
                            btScalar newc = -limot->m_bounce* vel;
                            if (newc > info->c[row]) info->c[row] = newc;
                        }
                    }
                    else
                    {
                        // high limit - all those computations are reversed
                        if (vel > 0)
                        {
                            btScalar newc = -limot->m_bounce * vel;
                            if (newc < info->c[row]) info->c[row] = newc;
                        }
                    }
                }
            }
        }
        return 1;
    }
    else return 0;
}


///////////////////OdeD6Joint





OdeD6Joint::OdeD6Joint(
    btTypedConstraint * constraint,
    int index,bool swap,btOdeSolverBody* body0,btOdeSolverBody* body1):
        btOdeTypedJoint(constraint,index,swap,body0,body1)
{
}


void OdeD6Joint::GetInfo1(Info1 *info)
{
	btGeneric6DofConstraint * d6constraint = this->getD6Constraint();
	//prepare constraint
	d6constraint->calculateTransforms();
    info->m = 3;
    info->nub = 3;

    //test angular limits
    for (int i=0;i<3 ;i++ )
    {
    	//if(i==2) continue;
		if(d6constraint->testAngularLimitMotor(i))
		{
			info->m++;
		}
    }


}


int OdeD6Joint::setLinearLimits(Info2 *info)
{

    btGeneric6DofConstraint * d6constraint = this->getD6Constraint();

    //retrieve matrices
    btTransform body0_trans;
    if (m_body0)
    {
        body0_trans = m_body0->m_originalBody->getCenterOfMassTransform();
    }

    btTransform body1_trans;

    if (m_body1)
    {
        body1_trans = m_body1->m_originalBody->getCenterOfMassTransform();
    }

    // anchor points in global coordinates with respect to body PORs.

    int s = info->rowskip;

    // set jacobian
    info->J1l[0] = 1;
    info->J1l[s+1] = 1;
    info->J1l[2*s+2] = 1;


    btVector3 a1,a2;

    a1 = body0_trans.getBasis()*d6constraint->getFrameOffsetA().getOrigin();
    //dMULTIPLY0_331 (a1, body0_mat,m_constraint->m_pivotInA);
    dCROSSMAT (info->J1a,a1,s,-,+);
    if (m_body1)
    {
        info->J2l[0] = -1;
        info->J2l[s+1] = -1;
        info->J2l[2*s+2] = -1;
        a2 = body1_trans.getBasis()*d6constraint->getFrameOffsetB().getOrigin();

        //dMULTIPLY0_331 (a2,body1_mat,m_constraint->m_pivotInB);
        dCROSSMAT (info->J2a,a2,s,+,-);
    }


    // set right hand side
    btScalar k = info->fps * info->erp;
    if (m_body1)
    {
        for (int j=0; j<3; j++)
        {
            info->c[j] = k * (a2[j] + body1_trans.getOrigin()[j] -
                              a1[j] - body0_trans.getOrigin()[j]);
        }
    }
    else
    {
        for (int j=0; j<3; j++)
        {
            info->c[j] = k * (d6constraint->getCalculatedTransformB().getOrigin()[j] - a1[j] -
                              body0_trans.getOrigin()[j]);
        }
    }

    return 3;

}

int OdeD6Joint::setAngularLimits(Info2 *info, int row_offset)
{
	btGeneric6DofConstraint * d6constraint = this->getD6Constraint();
	int row = row_offset;
	//solve angular limits
    for (int i=0;i<3 ;i++ )
    {
    	//if(i==2) continue;
		if(d6constraint->getRotationalLimitMotor(i)->needApplyTorques())
		{
			btVector3 axis = d6constraint->getAxis(i);
			row += bt_get_limit_motor_info2(
				d6constraint->getRotationalLimitMotor(i),
				m_body0->m_originalBody,
				m_body1 ? m_body1->m_originalBody : NULL,
				info,row,axis,1);
		}
    }

    return row;
}

void OdeD6Joint::GetInfo2(Info2 *info)
{
    int row = setLinearLimits(info);
    setAngularLimits(info, row);
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
/*
OdeSliderJoint
Ported from ODE by Roman Ponomarev (rponom@gmail.com)
April 24, 2008
*/

OdeSliderJoint::OdeSliderJoint(
    btTypedConstraint * constraint,
    int index,bool swap, btOdeSolverBody* body0, btOdeSolverBody* body1):
        btOdeTypedJoint(constraint,index,swap,body0,body1)
{
} // OdeSliderJoint::OdeSliderJoint()

//----------------------------------------------------------------------------------

void OdeSliderJoint::GetInfo1(Info1* info)
{
	info->nub = 4; 
	info->m = 4; // Fixed 2 linear + 2 angular
	btSliderConstraint * slider = this->getSliderConstraint();
	//prepare constraint
	slider->calculateTransforms();
	slider->testLinLimits();
	if(slider->getSolveLinLimit() || slider->getPoweredLinMotor())
	{
		info->m++; // limit 3rd linear as well
	}
	slider->testAngLimits();
	if(slider->getSolveAngLimit() || slider->getPoweredAngMotor())
	{
		info->m++; // limit 3rd angular as well
	}
} // OdeSliderJoint::GetInfo1()

//----------------------------------------------------------------------------------

void OdeSliderJoint::GetInfo2(Info2 *info)
{
	int i, s = info->rowskip;
	btSliderConstraint * slider = this->getSliderConstraint();
	const btTransform& trA = slider->getCalculatedTransformA();
	const btTransform& trB = slider->getCalculatedTransformB();
	// make rotations around Y and Z equal
	// the slider axis should be the only unconstrained
	// rotational axis, the angular velocity of the two bodies perpendicular to
	// the slider axis should be equal. thus the constraint equations are
	//    p*w1 - p*w2 = 0
	//    q*w1 - q*w2 = 0
	// where p and q are unit vectors normal to the slider axis, and w1 and w2
	// are the angular velocity vectors of the two bodies.
	// get slider axis (X)
	btVector3 ax1 = trA.getBasis().getColumn(0);
	// get 2 orthos to slider axis (Y, Z)
	btVector3 p = trA.getBasis().getColumn(1);
	btVector3 q = trA.getBasis().getColumn(2);
	// set the two slider rows 
	info->J1a[0] = p[0];
	info->J1a[1] = p[1];
	info->J1a[2] = p[2];
	info->J1a[s+0] = q[0];
	info->J1a[s+1] = q[1];
	info->J1a[s+2] = q[2];
	if(m_body1) 
	{
		info->J2a[0] = -p[0];
		info->J2a[1] = -p[1];
		info->J2a[2] = -p[2];
		info->J2a[s+0] = -q[0];
		info->J2a[s+1] = -q[1];
		info->J2a[s+2] = -q[2];
	}
	// compute the right hand side of the constraint equation. set relative
	// body velocities along p and q to bring the slider back into alignment.
	// if ax1,ax2 are the unit length slider axes as computed from body1 and
	// body2, we need to rotate both bodies along the axis u = (ax1 x ax2).
	// if "theta" is the angle between ax1 and ax2, we need an angular velocity
	// along u to cover angle erp*theta in one step :
	//   |angular_velocity| = angle/time = erp*theta / stepsize
	//                      = (erp*fps) * theta
	//    angular_velocity  = |angular_velocity| * (ax1 x ax2) / |ax1 x ax2|
	//                      = (erp*fps) * theta * (ax1 x ax2) / sin(theta)
	// ...as ax1 and ax2 are unit length. if theta is smallish,
	// theta ~= sin(theta), so
	//    angular_velocity  = (erp*fps) * (ax1 x ax2)
	// ax1 x ax2 is in the plane space of ax1, so we project the angular
	// velocity to p and q to find the right hand side.
	btScalar k = info->fps * info->erp * slider->getSoftnessOrthoAng();
    btVector3 ax2 = trB.getBasis().getColumn(0);
	btVector3 u;
	if(m_body1)
	{
		u = ax1.cross(ax2);
	}
	else
	{
		u = ax2.cross(ax1);
	}
	info->c[0] = k * u.dot(p);
	info->c[1] = k * u.dot(q);
	// pull out pos and R for both bodies. also get the connection
	// vector c = pos2-pos1.
	// next two rows. we want: vel2 = vel1 + w1 x c ... but this would
	// result in three equations, so we project along the planespace vectors
	// so that sliding along the slider axis is disregarded. for symmetry we
	// also substitute (w1+w2)/2 for w1, as w1 is supposed to equal w2.
	btTransform bodyA_trans = m_body0->m_originalBody->getCenterOfMassTransform();
	btTransform bodyB_trans;
	if(m_body1)
	{
		bodyB_trans = m_body1->m_originalBody->getCenterOfMassTransform();
	}
	int s2 = 2 * s, s3 = 3 * s;
	btVector3 c;
	if(m_body1)
	{
		c = bodyB_trans.getOrigin() - bodyA_trans.getOrigin();
		btVector3 tmp = btScalar(0.5) * c.cross(p);

		for (i=0; i<3; i++) info->J1a[s2+i] = tmp[i];
		for (i=0; i<3; i++) info->J2a[s2+i] = tmp[i];

		tmp = btScalar(0.5) * c.cross(q);

		for (i=0; i<3; i++) info->J1a[s3+i] = tmp[i];
		for (i=0; i<3; i++) info->J2a[s3+i] = tmp[i];

		for (i=0; i<3; i++) info->J2l[s2+i] = -p[i];
		for (i=0; i<3; i++) info->J2l[s3+i] = -q[i];
	}
	for (i=0; i<3; i++) info->J1l[s2+i] = p[i];
	for (i=0; i<3; i++) info->J1l[s3+i] = q[i];
	// compute two elements of right hand side. we want to align the offset
	// point (in body 2's frame) with the center of body 1.
	btVector3 ofs; // offset point in global coordinates
	if(m_body1)
	{
		ofs = trB.getOrigin() - trA.getOrigin();
	}
	else
	{
		ofs = trA.getOrigin() - trB.getOrigin();
	}
	k = info->fps * info->erp * slider->getSoftnessOrthoLin();
	info->c[2] = k * p.dot(ofs);
	info->c[3] = k * q.dot(ofs);
	int nrow = 3; // last filled row
	int srow;
	// check linear limits linear
	btScalar limit_err = btScalar(0.0);
	int limit = 0;
	if(slider->getSolveLinLimit())
	{
		limit_err = slider->getLinDepth();
		if(m_body1) 
		{
			limit = (limit_err > btScalar(0.0)) ? 1 : 2;
		}
		else
		{
			limit = (limit_err > btScalar(0.0)) ? 2 : 1;
		}
	}
	int powered = 0;
	if(slider->getPoweredLinMotor())
	{
		powered = 1;
	}
	// if the slider has joint limits, add in the extra row
	if (limit || powered) 
	{
		nrow++;
		srow = nrow * info->rowskip;
		info->J1l[srow+0] = ax1[0];
		info->J1l[srow+1] = ax1[1];
		info->J1l[srow+2] = ax1[2];
		if(m_body1)
		{
			info->J2l[srow+0] = -ax1[0];
			info->J2l[srow+1] = -ax1[1];
			info->J2l[srow+2] = -ax1[2];
		}
		// linear torque decoupling step:
		//
		// we have to be careful that the linear constraint forces (+/- ax1) applied to the two bodies
		// do not create a torque couple. in other words, the points that the
		// constraint force is applied at must lie along the same ax1 axis.
		// a torque couple will result in limited slider-jointed free
		// bodies from gaining angular momentum.
		// the solution used here is to apply the constraint forces at the point
		// halfway between the body centers. there is no penalty (other than an
		// extra tiny bit of computation) in doing this adjustment. note that we
		// only need to do this if the constraint connects two bodies.
	    if (m_body1) 
		{
			dVector3 ltd;	// Linear Torque Decoupling vector (a torque)
			c = btScalar(0.5) * c;
			dCROSS (ltd,=,c,ax1);
			info->J1a[srow+0] = ltd[0];
			info->J1a[srow+1] = ltd[1];
			info->J1a[srow+2] = ltd[2];
			info->J2a[srow+0] = ltd[0];
			info->J2a[srow+1] = ltd[1];
			info->J2a[srow+2] = ltd[2];
		}
		// right-hand part
		btScalar lostop = slider->getLowerLinLimit();
		btScalar histop = slider->getUpperLinLimit();
		if(limit && (lostop == histop))
		{  // the joint motor is ineffective
			powered = 0;
		}
		if(powered)
		{
            info->cfm[nrow] = btScalar(0.0); 
            if(!limit)
            {
				info->c[nrow] = slider->getTargetLinMotorVelocity();
				info->lo[nrow] = -slider->getMaxLinMotorForce() * info->fps;
				info->hi[nrow] = slider->getMaxLinMotorForce() * info->fps;
            }
		}
		if(limit)
		{
			k = info->fps * info->erp;
			if(m_body1) 
			{
				info->c[nrow] = k * limit_err;
			}
			else
			{
				info->c[nrow] = - k * limit_err;
			}
			info->cfm[nrow] = btScalar(0.0); // stop_cfm;
			if(lostop == histop) 
			{
				// limited low and high simultaneously
				info->lo[nrow] = -SIMD_INFINITY;
				info->hi[nrow] = SIMD_INFINITY;
			}
			else 
			{
				if(limit == 1) 
				{
					// low limit
					info->lo[nrow] = 0;
					info->hi[nrow] = SIMD_INFINITY;
				}
				else 
				{
					// high limit
					info->lo[nrow] = -SIMD_INFINITY;
					info->hi[nrow] = 0;
				}
			}
			// bounce (we'll use slider parameter abs(1.0 - m_dampingLimLin) for that)
			btScalar bounce = btFabs(btScalar(1.0) - slider->getDampingLimLin());
			if(bounce > btScalar(0.0))
			{
				btScalar vel = m_body0->m_originalBody->getLinearVelocity().dot(ax1);
				if(m_body1)
				{
					vel -= m_body1->m_originalBody->getLinearVelocity().dot(ax1);
				}
				// only apply bounce if the velocity is incoming, and if the
				// resulting c[] exceeds what we already have.
				if(limit == 1)
				{
					// low limit
					if(vel < 0)
					{
						btScalar newc = -bounce * vel;
						if (newc > info->c[nrow]) info->c[nrow] = newc;
					}
				}
				else
				{
					// high limit - all those computations are reversed
					if(vel > 0)
					{
						btScalar newc = -bounce * vel;
						if(newc < info->c[nrow]) info->c[nrow] = newc;
					}
				}
			}
			info->c[nrow] *= slider->getSoftnessLimLin();
		} // if(limit)
	} // if linear limit
	// check angular limits
	limit_err = btScalar(0.0);
	limit = 0;
	if(slider->getSolveAngLimit())
	{
		limit_err = slider->getAngDepth();
		if(m_body1) 
		{
			limit = (limit_err > btScalar(0.0)) ? 1 : 2;
		}
		else
		{
			limit = (limit_err > btScalar(0.0)) ? 2 : 1;
		}
	}
	// if the slider has joint limits, add in the extra row
	powered = 0;
	if(slider->getPoweredAngMotor())
	{
		powered = 1;
	}
	if(limit || powered) 
	{
		nrow++;
		srow = nrow * info->rowskip;
		info->J1a[srow+0] = ax1[0];
		info->J1a[srow+1] = ax1[1];
		info->J1a[srow+2] = ax1[2];
		if(m_body1)
		{
			info->J2a[srow+0] = -ax1[0];
			info->J2a[srow+1] = -ax1[1];
			info->J2a[srow+2] = -ax1[2];
		}
		btScalar lostop = slider->getLowerAngLimit();
		btScalar histop = slider->getUpperAngLimit();
		if(limit && (lostop == histop))
		{  // the joint motor is ineffective
			powered = 0;
		}
		if(powered)
		{
            info->cfm[nrow] = btScalar(0.0); 
            if(!limit)
            {
				info->c[nrow] = slider->getTargetAngMotorVelocity();
				info->lo[nrow] = -slider->getMaxAngMotorForce() * info->fps;
				info->hi[nrow] = slider->getMaxAngMotorForce() * info->fps;
            }
		}
		if(limit)
		{
			k = info->fps * info->erp;
			if (m_body1) 
			{
				info->c[nrow] = k * limit_err;
			}
			else
			{
				info->c[nrow] = -k * limit_err;
			}
			info->cfm[nrow] = btScalar(0.0); // stop_cfm;
			if(lostop == histop) 
			{
				// limited low and high simultaneously
				info->lo[nrow] = -SIMD_INFINITY;
				info->hi[nrow] = SIMD_INFINITY;
			}
			else 
			{
				if (limit == 1) 
				{
					// low limit
					info->lo[nrow] = 0;
					info->hi[nrow] = SIMD_INFINITY;
				}
				else 
				{
					// high limit
					info->lo[nrow] = -SIMD_INFINITY;
					info->hi[nrow] = 0;
				}
			}
			// bounce (we'll use slider parameter abs(1.0 - m_dampingLimAng) for that)
			btScalar bounce = btFabs(btScalar(1.0) - slider->getDampingLimAng());
			if(bounce > btScalar(0.0))
			{
				btScalar vel = m_body0->m_originalBody->getAngularVelocity().dot(ax1);
				if(m_body1)
				{
					vel -= m_body1->m_originalBody->getAngularVelocity().dot(ax1);
				}
				// only apply bounce if the velocity is incoming, and if the
				// resulting c[] exceeds what we already have.
				if(limit == 1)
				{
					// low limit
					if(vel < 0)
					{
						btScalar newc = -bounce * vel;
						if (newc > info->c[nrow]) info->c[nrow] = newc;
					}
				}
				else
				{
					// high limit - all those computations are reversed
					if(vel > 0)
					{
						btScalar newc = -bounce * vel;
						if(newc < info->c[nrow]) info->c[nrow] = newc;
					}
				}
			}
			info->c[nrow] *= slider->getSoftnessLimAng();
		} // if(limit)
	} // if angular limit or powered
} // OdeSliderJoint::GetInfo2()

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------




