/* $Id$
 * Armature.cpp
 *
 *  Created on: Feb 3, 2009
 *      Author: benoitbolsee
 */

#include "Armature.hpp"
#include <algorithm>
#include <malloc.h>
#include <string.h>

namespace iTaSC {

// a joint constraint is characterized by 5 values: tolerance, K, alpha, yd, yddot
static const unsigned int constraintCacheSize = sizeof(double)*5;
std::string Armature::m_root = "root";

Armature::Armature():
	ControlledObject(),
	m_tree(),
	m_njoint(0),
	m_nconstraint(0),
	m_neffector(0),
	m_finalized(false),
	m_cache(NULL),
	m_qCCh(-1),
	m_qCTs(0),
	m_yCCh(-1),
	m_yCTs(0),
	m_qKdl(),
	m_qdotKdl(),
	m_jac(NULL),
	m_jacsolver(NULL),
	m_fksolver(NULL)
{
}

Armature::~Armature()
{
	if (m_jac)
		delete m_jac;
	if (m_jacsolver)
		delete m_jacsolver;
	if (m_fksolver)
		delete m_fksolver;
	for (JointConstraintList::iterator it=m_constraints.begin(); it != m_constraints.end(); it++) {
		if (*it != NULL)
			delete (*it);
	}
	m_constraints.clear();
}

void Armature::initCache(Cache *_cache)
{
	m_cache = _cache;
	m_qCCh = -1;
	m_yCCh = -1;
	if (m_cache) {
		// add a special channel for the joint
		m_qCCh = m_cache->addChannel(this, "q", m_qKdl.rows()*sizeof(double));
		// for the constraints, instead of creating many different channels, we will
		// create a single channel for all the constraints
		if (m_nconstraint) {
			m_yCCh = m_cache->addChannel(this, "y", m_nconstraint*constraintCacheSize);
		}
		// store the initial cache position at timestamp 0
		pushQ(0);
		pushConstraints(0);
	}
}

void Armature::pushQ(CacheTS timestamp)
{
	if (m_qCCh >= 0) {
		m_cache->addCacheItem(this, m_qCCh, timestamp, &m_qKdl(0), m_qKdl.rows()*sizeof(double));
		m_qCTs = timestamp;
	}
}

/* return true if a m_cache position was loaded */
bool Armature::popQ(CacheTS timestamp)
{
	if (m_qCCh >= 0) {
		double* item;
		item = (double*)m_cache->getPreviousCacheItem(this, m_qCCh, &timestamp);
		if (item && m_qCTs != timestamp) {
			double& q = m_qKdl(0);
			memcpy(&q, item, m_qKdl.rows()*sizeof(q));
			m_qCTs = timestamp;
			// changing the joint => recompute the jacobian
			updateJacobian();
		}
		return (item) ? true : false;
	}
	return true;
}

void Armature::pushConstraints(CacheTS timestamp)
{
	if (m_yCCh >= 0) {
		double *item = (double*)m_cache->addCacheItem(this, m_yCCh, timestamp, NULL, m_nconstraint*constraintCacheSize);
		if (item) {
			for (unsigned int i=0; i<m_nconstraint; i++) {
				JointConstraint_struct* pConstraint = m_constraints[i];
				*item++ = pConstraint->values.feedback;
				*item++ = pConstraint->values.tolerance;
				*item++ = pConstraint->value.yd;
				*item++ = pConstraint->value.yddot;
				*item++ = pConstraint->values.alpha;
			}
		}
		m_yCTs = timestamp;
	}
}

/* return true if a cache position was loaded */
bool Armature::popConstraints(CacheTS timestamp)
{
	if (m_yCCh >= 0) {
		double *item = (double*)m_cache->getPreviousCacheItem(this, m_yCCh, &timestamp);
		if (item && m_yCTs != timestamp) {
			for (unsigned int i=0; i<m_nconstraint; i++) {
				JointConstraint_struct* pConstraint = m_constraints[i];
				pConstraint->values.feedback = *item++;
				pConstraint->values.tolerance = *item++;
				pConstraint->value.yd = *item++;
				pConstraint->value.yddot = *item++;
				pConstraint->values.alpha = *item++;
			}
			m_yCTs = timestamp;
		}
		return (item) ? true : false;
	}
	return true;
}

bool Armature::addSegment(const std::string& segment_name, const std::string& hook_name, const Joint& joint, double q_rest, const Frame& f_tip, const Inertia& M)
{
	if (m_finalized)
		return false;

	Segment segment(joint, f_tip, M);
	if (!m_tree.addSegment(segment, segment_name, hook_name))
		return false;

	if (joint.getType() != Joint::None) {
		m_joints.push_back(q_rest);
		m_njoint++;
	}
	return true;
}

bool Armature::getSegment(const std::string& name, const Joint* &p_joint, double &q_rest, double &q, const Frame* &p_tip)
{
	SegmentMap::const_iterator sit = m_tree.getSegment(name);
	if (sit == m_tree.getSegments().end())
		return false;
	p_joint = &sit->second.segment.getJoint();
	p_tip = &sit->second.segment.getFrameToTip();
	if (p_joint->getType() != Joint::None) {
		q_rest = m_joints[sit->second.q_nr];
		q = m_qKdl(sit->second.q_nr);
	}
	return true;
}

double Armature::getMaxJointChange(double timestep)
{
	if (!m_finalized)
		return 0.0;
	double maxJoint = 0.0;
	for (unsigned int i=0; i<m_njoint; i++) {
		double joint = fabs(m_qdot(i)*timestep);
		if (maxJoint < joint)
			maxJoint = joint;
	}
	return maxJoint;
}

int Armature::addConstraint(const std::string& segment_name, ConstraintCallback _function, void* _param, bool _freeParam, bool _substep)
{
	SegmentMap::const_iterator segment_it = m_tree.getSegment(segment_name);
	if (segment_it == m_tree.getSegments().end() || segment_it->second.segment.getJoint().getType() == Joint::None) {
		if (_freeParam && _param)
			free(_param);
		return -1;
	}
	JointConstraintList::iterator constraint_it;
	JointConstraint_struct* pConstraint;
	int iConstraint;
	for (iConstraint=0, constraint_it=m_constraints.begin(); constraint_it != m_constraints.end(); constraint_it++, iConstraint++) {
		pConstraint = *constraint_it;
		if (pConstraint->segment == segment_it) {
			// redefining a constraint
			if (pConstraint->freeParam && pConstraint->param) {
				free(pConstraint->param);
			}
			pConstraint->function = _function;
			pConstraint->param = _param;
			pConstraint->freeParam = _freeParam;
			pConstraint->substep = _substep;
			return iConstraint;
		}
	}
	if (m_finalized)  {
		if (_freeParam && _param)
			free(_param);
		return -1;
	}
	// new constraint, append
	pConstraint = new JointConstraint_struct(segment_it, ID_JOINT, _function, _param, _freeParam, _substep);
	m_constraints.push_back(pConstraint);
	// desired value = rest position
	//(suitable for joint limit constraint, maybe changed by user in callback)
	pConstraint->value.yd  = m_joints[segment_it->second.q_nr];
	return m_nconstraint++;
}

bool Armature::JointLimitCallback(const Timestamp& timestamp, struct ConstraintValues* const _values, unsigned int _nvalues, void* _param)
{
	// called from updateControlOutput() when a limit is set on a joint
	// update the parameters
	LimitConstraintParam_struct* pLimit = (LimitConstraintParam_struct*)_param;
	double y = _values->values->y;
	double x;
	if (y > pLimit->maxThreshold) {
		if (y < pLimit->max) {
			x = (pLimit->max-y)/pLimit->threshold;
			_values->alpha = pLimit->maxWeight*(1.0-x)/(1.0+pLimit->slope*x);
		} else {
			_values->alpha = pLimit->maxWeight;
		}
		// change the limit to the threshold value so that there is no oscillation
		_values->values->yd = pLimit->maxThreshold;
	} else if (y < pLimit->minThreshold) {
		if (y > pLimit->min) {
			x = (y-pLimit->min)/pLimit->threshold;
			_values->alpha = pLimit->maxWeight*(1.0-x)/(1.0+pLimit->slope*x);
		} else {
			_values->alpha = pLimit->maxWeight;
		}
		// change the limit to the threshold value so that there is no oscillation
		_values->values->yd = pLimit->minThreshold;
	} else {
		_values->alpha = 0.0;
	}
	return true;
}

int Armature::addLimitConstraint(const std::string& segment_name, double _min, double _max, double _threshold, double _maxWeight, double _slope)
{
	SegmentMap::const_iterator segment_it = m_tree.getSegment(segment_name);
	if (segment_it == m_tree.getSegments().end() || segment_it->second.segment.getJoint().getType() == Joint::None) {
		return -1;
	}
	if (segment_it->second.segment.getJoint().getType() < Joint::TransX) {
		// for rotation joint, the limit is given in degree, convert to radian
		_min *= KDL::deg2rad;
		_max *= KDL::deg2rad;
		_threshold *= KDL::deg2rad;
	}
	LimitConstraintParam_struct* param = new LimitConstraintParam_struct;
	param->max = _max;
	param->min = _min;
	param->threshold = _threshold;
	param->maxThreshold = _max - _threshold;
	param->minThreshold = _min + _threshold;
	param->maxWeight = _maxWeight;
	param->slope = (_maxWeight-_slope)/_slope;
	return addConstraint(segment_name, JointLimitCallback, param, true, true);
}

int Armature::addEndEffector(const std::string& name)
{
	const SegmentMap& segments = m_tree.getSegments();
	if (segments.find(name) == segments.end())
		return -1;

	EffectorList::const_iterator it;
	int ee;
	for (it=m_effectors.begin(), ee=0; it!=m_effectors.end(); it++, ee++) {
		if (it->name == name)
			return ee;
	}
	if (m_finalized)
		return -1;
	Effector_struct effector(name);
	m_effectors.push_back(effector);
	return m_neffector++;
}

void Armature::finalize()
{
	unsigned int i;
	if (m_finalized)
		return;
	initialize(m_njoint, m_nconstraint, m_neffector);
	for (i=0; i<m_nconstraint; i++) {
		JointConstraint_struct* pConstraint = m_constraints[i];
		m_Cq(i,pConstraint->segment->second.q_nr) = 1.0;
		m_Wy(i) = pConstraint->values.alpha/(pConstraint->values.tolerance*pConstraint->values.feedback);
	}
	m_jacsolver= new KDL::TreeJntToJacSolver(m_tree);
	m_fksolver = new KDL::TreeFkSolverPos_recursive(m_tree);
	m_jac = new Jacobian(m_njoint);
	m_qKdl.resize(m_njoint);
	m_qdotKdl.resize(m_njoint);
	for (i=0; i<m_njoint; i++) {
		m_qKdl(i) = m_joints[i];
	}
	updateJacobian();
	m_finalized = true;
}

void Armature::updateKinematics(const Timestamp& timestamp){

    //TODO: what about the timestep?
    //Integrate m_qdot
	if (!m_finalized)
		return;

    for(unsigned int i=0;i<m_nq;i++)
        m_qdotKdl(i)=m_qdot(i)*timestamp.realTimestep;
    Add(m_qKdl,m_qdotKdl,m_qKdl);

	if (!timestamp.substep) {
		pushQ(timestamp.cacheTimestamp);
		pushConstraints(timestamp.cacheTimestamp);
	}

	updateJacobian();
	// here update the desired output.
	// We assume constant desired output for the joint limit constraint, no need to update it.
}

void Armature::updateJacobian()
{
    //calculate pose and jacobian
	for (unsigned int ee=0; ee<m_nee; ee++) {
		m_fksolver->JntToCart(m_qKdl,m_effectors[ee].pose,m_effectors[ee].name,m_root);
		m_jacsolver->JntToJac(m_qKdl,*m_jac,m_effectors[ee].name);
		// get the jacobian for the base point, to prepare transformation to world reference
		changeRefPoint(*m_jac,-m_effectors[ee].pose.p,*m_jac);
		//copy to Jq:
		e_matrix& Jq = m_JqArray[ee];
		for(unsigned int i=0;i<6;i++) {
			for(unsigned int j=0;j<m_nq;j++)
				Jq(i,j)=(*m_jac)(i,j);
		}
	}
}

const Frame& Armature::getPose(const unsigned int ee)
{
	if (!m_finalized)
		return F_identity;
	return (ee >= m_nee) ? F_identity : m_effectors[ee].pose;
}

bool Armature::getRelativeFrame(Frame& result, const std::string& segment_name, const std::string& base_name)
{
	if (!m_finalized)
		return false;
	return (m_fksolver->JntToCart(m_qKdl,result,segment_name,base_name) < 0) ? false : true;
}

double Armature::getJoint(unsigned int joint)
{
	if (m_finalized && joint < m_njoint)
		return m_qKdl(joint);
	return 0.0;
}

void Armature::updateControlOutput(const Timestamp& timestamp)
{
	if (!m_finalized)
		return;

	if (!timestamp.substep && !timestamp.reiterate) {
		popQ(timestamp.cacheTimestamp);
		popConstraints(timestamp.cacheTimestamp);
	}

	JointConstraintList::iterator it;
	unsigned int iConstraint;

	// scan through the constraints and call the callback functions
	for (iConstraint=0, it=m_constraints.begin(); it!=m_constraints.end(); it++, iConstraint++) {
		JointConstraint_struct* pConstraint = *it;
		*(double*)&pConstraint->value.y = m_qKdl(pConstraint->segment->second.q_nr);
		if (pConstraint->function && (!timestamp.substep || pConstraint->substep)) {
			*(double*)&pConstraint->value.ydot = m_qdotKdl(pConstraint->segment->second.q_nr);
			(*pConstraint->function)(timestamp, &pConstraint->values, 1, pConstraint->param);
		}
		// recompute the weight in any case, that's the most likely modification
		m_Wy(iConstraint) = pConstraint->values.alpha/(pConstraint->values.tolerance*pConstraint->values.feedback);
		m_ydot(iConstraint)=pConstraint->value.yddot+pConstraint->values.feedback*(pConstraint->value.yd-pConstraint->value.y);
	}
}

bool Armature::setControlParameter(unsigned int constraintId, unsigned int valueId, ConstraintAction action, double value, double timestep)
{
	if (constraintId < m_nconstraint) {
		JointConstraint_struct* pConstraint = m_constraints[constraintId];
		if (valueId == ID_JOINT) {
			switch (action) {
			case ACT_VALUE:
				pConstraint->value.yd = value;
				break;
			case ACT_VELOCITY:
				pConstraint->value.yddot = value;
				break;
			case ACT_TOLERANCE:
				pConstraint->values.tolerance = value;
				break;
			case ACT_FEEDBACK:
				pConstraint->values.feedback = value;
				break;
			case ACT_ALPHA:
				pConstraint->values.alpha = value;
				break;
			case ACT_NONE:
			    break;
			}
			if (m_finalized)
				m_Wy(constraintId) = pConstraint->values.alpha/(pConstraint->values.tolerance*pConstraint->values.feedback);
			return true;
		}
	}
	return false;
}

double Armature::getMaxTimestep(double& timestep)
{
	// We will scan through the joint and for each determine the maximum change
	// based on joint limit and velocity
	// First get the timestep based on max velocity
	ControlledObject::getMaxTimestep(timestep);
	// then search through the joint limit
	for (JointConstraintList::const_iterator it=m_constraints.begin(); it != m_constraints.end(); it++) {
		JointConstraint_struct* cs = *it;

		
	}
	return timestep;
}

}

