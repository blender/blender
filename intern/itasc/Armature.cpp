/** \file itasc/Armature.cpp
 *  \ingroup itasc
 */
/*
 * Armature.cpp
 *
 *  Created on: Feb 3, 2009
 *      Author: benoitbolsee
 */

#include "Armature.hpp"
#include <algorithm>
#include <string.h>
#include <stdlib.h>

namespace iTaSC {

// a joint constraint is characterized by 5 values: tolerance, K, alpha, yd, yddot
static const unsigned int constraintCacheSize = 5;
std::string Armature::m_root = "root";

Armature::Armature():
	ControlledObject(),
	m_tree(),
	m_njoint(0),
	m_nconstraint(0),
	m_noutput(0),
	m_neffector(0),
	m_finalized(false),
	m_cache(NULL),
	m_buf(NULL),
	m_qCCh(-1),
	m_qCTs(0),
	m_yCCh(-1),
	m_yCTs(0),
	m_qKdl(),
	m_oldqKdl(),
	m_newqKdl(),
	m_qdotKdl(),
	m_jac(NULL),
	m_armlength(0.0),
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
	if (m_buf)
		delete [] m_buf;
	m_constraints.clear();
}

Armature::JointConstraint_struct::JointConstraint_struct(SegmentMap::const_iterator _segment, unsigned int _y_nr, ConstraintCallback _function, void* _param, bool _freeParam, bool _substep):
	segment(_segment), value(), values(), function(_function), y_nr(_y_nr), param(_param), freeParam(_freeParam), substep(_substep)
{
	memset(values, 0, sizeof(values));
	memset(value, 0, sizeof(value));
	values[0].feedback = 20.0;
	values[1].feedback = 20.0;
	values[2].feedback = 20.0;
	values[0].tolerance = 1.0;
	values[1].tolerance = 1.0;
	values[2].tolerance = 1.0;
	values[0].values = &value[0];
	values[1].values = &value[1];
	values[2].values = &value[2];
	values[0].number = 1;
	values[1].number = 1;
	values[2].number = 1;
	switch (segment->second.segment.getJoint().getType()) {
	case Joint::RotX:
		value[0].id = ID_JOINT_RX;		
		values[0].id = ID_JOINT_RX;		
		v_nr = 1;
		break;
	case Joint::RotY:
		value[0].id = ID_JOINT_RY;		
		values[0].id = ID_JOINT_RY;		
		v_nr = 1;
		break;
	case Joint::RotZ:
		value[0].id = ID_JOINT_RZ;		
		values[0].id = ID_JOINT_RZ;		
		v_nr = 1;
		break;
	case Joint::TransX:
		value[0].id = ID_JOINT_TX;		
		values[0].id = ID_JOINT_TX;		
		v_nr = 1;
		break;
	case Joint::TransY:
		value[0].id = ID_JOINT_TY;		
		values[0].id = ID_JOINT_TY;		
		v_nr = 1;
		break;
	case Joint::TransZ:
		value[0].id = ID_JOINT_TZ;		
		values[0].id = ID_JOINT_TZ;		
		v_nr = 1;
		break;
	case Joint::Sphere:
		values[0].id = value[0].id = ID_JOINT_RX;		
		values[1].id = value[1].id = ID_JOINT_RY;
		values[2].id = value[2].id = ID_JOINT_RZ;		
		v_nr = 3;
		break;
	case Joint::Swing:
		values[0].id = value[0].id = ID_JOINT_RX;		
		values[1].id = value[1].id = ID_JOINT_RZ;		
		v_nr = 2;
		break;
	case Joint::None:
		break;
	}
}

Armature::JointConstraint_struct::~JointConstraint_struct()
{
	if (freeParam && param)
		free(param);
}

void Armature::initCache(Cache *_cache)
{
	m_cache = _cache;
	m_qCCh = -1;
	m_yCCh = -1;
	m_buf = NULL;
	if (m_cache) {
		// add a special channel for the joint
		m_qCCh = m_cache->addChannel(this, "q", m_qKdl.rows()*sizeof(double));
#if 0
		// for the constraints, instead of creating many different channels, we will
		// create a single channel for all the constraints
		if (m_nconstraint) {
			m_yCCh = m_cache->addChannel(this, "y", m_nconstraint*constraintCacheSize*sizeof(double));
			m_buf = new double[m_nconstraint*constraintCacheSize];
		}
		// store the initial cache position at timestamp 0
		pushConstraints(0);
#endif
		pushQ(0);
	}
}

void Armature::pushQ(CacheTS timestamp)
{
	if (m_qCCh >= 0) {
		// try to keep the cache if the joints are the same
		m_cache->addCacheVectorIfDifferent(this, m_qCCh, timestamp, &m_qKdl(0), m_qKdl.rows(), KDL::epsilon);
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
#if 0
void Armature::pushConstraints(CacheTS timestamp)
{
	if (m_yCCh >= 0) {
		double *buf = NULL;
		if (m_nconstraint) {
			double *item = m_buf;
			for (unsigned int i=0; i<m_nconstraint; i++) {
				JointConstraint_struct* pConstraint = m_constraints[i];
				*item++ = pConstraint->values.feedback;
				*item++ = pConstraint->values.tolerance;
				*item++ = pConstraint->value.yd;
				*item++ = pConstraint->value.yddot;
				*item++ = pConstraint->values.alpha;
			}
		}
		m_cache->addCacheVectorIfDifferent(this, m_yCCh, timestamp, m_buf, m_nconstraint*constraintCacheSize, KDL::epsilon);
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
				if (pConstraint->function != Joint1DOFLimitCallback) {
					pConstraint->values.feedback = *item++;
					pConstraint->values.tolerance = *item++;
					pConstraint->value.yd = *item++;
					pConstraint->value.yddot = *item++;
					pConstraint->values.alpha = *item++;
				} else {
					item += constraintCacheSize;
				}
			}
			m_yCTs = timestamp;
		}
		return (item) ? true : false;
	}
	return true;
}
#endif

bool Armature::addSegment(const std::string& segment_name, const std::string& hook_name, const Joint& joint, const double& q_rest, const Frame& f_tip, const Inertia& M)
{
	if (m_finalized)
		return false;

	Segment segment(joint, f_tip, M);
	if (!m_tree.addSegment(segment, segment_name, hook_name))
		return false;
	int ndof = joint.getNDof();
	for (int dof=0; dof<ndof; dof++) {
		Joint_struct js(joint.getType(), ndof, (&q_rest)[dof]);
		m_joints.push_back(js);
	}
	m_njoint+=ndof;
	return true;
}

bool Armature::getSegment(const std::string& name, const unsigned int q_size, const Joint* &p_joint, double &q_rest, double &q, const Frame* &p_tip)
{
	SegmentMap::const_iterator sit = m_tree.getSegment(name);
	if (sit == m_tree.getSegments().end())
		return false;
	p_joint = &sit->second.segment.getJoint();
	if (q_size < p_joint->getNDof())
		return false;
	p_tip = &sit->second.segment.getFrameToTip();
	for (unsigned int dof=0; dof<p_joint->getNDof(); dof++) {
		(&q_rest)[dof] = m_joints[sit->second.q_nr+dof].rest;
		(&q)[dof] = m_qKdl(sit->second.q_nr+dof);
	}
	return true;
}

double Armature::getMaxJointChange()
{
	if (!m_finalized)
		return 0.0;
	double maxJoint = 0.0;
	for (unsigned int i=0; i<m_njoint; i++) {
		// this is a very rough calculation, it doesn't work well for spherical joint
		double joint = fabs(m_oldqKdl(i)-m_qKdl(i));
		if (maxJoint < joint)
			maxJoint = joint;
	}
	return maxJoint;
}

double Armature::getMaxEndEffectorChange()
{
	if (!m_finalized)
		return 0.0;
	double maxDelta = 0.0;
	double delta;
	Twist twist;
	for (unsigned int i = 0; i<m_neffector; i++) {
		twist = diff(m_effectors[i].pose, m_effectors[i].oldpose);
		delta = twist.rot.Norm();
		if (delta > maxDelta)
			maxDelta = delta;
		delta = twist.vel.Norm();
		if (delta > maxDelta)
			maxDelta = delta;
	}
	return maxDelta;
}

int Armature::addConstraint(const std::string& segment_name, ConstraintCallback _function, void* _param, bool _freeParam, bool _substep)
{
	SegmentMap::const_iterator segment_it = m_tree.getSegment(segment_name);
	// not suitable for NDof joints
	if (segment_it == m_tree.getSegments().end()) {
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
	pConstraint = new JointConstraint_struct(segment_it, m_noutput, _function, _param, _freeParam, _substep);
	m_constraints.push_back(pConstraint);
	m_noutput += pConstraint->v_nr;
	return m_nconstraint++;
}

int Armature::addLimitConstraint(const std::string& segment_name, unsigned int dof, double _min, double _max)
{
	SegmentMap::const_iterator segment_it = m_tree.getSegment(segment_name);
	if (segment_it == m_tree.getSegments().end())
		return -1;
	const Joint& joint = segment_it->second.segment.getJoint();
	if (joint.getNDof() != 1 && joint.getType() != Joint::Swing) {
		// not suitable for Sphere joints
		return -1;
	}
	if ((joint.getNDof() == 1 && dof > 0) || (joint.getNDof() == 2 && dof > 1))
		return -1;
	Joint_struct& p_joint = m_joints[segment_it->second.q_nr+dof];
	p_joint.min = _min;
	p_joint.max = _max;
	p_joint.useLimit = true;
	return 0;
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

bool Armature::finalize()
{
	unsigned int i, j, c;
	if (m_finalized)
		return true;
	if (m_njoint == 0)
		return false;
	initialize(m_njoint, m_noutput, m_neffector);
	for (i=c=0; i<m_nconstraint; i++) {
		JointConstraint_struct* pConstraint = m_constraints[i];
		for (j=0; j<pConstraint->v_nr; j++, c++) {
			m_Cq(c,pConstraint->segment->second.q_nr+j) = 1.0;
			m_Wy(c) = pConstraint->values[j].alpha/*/(pConstraint->values.tolerance*pConstraint->values.feedback)*/;
		}
	}
	m_jacsolver= new KDL::TreeJntToJacSolver(m_tree);
	m_fksolver = new KDL::TreeFkSolverPos_recursive(m_tree);
	m_jac = new Jacobian(m_njoint);
	m_qKdl.resize(m_njoint);
	m_oldqKdl.resize(m_njoint);
	m_newqKdl.resize(m_njoint);
	m_qdotKdl.resize(m_njoint);
	for (i=0; i<m_njoint; i++) {
		m_newqKdl(i) = m_oldqKdl(i) = m_qKdl(i) = m_joints[i].rest;
	}
	updateJacobian();
	// estimate the maximum size of the robot arms
	double length;
	m_armlength = 0.0;
	for (i=0; i<m_neffector; i++) {
		length = 0.0;
		KDL::SegmentMap::const_iterator sit = m_tree.getSegment(m_effectors[i].name);
		while (sit->first != "root") {
			Frame tip = sit->second.segment.pose(m_qKdl(sit->second.q_nr));
			length += tip.p.Norm();
			sit = sit->second.parent;
		}
		if (length > m_armlength)
			m_armlength = length;
	}
	if (m_armlength < KDL::epsilon)
		m_armlength = KDL::epsilon;
	m_finalized = true;
	return true;
}

void Armature::pushCache(const Timestamp& timestamp)
{
	if (!timestamp.substep && timestamp.cache) {
		pushQ(timestamp.cacheTimestamp);
		//pushConstraints(timestamp.cacheTimestamp);
	}
}

bool Armature::setJointArray(const KDL::JntArray& joints)
{
	if (!m_finalized)
		return false;
	if (joints.rows() != m_qKdl.rows())
		return false;
	m_qKdl = joints;
	updateJacobian();
	return true;
}

const KDL::JntArray& Armature::getJointArray()
{
	return m_qKdl;
}

bool Armature::updateJoint(const Timestamp& timestamp, JointLockCallback& callback)
{
	if (!m_finalized)
		return false;
	
	// integration and joint limit
	// for spherical joint we must use a more sophisticated method
	unsigned int q_nr;
	double* qdot=&m_qdotKdl(0);
	double* q=&m_qKdl(0);
	double* newq=&m_newqKdl(0);
	double norm, qx, qz, CX, CZ, sx, sz;
	bool locked = false;
	int unlocked = 0;

	for (q_nr=0; q_nr<m_nq; ++q_nr)
		m_qdotKdl(q_nr)=m_qdot(q_nr);

	for (q_nr=0; q_nr<m_nq; ) {
		Joint_struct* joint = &m_joints[q_nr];
		if (!joint->locked) {
			switch (joint->type) {
			case KDL::Joint::Swing:
			{
				KDL::Rotation base = KDL::Rot(KDL::Vector(q[0],0.0,q[1]));
				(base*KDL::Rot(KDL::Vector(qdot[0],0.0,qdot[1])*timestamp.realTimestep)).GetXZRot().GetValue(newq);
				if (joint[0].useLimit) {
					if (joint[1].useLimit) {
						// elliptical limit
						sx = sz = 1.0;
						qx = newq[0];
						qz = newq[1];
						// determine in which quadrant we are
						if (qx > 0.0 && qz > 0.0) {
							CX = joint[0].max;
							CZ = joint[1].max;
						} else if (qx <= 0.0 && qz > 0.0) {
							CX = -joint[0].min;
							CZ = joint[1].max;
							qx = -qx;
							sx = -1.0;
						} else if (qx <= 0.0 && qz <= 0.0) {
							CX = -joint[0].min;
							CZ = -joint[1].min;
							qx = -qx;
							qz = -qz;
							sx = sz = -1.0;
						} else {
							CX = joint[0].max;
							CZ = -joint[0].min;
							qz = -qz;
							sz = -1.0;
						}
						if (CX < KDL::epsilon || CZ < KDL::epsilon) {
							// quadrant is degenerated
							if (qx > CX) {
								newq[0] = CX*sx;
								joint[0].locked = true;
							}
							if (qz > CZ) {
								newq[1] = CZ*sz;
								joint[0].locked = true;
							}
						} else {
							// general case
							qx /= CX;
							qz /= CZ;
							norm = KDL::sqrt(KDL::sqr(qx)+KDL::sqr(qz));
							if (norm > 1.0) {
								norm = 1.0/norm;
								newq[0] = qx*norm*CX*sx;
								newq[1] = qz*norm*CZ*sz;
								joint[0].locked = true;
							}
						}
					} else {
						// limit on X only
						qx = newq[0];
						if (qx > joint[0].max) {
							newq[0] = joint[0].max;
							joint[0].locked = true;
						} else if (qx < joint[0].min) {
							newq[0] = joint[0].min;
							joint[0].locked = true;
						}
					}
				} else if (joint[1].useLimit) {
					// limit on Z only
					qz = newq[1];
					if (qz > joint[1].max) {
						newq[1] = joint[1].max;
						joint[0].locked = true;
					} else if (qz < joint[1].min) {
						newq[1] = joint[1].min;
						joint[0].locked = true;
					}
				}
				if (joint[0].locked) {
					// check the difference from previous position
					locked = true;
					norm = KDL::sqr(newq[0]-q[0])+KDL::sqr(newq[1]-q[1]);
					if (norm < KDL::epsilon2) {
						// joint didn't move, no need to update the jacobian
						callback.lockJoint(q_nr, 2);
					} else {
						// joint moved, compute the corresponding velocity
						double deltaq[2];
						(base.Inverse()*KDL::Rot(KDL::Vector(newq[0],0.0,newq[1]))).GetXZRot().GetValue(deltaq);
						deltaq[0] /= timestamp.realTimestep;
						deltaq[1] /= timestamp.realTimestep;
						callback.lockJoint(q_nr, 2, deltaq);
						// no need to update the other joints, it will be done after next rerun
						goto end_loop;
					}
				} else
					unlocked++;
				break;
			}
			case KDL::Joint::Sphere:
			{
				(KDL::Rot(KDL::Vector(q))*KDL::Rot(KDL::Vector(qdot)*timestamp.realTimestep)).GetRot().GetValue(newq);
				// no limit on this joint
				unlocked++;
				break;
			}
			default:
				for (unsigned int i=0; i<joint->ndof; i++) {
					newq[i] = q[i]+qdot[i]*timestamp.realTimestep;
					if (joint[i].useLimit) {
						if (newq[i] > joint[i].max) {
							newq[i] = joint[i].max;
							joint[0].locked = true;
						} else if (newq[i] < joint[i].min) {
							newq[i] = joint[i].min;
							joint[0].locked = true;
						}
					}
				}
				if (joint[0].locked) {
					locked = true;
					norm = 0.0;
					// compute delta to locked position
					for (unsigned int i=0; i<joint->ndof; i++) {
						qdot[i] = newq[i] - q[i];
						norm += qdot[i]*qdot[i];
					}
					if (norm < KDL::epsilon2) {
						// joint didn't move, no need to update the jacobian
						callback.lockJoint(q_nr, joint->ndof);
					} else {
						// solver needs velocity, compute equivalent velocity
						for (unsigned int i=0; i<joint->ndof; i++) {
							qdot[i] /= timestamp.realTimestep;
						}
						callback.lockJoint(q_nr, joint->ndof, qdot);
						goto end_loop;
					}
				} else 
					unlocked++;
			}
		}
		qdot += joint->ndof;
		q    += joint->ndof;
		newq += joint->ndof;
		q_nr += joint->ndof;
	}
end_loop:
	// check if there any other unlocked joint
	for ( ; q_nr<m_nq; ) {
		Joint_struct* joint = &m_joints[q_nr];
		if (!joint->locked)
			unlocked++;
		q_nr += joint->ndof;
	}
	// if all joints have been locked no need to run the solver again
	return (unlocked) ? locked : false;
}

void Armature::updateKinematics(const Timestamp& timestamp){

    //Integrate m_qdot
	if (!m_finalized)
		return;

	// the new joint value have been computed already, just copy
	memcpy(&m_qKdl(0), &m_newqKdl(0), sizeof(double)*m_qKdl.rows());
	pushCache(timestamp);
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
	// remember that this object has moved 
	m_updated = true;
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

void Armature::updateControlOutput(const Timestamp& timestamp)
{
	if (!m_finalized)
		return;


	if (!timestamp.substep && !timestamp.reiterate && timestamp.interpolate) {
		popQ(timestamp.cacheTimestamp);
		//popConstraints(timestamp.cacheTimestamp);
	}

	if (!timestamp.substep) {
		// save previous joint state for getMaxJointChange()
		memcpy(&m_oldqKdl(0), &m_qKdl(0), sizeof(double)*m_qKdl.rows());
		for (unsigned int i=0; i<m_neffector; i++) {
			m_effectors[i].oldpose = m_effectors[i].pose;
		}
	}

	// remove all joint lock
	for (JointList::iterator jit=m_joints.begin(); jit!=m_joints.end(); ++jit) {
		(*jit).locked = false;
	}

	JointConstraintList::iterator it;
	unsigned int iConstraint;

	// scan through the constraints and call the callback functions
	for (iConstraint=0, it=m_constraints.begin(); it!=m_constraints.end(); it++, iConstraint++) {
		JointConstraint_struct* pConstraint = *it;
		unsigned int nr, i;
		for (i=0, nr = pConstraint->segment->second.q_nr; i<pConstraint->v_nr; i++, nr++) {
			*(double*)&pConstraint->value[i].y = m_qKdl(nr);
			*(double*)&pConstraint->value[i].ydot = m_qdotKdl(nr);
		}
		if (pConstraint->function && (pConstraint->substep || (!timestamp.reiterate && !timestamp.substep))) {
			(*pConstraint->function)(timestamp, pConstraint->values, pConstraint->v_nr, pConstraint->param);
		}
		// recompute the weight in any case, that's the most likely modification
		for (i=0, nr=pConstraint->y_nr; i<pConstraint->v_nr; i++, nr++) {
			m_Wy(nr) = pConstraint->values[i].alpha/*/(pConstraint->values.tolerance*pConstraint->values.feedback)*/;
			m_ydot(nr)=pConstraint->value[i].yddot+pConstraint->values[i].feedback*(pConstraint->value[i].yd-pConstraint->value[i].y);
		}
	}
}

bool Armature::setControlParameter(unsigned int constraintId, unsigned int valueId, ConstraintAction action, double value, double timestep)
{
	unsigned int lastid, i;
	if (constraintId == CONSTRAINT_ID_ALL) {
		constraintId = 0;
		lastid = m_nconstraint;
	} else if (constraintId < m_nconstraint) {
		lastid = constraintId+1;
	} else {
		return false;
	}
	for ( ; constraintId<lastid; ++constraintId) {
		JointConstraint_struct* pConstraint = m_constraints[constraintId];
		if (valueId == ID_JOINT) {
			for (i=0; i<pConstraint->v_nr; i++) {
				switch (action) {
				case ACT_TOLERANCE:
					pConstraint->values[i].tolerance = value;
					break;
				case ACT_FEEDBACK:
					pConstraint->values[i].feedback = value;
					break;
				case ACT_ALPHA:
					pConstraint->values[i].alpha = value;
					break;
				default:
					break;
				}
			}
		} else {
			for (i=0; i<pConstraint->v_nr; i++) {
				if (valueId == pConstraint->value[i].id) {
					switch (action) {
					case ACT_VALUE:
						pConstraint->value[i].yd = value;
						break;
					case ACT_VELOCITY:
						pConstraint->value[i].yddot = value;
						break;
					case ACT_TOLERANCE:
						pConstraint->values[i].tolerance = value;
						break;
					case ACT_FEEDBACK:
						pConstraint->values[i].feedback = value;
						break;
					case ACT_ALPHA:
						pConstraint->values[i].alpha = value;
						break;
					case ACT_NONE:
						break;
					}
				}
			}
		}
		if (m_finalized) {
			for (i=0; i<pConstraint->v_nr; i++) 
				m_Wy(pConstraint->y_nr+i) = pConstraint->values[i].alpha/*/(pConstraint->values.tolerance*pConstraint->values.feedback)*/;
		}
	}
	return true;
}

}

