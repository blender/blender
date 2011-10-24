/** \file itasc/ConstraintSet.cpp
 *  \ingroup itasc
 */
/*
 * ConstraintSet.cpp
 *
 *  Created on: Jan 5, 2009
 *      Author: rubensmits
 */

#include "ConstraintSet.hpp"
#include "kdl/utilities/svd_eigen_HH.hpp"

namespace iTaSC {

ConstraintSet::ConstraintSet(unsigned int _nc,double accuracy,unsigned int maximum_iterations):
    m_nc(_nc),
    m_Cf(e_zero_matrix(m_nc,6)),
    m_Wy(e_scalar_vector(m_nc,1.0)),
    m_y(m_nc),m_ydot(e_zero_vector(m_nc)),m_chi(e_zero_vector(6)),
    m_S(6),m_temp(6),m_tdelta(6),
    m_Jf(e_identity_matrix(6,6)),
    m_U(e_identity_matrix(6,6)),m_V(e_identity_matrix(6,6)),m_B(e_zero_matrix(6,6)),
    m_Jf_inv(e_zero_matrix(6,6)),
	m_internalPose(F_identity), m_externalPose(F_identity),
	m_constraintCallback(NULL), m_constraintParam(NULL), 
	m_toggle(false),m_substep(false),
    m_threshold(accuracy),m_maxIter(maximum_iterations)
{
	m_maxDeltaChi = e_scalar(0.52);
}

ConstraintSet::ConstraintSet():
    m_nc(0), 
	m_internalPose(F_identity), m_externalPose(F_identity),
	m_constraintCallback(NULL), m_constraintParam(NULL), 
	m_toggle(false),m_substep(false),
	m_threshold(0.0),m_maxIter(0)
{
	m_maxDeltaChi = e_scalar(0.52);
}

void ConstraintSet::reset(unsigned int _nc,double accuracy,unsigned int maximum_iterations)
{
    m_nc = _nc;
    m_Jf = e_identity_matrix(6,6);
    m_Cf = e_zero_matrix(m_nc,6);
    m_U = e_identity_matrix(6,6);
	m_V = e_identity_matrix(6,6);
	m_B = e_zero_matrix(6,6);
    m_Jf_inv = e_zero_matrix(6,6),
    m_Wy = e_scalar_vector(m_nc,1.0),
    m_chi = e_zero_vector(6);
    m_chidot = e_zero_vector(6);
	m_y = e_zero_vector(m_nc);
	m_ydot = e_zero_vector(m_nc);
	m_S = e_zero_vector(6);
	m_temp = e_zero_vector(6);
	m_tdelta = e_zero_vector(6);
    m_threshold = accuracy;
	m_maxIter = maximum_iterations;
}

ConstraintSet::~ConstraintSet() {

}

void ConstraintSet::modelUpdate(Frame& _external_pose,const Timestamp& timestamp)
{
    m_chi+=m_chidot*timestamp.realTimestep;
	m_externalPose = _external_pose;

    //update the internal pose and Jf
    updateJacobian();
	//check if loop is already closed, if not update the pose and Jf
    unsigned int iter=0;
    while(iter<5&&!closeLoop())
        iter++;
}

double ConstraintSet::getMaxTimestep(double& timestep)
{
	e_scalar maxChidot = m_chidot.array().abs().maxCoeff();
	if (timestep*maxChidot > m_maxDeltaChi) {
		timestep = m_maxDeltaChi/maxChidot;
	}
	return timestep;
}

bool ConstraintSet::initialise(Frame& init_pose){
    m_externalPose=init_pose;
	// get current Jf
    updateJacobian();

    unsigned int iter=0;
    while(iter<m_maxIter&&!closeLoop()){
        iter++;
    }
    if (iter<m_maxIter)
        return true;
    else
        return false;
}

bool ConstraintSet::setControlParameter(int id, ConstraintAction action, double data, double timestep)
{
	ConstraintValues values;
	ConstraintSingleValue value;
	values.values = &value;
	values.number = 0;
	values.action = action;
	values.id = id;
	value.action = action;
	value.id = id;
	switch (action) {
	case ACT_NONE:
		return true;
	case ACT_VALUE:
		value.yd = data;
		values.number = 1;
		break;
	case ACT_VELOCITY:
		value.yddot = data;
		values.number = 1;
		break;
	case ACT_TOLERANCE:
		values.tolerance = data;
		break;
	case ACT_FEEDBACK:
		values.feedback = data;
		break;
	case ACT_ALPHA:
		values.alpha = data;
		break;
	default:
		assert(action==ACT_NONE);
		break;
	}
	return setControlParameters(&values, 1, timestep);
}

bool ConstraintSet::closeLoop(){
    //Invert Jf
    //TODO: svd_boost_Macie has problems if Jf contains zero-rows
    //toggle=!toggle;
    //svd_boost_Macie(Jf,U,S,V,B,temp,1e-3*threshold,toggle);
	int ret = KDL::svd_eigen_HH(m_Jf,m_U,m_S,m_V,m_temp);
    if(ret<0)
        return false;

	// the reference point and frame of the jacobian is the base frame
	// m_externalPose-m_internalPose is the twist to extend the end effector
	// to get the required pose => change the reference point to the base frame
	Twist twist_delta(diff(m_internalPose,m_externalPose));
	twist_delta=twist_delta.RefPoint(-m_internalPose.p);
    for(unsigned int i=0;i<6;i++)
        m_tdelta(i)=twist_delta(i);
    //TODO: use damping in constraintset inversion?
    for(unsigned int i=0;i<6;i++)
        if(m_S(i)<m_threshold){
			m_B.row(i).setConstant(0.0);
        }else
            m_B.row(i) = m_U.col(i)/m_S(i);

    m_Jf_inv.noalias()=m_V*m_B;

    m_chi.noalias()+=m_Jf_inv*m_tdelta;
    updateJacobian();
	// m_externalPose-m_internalPose in end effector frame
	// this is just to compare the pose, a different formula would work too
	return Equal(m_internalPose.Inverse()*m_externalPose,F_identity,m_threshold);

}
}
