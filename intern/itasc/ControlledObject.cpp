/* $Id$
 * ControlledObject.cpp
 *
 *  Created on: Jan 5, 2009
 *      Author: rubensmits
 */

#include "ControlledObject.hpp"


namespace iTaSC {
ControlledObject::ControlledObject():
    Object(Controlled),m_nq(0),m_nc(0),m_nee(0)
{
	// max joint variable = 0.52 radian or 0.52 meter in one timestep
	m_maxDeltaQ = e_scalar(0.52);
}

void ControlledObject::initialize(unsigned int _nq,unsigned int _nc, unsigned int _nee)
{
	assert(_nee >= 1);
	m_nq = _nq;
	m_nc = _nc;
	m_nee = _nee;
	if (m_nq > 0) {
		m_Wq =  e_identity_matrix(m_nq,m_nq);
		m_qdot = e_zero_vector(m_nq);
	}
	if (m_nc > 0) {
		m_Wy = e_scalar_vector(m_nc,1.0);
		m_ydot = e_zero_vector(m_nc);
	}
	if (m_nc > 0 && m_nq > 0)
		m_Cq = e_zero_matrix(m_nc,m_nq);
	// clear all Jacobian if any
	m_JqArray.clear();
	// reserve one more to have a zero matrix handy
	if (m_nq > 0)
		m_JqArray.resize(m_nee+1, e_zero_matrix(6,m_nq));
}

ControlledObject::~ControlledObject() {}



const e_matrix& ControlledObject::getJq(unsigned int ee) const
{
	assert(m_nq > 0);
	return m_JqArray[(ee>m_nee)?m_nee:ee];
}

double ControlledObject::getMaxTimestep(double& timestep)
{
	e_scalar maxQdot = m_qdot.cwise().abs().maxCoeff();
	if (timestep*maxQdot > m_maxDeltaQ) {
		timestep = m_maxDeltaQ/maxQdot;
	}
	return timestep;
}

}
