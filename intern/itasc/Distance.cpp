/* $Id$
 * Distance.cpp
 *
 *  Created on: Jan 30, 2009
 *      Author: rsmits
 */

#include "Distance.hpp"
#include "kdl/kinfam_io.hpp"
#include <math.h>
#include <string.h>

namespace iTaSC
{
// a distance constraint is characterized by 5 values: alpha, tolerance, K, yd, yddot
static const unsigned int distanceCacheSize = sizeof(double)*5 + sizeof(e_scalar)*6;

Distance::Distance(double armlength, double accuracy, unsigned int maximum_iterations):
    ConstraintSet(1,accuracy,maximum_iterations),
    m_chiKdl(6),m_jac(6),m_cache(NULL),
	m_distCCh(-1),m_distCTs(0)
{
    m_chain.addSegment(Segment(Joint(Joint::RotZ)));
    m_chain.addSegment(Segment(Joint(Joint::RotX)));
    m_chain.addSegment(Segment(Joint(Joint::TransY)));
    m_chain.addSegment(Segment(Joint(Joint::RotZ)));
    m_chain.addSegment(Segment(Joint(Joint::RotY)));
    m_chain.addSegment(Segment(Joint(Joint::RotX)));

	m_fksolver = new KDL::ChainFkSolverPos_recursive(m_chain);
	m_jacsolver = new KDL::ChainJntToJacSolver(m_chain);
    m_Cf(0,2)=1.0;
	m_alpha = 1.0;
	m_tolerance = 0.05;
	m_maxerror = armlength/2.0;
	m_K = 20.0;
	m_Wy(0) = m_alpha/*/(m_tolerance*m_K)*/;
	m_yddot = m_nextyddot = 0.0;
	m_yd = m_nextyd = KDL::epsilon;
	memset(&m_data, 0, sizeof(m_data));
	// initialize the data with normally fixed values
	m_data.id = ID_DISTANCE;
	m_values.id = ID_DISTANCE;
	m_values.number = 1;
	m_values.alpha = m_alpha;
	m_values.feedback = m_K;
	m_values.tolerance = m_tolerance;
	m_values.values = &m_data;
}

Distance::~Distance()
{
    delete m_fksolver;
    delete m_jacsolver;
}

bool Distance::computeChi(Frame& pose)
{
	double dist, alpha, beta, gamma;
	dist = pose.p.Norm();
	Rotation basis;
	if (dist < KDL::epsilon) {
		// distance is almost 0, no need for initial rotation
		m_chi(0) = 0.0;
		m_chi(1) = 0.0;
	} else {
		// find the XZ angles that bring the Y axis to point to init_pose.p
		Vector axis(pose.p/dist);
		beta = 0.0;
		if (fabs(axis(2)) > 1-KDL::epsilon) {
			// direction is aligned on Z axis, just rotation on X
			alpha = 0.0;
			gamma = KDL::sign(axis(2))*KDL::PI/2;
		} else {
			alpha = -KDL::atan2(axis(0), axis(1));
			gamma = KDL::atan2(axis(2), KDL::sqrt(KDL::sqr(axis(0))+KDL::sqr(axis(1))));
		}
		// rotation after first 2 joints
		basis = Rotation::EulerZYX(alpha, beta, gamma);
		m_chi(0) = alpha;
		m_chi(1) = gamma;
	}
	m_chi(2) = dist;
	basis = basis.Inverse()*pose.M;
	basis.GetEulerZYX(alpha, beta, gamma);
	// alpha = rotation on Z
	// beta = rotation on Y
	// gamma = rotation on X in that order
	// it corresponds to the joint order, so just assign
	m_chi(3) = alpha;
	m_chi(4) = beta;
	m_chi(5) = gamma;
	return true;
}

bool Distance::initialise(Frame& init_pose)
{
	// we will initialize m_chi to values that match the pose
    m_externalPose=init_pose;
	computeChi(m_externalPose);
	// get current Jf and update internal pose
    updateJacobian();
	return true;
}

bool Distance::closeLoop()
{
	if (!Equal(m_internalPose.Inverse()*m_externalPose,F_identity,m_threshold)){
		computeChi(m_externalPose);
		updateJacobian();
	}
	return true;
}

void Distance::initCache(Cache *_cache)
{
	m_cache = _cache;
	m_distCCh = -1;
	if (m_cache) {
		// create one channel for the coordinates
		m_distCCh = m_cache->addChannel(this, "Xf", distanceCacheSize);
		// save initial constraint in cache position 0
		pushDist(0);
	}
}

void Distance::pushDist(CacheTS timestamp)
{
	if (m_distCCh >= 0) {
		double *item = (double*)m_cache->addCacheItem(this, m_distCCh, timestamp, NULL, distanceCacheSize);
		if (item) {
			*item++ = m_K;
			*item++ = m_tolerance;
			*item++ = m_yd;
			*item++ = m_yddot;
			*item++ = m_alpha;
			memcpy(item, &m_chi[0], 6*sizeof(e_scalar));
		}
		m_distCTs = timestamp;
	}
}

bool Distance::popDist(CacheTS timestamp)
{
	if (m_distCCh >= 0) {
		double *item = (double*)m_cache->getPreviousCacheItem(this, m_distCCh, &timestamp);
		if (item && timestamp != m_distCTs) {
			m_values.feedback = m_K = *item++;
			m_values.tolerance = m_tolerance = *item++;
			m_yd = *item++;
			m_yddot = *item++;
			m_values.alpha = m_alpha = *item++;
			memcpy(&m_chi[0], item, 6*sizeof(e_scalar));
			m_distCTs = timestamp;
			m_Wy(0) = m_alpha/*/(m_tolerance*m_K)*/;
			updateJacobian();
		}
		return (item) ? true : false;
	}
	return true;
}

void Distance::pushCache(const Timestamp& timestamp)
{
	if (!timestamp.substep && timestamp.cache)
		pushDist(timestamp.cacheTimestamp);
}

void Distance::updateKinematics(const Timestamp& timestamp)
{
	if (timestamp.interpolate) {
		//the internal pose and Jf is already up to date (see model_update)
		//update the desired output based on yddot
		if (timestamp.substep) {
			m_yd += m_yddot*timestamp.realTimestep;
			if (m_yd < KDL::epsilon)
				m_yd = KDL::epsilon;
		} else {
			m_yd = m_nextyd;
			m_yddot = m_nextyddot;
		}
	}
	pushCache(timestamp);
}

void Distance::updateJacobian()
{
    for(unsigned int i=0;i<6;i++)
        m_chiKdl(i)=m_chi(i);

    m_fksolver->JntToCart(m_chiKdl,m_internalPose);
    m_jacsolver->JntToJac(m_chiKdl,m_jac);
    changeRefPoint(m_jac,-m_internalPose.p,m_jac);
    for(unsigned int i=0;i<6;i++)
        for(unsigned int j=0;j<6;j++)
            m_Jf(i,j)=m_jac(i,j);
}

bool Distance::setControlParameters(struct ConstraintValues* _values, unsigned int _nvalues, double timestep)
{
	int action = 0;
	int i;
	ConstraintSingleValue* _data;

	while (_nvalues > 0) {
		if (_values->id == ID_DISTANCE) {
			if ((_values->action & ACT_ALPHA) && _values->alpha >= 0.0) {
				m_alpha = _values->alpha;
				action |= ACT_ALPHA;
			}
			if ((_values->action & ACT_TOLERANCE) && _values->tolerance > KDL::epsilon) {
				m_tolerance = _values->tolerance;
				action |= ACT_TOLERANCE;
			}
			if ((_values->action & ACT_FEEDBACK) && _values->feedback > KDL::epsilon) {
				m_K = _values->feedback;
				action |= ACT_FEEDBACK;
			}
			for (_data = _values->values, i=0; i<_values->number; i++, _data++) {
				if (_data->id == ID_DISTANCE) {
					switch (_data->action & (ACT_VALUE|ACT_VELOCITY)) {
					case 0:
						// no indication, keep current values
						break;
					case ACT_VELOCITY:
						// only the velocity is given estimate the new value by integration
						_data->yd = m_yd+_data->yddot*timestep;
						// walkthrough for negative value correction
					case ACT_VALUE:
						// only the value is given, estimate the velocity from previous value
						if (_data->yd < KDL::epsilon)
							_data->yd = KDL::epsilon;
						m_nextyd = _data->yd;
						// if the user sets the value, we assume future velocity is zero
						// (until the user changes the value again)
						m_nextyddot = (_data->action & ACT_VALUE) ? 0.0 : _data->yddot;
						if (timestep>0.0) {
							m_yddot = (_data->yd-m_yd)/timestep;
						} else {
							// allow the user to change target instantenously when this function
							// if called from setControlParameter with timestep = 0
							m_yddot = m_nextyddot;
							m_yd = m_nextyd;
						}
						break;
					case (ACT_VALUE|ACT_VELOCITY):
						// the user should not set the value and velocity at the same time.
						// In this case, we will assume that he want to set the future value
						// and we compute the current value to match the velocity
						if (_data->yd < KDL::epsilon)
							_data->yd = KDL::epsilon;
						m_yd = _data->yd - _data->yddot*timestep;
						if (m_yd < KDL::epsilon)
							m_yd = KDL::epsilon;
						m_nextyd = _data->yd;
						m_nextyddot = _data->yddot;
						if (timestep>0.0) {
							m_yddot = (_data->yd-m_yd)/timestep;
						} else {
							m_yd = m_nextyd;
							m_yddot = m_nextyddot;
						}
						break;
					}
				}
			}
		}
		_nvalues--;
		_values++;
	}
	if (action & (ACT_TOLERANCE|ACT_FEEDBACK|ACT_ALPHA)) {
		// recompute the weight
		m_Wy(0) = m_alpha/*/(m_tolerance*m_K)*/;
	}
	return true;
}

const ConstraintValues* Distance::getControlParameters(unsigned int* _nvalues)
{
	*(double*)&m_data.y = m_chi(2);
	*(double*)&m_data.ydot = m_ydot(0);
	m_data.yd = m_yd;
	m_data.yddot = m_yddot;
	m_data.action = 0;
	m_values.action = 0;
	if (_nvalues) 
		*_nvalues=1; 
	return &m_values; 
}

void Distance::updateControlOutput(const Timestamp& timestamp)
{
	bool cacheAvail = true;
	if (!timestamp.substep) {
		if (!timestamp.reiterate)
			cacheAvail = popDist(timestamp.cacheTimestamp);
	}
	if (m_constraintCallback && (m_substep || (!timestamp.reiterate && !timestamp.substep))) {
		// initialize first callback the application to get the current values
		*(double*)&m_data.y = m_chi(2);
		*(double*)&m_data.ydot = m_ydot(0);
		m_data.yd = m_yd;
		m_data.yddot = m_yddot;
		m_data.action = 0;
		m_values.action = 0;
		if ((*m_constraintCallback)(timestamp, &m_values, 1, m_constraintParam)) {
			setControlParameters(&m_values, 1, timestamp.realTimestep);
		}
	}
	if (!cacheAvail || !timestamp.interpolate) {
		// first position in cache: set the desired output immediately as we cannot interpolate
		m_yd = m_nextyd;
		m_yddot = m_nextyddot;
	}
	double error = m_yd-m_chi(2);
	if (KDL::Norm(error) > m_maxerror)
		error = KDL::sign(error)*m_maxerror;
    m_ydot(0)=m_yddot+m_K*error;
}

}
