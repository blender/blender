/** \file itasc/Scene.cpp
 *  \ingroup itasc
 */
/*
 * Scene.cpp
 *
 *  Created on: Jan 5, 2009
 *      Author: rubensmits
 */

#include "Scene.hpp"
#include "ControlledObject.hpp"
#include "kdl/utilities/svd_eigen_HH.hpp"
#include <cstdio>

namespace iTaSC {

class SceneLock : public ControlledObject::JointLockCallback {
private:
	Scene* m_scene;
	Range  m_qrange;

public:
	SceneLock(Scene* scene) :
	  m_scene(scene), m_qrange(0,0) {}
	virtual ~SceneLock() {}

	void setRange(Range& range)
	{
		m_qrange = range;
	}
	// lock a joint, no need to update output
	virtual void lockJoint(unsigned int q_nr, unsigned int ndof)
	{
		q_nr += m_qrange.start;
		project(m_scene->m_Wq, Range(q_nr, ndof), m_qrange).setZero();
	}
	// lock a joint and update output in view of reiteration
	virtual void lockJoint(unsigned int q_nr, unsigned int ndof, double* qdot)
	{
		q_nr += m_qrange.start;
		project(m_scene->m_Wq, Range(q_nr, ndof), m_qrange).setZero();
		// update the ouput vector so that the movement of this joint will be 
		// taken into account and we can put the joint back in its initial position
		// which means that the jacobian doesn't need to be changed
		for (unsigned int i=0 ;i<ndof ; ++i, ++q_nr) {
			m_scene->m_ydot -= m_scene->m_A.col(q_nr)*qdot[i];
		}
	}
};

Scene::Scene():
	m_A(), m_B(), m_Atemp(), m_Wq(), m_Jf(), m_Jq(), m_Ju(), m_Cf(), m_Cq(), m_Jf_inv(),
	m_Vf(),m_Uf(), m_Wy(), m_ydot(), m_qdot(), m_xdot(), m_Sf(),m_tempf(),
	m_ncTotal(0),m_nqTotal(0),m_nuTotal(0),m_nsets(0),
	m_solver(NULL),m_cache(NULL) 
{
	m_minstep = 0.01;
	m_maxstep = 0.06;
}

Scene::~Scene() 
{
	ConstraintMap::iterator constraint_it;
	while ((constraint_it = constraints.begin()) != constraints.end()) {
		delete constraint_it->second;
		constraints.erase(constraint_it);
	}
	ObjectMap::iterator object_it;
	while ((object_it = objects.begin()) != objects.end()) {
		delete object_it->second;
		objects.erase(object_it);
	}
}

bool Scene::setParam(SceneParam paramId, double value)
{
	switch (paramId) {
	case MIN_TIMESTEP:
		m_minstep = value;
		break;
	case MAX_TIMESTEP:		
		m_maxstep = value;
		break;
	default:
		return false;
	}
	return true;
}

bool Scene::addObject(const std::string& name, Object* object, UncontrolledObject* base, const std::string& baseFrame)
{
	// finalize the object before adding
	if (!object->finalize())
		return false;
    //Check if Object is controlled or uncontrolled.
    if(object->getType()==Object::Controlled){
		int baseFrameIndex = base->addEndEffector(baseFrame);
		if (baseFrameIndex < 0)
			return false;
		std::pair<ObjectMap::iterator, bool> result;
		if (base->getNrOfCoordinates() == 0) {
			// base is fixed object, no coordinate range
			result = objects.insert(ObjectMap::value_type(
					name, new Object_struct(object,base,baseFrameIndex,
						Range(m_nqTotal,object->getNrOfCoordinates()),
						Range(m_ncTotal,((ControlledObject*)object)->getNrOfConstraints()),
						Range(0,0))));
		} else {
			// base is a moving object, must be in list already
		    ObjectMap::iterator base_it;
			for (base_it=objects.begin(); base_it != objects.end(); base_it++) {
				if (base_it->second->object == base)
					break;
			}
			if (base_it == objects.end())
				return false;
			result = objects.insert(ObjectMap::value_type(
					name, new Object_struct(object,base,baseFrameIndex,
						Range(m_nqTotal,object->getNrOfCoordinates()),
						Range(m_ncTotal,((ControlledObject*)object)->getNrOfConstraints()),
						base_it->second->coordinaterange)));
		}
		if (!result.second) {
			return false;
		}
        m_nqTotal+=object->getNrOfCoordinates();
        m_ncTotal+=((ControlledObject*)object)->getNrOfConstraints();
        return true;
    }
    if(object->getType()==Object::UnControlled){
		if ((WorldObject*)base != &Object::world)
			return false;
		std::pair<ObjectMap::iterator,bool> result = objects.insert(ObjectMap::value_type(
			name,new Object_struct(object,base,0,
						Range(0,0),
						Range(0,0),
						Range(m_nuTotal,object->getNrOfCoordinates()))));
        if(!result.second)
            return false;
        m_nuTotal+=object->getNrOfCoordinates();
        return true;
    }
    return false;
}

bool Scene::addConstraintSet(const std::string& name,ConstraintSet* task,const std::string& object1,const std::string& object2, const std::string& ee1, const std::string& ee2)
{
    //Check if objects exist:
    ObjectMap::iterator object1_it = objects.find(object1);
    ObjectMap::iterator object2_it = objects.find(object2);
    if(object1_it==objects.end()||object2_it==objects.end())
        return false;
	int ee1_index = object1_it->second->object->addEndEffector(ee1);
	int ee2_index = object2_it->second->object->addEndEffector(ee2);
	if (ee1_index < 0 || ee2_index < 0)
		return false;
	std::pair<ConstraintMap::iterator,bool> result = 
		constraints.insert(ConstraintMap::value_type(name,new ConstraintSet_struct(
			task,object1_it,ee1_index,object2_it,ee2_index,
			Range(m_ncTotal,task->getNrOfConstraints()),Range(6*m_nsets,6))));
    if(!result.second)
        return false;
    m_ncTotal+=task->getNrOfConstraints();
    m_nsets+=1;
    return true;
}

bool Scene::addSolver(Solver* _solver){
    if(m_solver==NULL){
        m_solver=_solver;
        return true;
    }
    else
        return false;
}

bool Scene::addCache(Cache* _cache){
    if(m_cache==NULL){
        m_cache=_cache;
        return true;
    }
    else
        return false;
}

bool Scene::initialize(){

    //prepare all matrices:
	if (m_ncTotal == 0 || m_nqTotal == 0 || m_nsets == 0)
		return false;

    m_A = e_zero_matrix(m_ncTotal,m_nqTotal);
	if (m_nuTotal > 0) {
		m_B = e_zero_matrix(m_ncTotal,m_nuTotal);
		m_xdot = e_zero_vector(m_nuTotal);
		m_Ju = e_zero_matrix(6*m_nsets,m_nuTotal);
	}
    m_Atemp = e_zero_matrix(m_ncTotal,6*m_nsets);
    m_ydot = e_zero_vector(m_ncTotal);
    m_qdot = e_zero_vector(m_nqTotal);
    m_Wq = e_zero_matrix(m_nqTotal,m_nqTotal);
    m_Wy = e_zero_vector(m_ncTotal);
    m_Jq = e_zero_matrix(6*m_nsets,m_nqTotal);
    m_Jf = e_zero_matrix(6*m_nsets,6*m_nsets);
    m_Jf_inv = m_Jf;
    m_Cf = e_zero_matrix(m_ncTotal,m_Jf.rows());
    m_Cq = e_zero_matrix(m_ncTotal,m_nqTotal);

    bool result=true;
	// finalize all objects
	for (ObjectMap::iterator it=objects.begin(); it!=objects.end(); ++it) {
		Object_struct* os = it->second;

		os->object->initCache(m_cache);
		if (os->constraintrange.count > 0)
			project(m_Cq,os->constraintrange,os->jointrange) = (((ControlledObject*)(os->object))->getCq());
	}

	m_ytask.resize(m_ncTotal);
	bool toggle=true;
	int cnt = 0;
    //Initialize all ConstraintSets:
    for(ConstraintMap::iterator it=constraints.begin();it!=constraints.end();++it){
        //Calculate the external pose:
		ConstraintSet_struct* cs = it->second;
		Frame external_pose;
		getConstraintPose(cs->task, cs, external_pose);
        result&=cs->task->initialise(external_pose);
		cs->task->initCache(m_cache);
		for (int i=0; i<cs->constraintrange.count; i++, cnt++) {
			m_ytask[cnt] = toggle;
		}
		toggle = !toggle;
		project(m_Cf,cs->constraintrange,cs->featurerange)=cs->task->getCf();
    }

    if(m_solver!=NULL)
        m_solver->init(m_nqTotal,m_ncTotal,m_ytask);
    else
        return false;


    return result;
}

bool Scene::getConstraintPose(ConstraintSet* constraint, void *_param, KDL::Frame& _pose)
{
	// function called from constraint when they need to get the external pose
	ConstraintSet_struct* cs = (ConstraintSet_struct*)_param;
	// verification, the pointer MUST match
	assert (constraint == cs->task);
	Object_struct* ob1 = cs->object1->second;
	Object_struct* ob2 = cs->object2->second;
	//Calculate the external pose:
	_pose=(ob1->base->getPose(ob1->baseFrameIndex)*ob1->object->getPose(cs->ee1index)).Inverse()*(ob2->base->getPose(ob2->baseFrameIndex)*ob2->object->getPose(cs->ee2index));
	return true;
}

bool Scene::update(double timestamp, double timestep, unsigned int numsubstep, bool reiterate, bool cache, bool interpolate)
{
	// we must have valid timestep and timestamp
	if (timestamp < KDL::epsilon || timestep < 0.0)
		return false;
	Timestamp ts;
	ts.realTimestamp = timestamp;
	// initially we start with the full timestep to allow velocity estimation over the full interval
	ts.realTimestep = timestep;
	setCacheTimestamp(ts);
	ts.substep = 0;
	// for reiteration don't load cache
	// reiteration=additional iteration with same timestamp if application finds the convergence not good enough
	ts.reiterate = (reiterate) ? 1 : 0;
	ts.interpolate = (interpolate) ? 1 : 0;
	ts.cache = (cache) ? 1 : 0;
	ts.update = 1;
	ts.numstep = (numsubstep & 0xFF);
	bool autosubstep = (numsubstep == 0) ? true : false;
	if (numsubstep < 1)
		numsubstep = 1;
	double timesubstep = timestep/numsubstep;
	double timeleft = timestep;

	if (timeleft == 0.0) {
		// this special case correspond to a request to cache data
		for(ObjectMap::iterator it=objects.begin();it!=objects.end();++it){
			it->second->object->pushCache(ts);
		}
		//Update the Constraints
		for(ConstraintMap::iterator it=constraints.begin();it!=constraints.end();++it){
			it->second->task->pushCache(ts);
		}
		return true;
	}

	// double maxqdot; // UNUSED
	e_scalar nlcoef;
	SceneLock lockCallback(this);
	Frame external_pose;
	bool locked;

	// initially we keep timestep unchanged so that update function compute the velocity over
	while (numsubstep > 0) {
		// get objects
		for(ObjectMap::iterator it=objects.begin();it!=objects.end();++it) {
			Object_struct* os = it->second;
			if (os->object->getType()==Object::Controlled) {
				((ControlledObject*)(os->object))->updateControlOutput(ts);
				if (os->constraintrange.count > 0) {
					project(m_ydot, os->constraintrange) = ((ControlledObject*)(os->object))->getControlOutput();
					project(m_Wy, os->constraintrange) = ((ControlledObject*)(os->object))->getWy();
					// project(m_Cq,os->constraintrange,os->jointrange) = (((ControlledObject*)(os->object))->getCq());
				}
				if (os->jointrange.count > 0) {
					project(m_Wq,os->jointrange,os->jointrange) = ((ControlledObject*)(os->object))->getWq();
				}
			}
			if (os->object->getType()==Object::UnControlled && ((UncontrolledObject*)os->object)->getNrOfCoordinates() != 0) {
	            ((UncontrolledObject*)(os->object))->updateCoordinates(ts);
				if (!ts.substep) {
					// velocity of uncontrolled object remains constant during substepping
					project(m_xdot,os->coordinaterange) = ((UncontrolledObject*)(os->object))->getXudot();
				}
			}
		}

		//get new Constraints values
		for(ConstraintMap::iterator it=constraints.begin();it!=constraints.end();++it) {
			ConstraintSet_struct* cs = it->second;
			Object_struct* ob1 = cs->object1->second;
			Object_struct* ob2 = cs->object2->second;

			if (ob1->base->updated() || ob1->object->updated() || ob2->base->updated() || ob2->object->updated()) {
				// the object from which the constraint depends have changed position
				// recompute the constraint pose
				getConstraintPose(cs->task, cs, external_pose);
				cs->task->initialise(external_pose);
			}
			cs->task->updateControlOutput(ts);
	        project(m_ydot,cs->constraintrange)=cs->task->getControlOutput();
			if (!ts.substep || cs->task->substep()) {
				project(m_Wy,cs->constraintrange)=(cs->task)->getWy();
				//project(m_Cf,cs->constraintrange,cs->featurerange)=cs->task->getCf();
			}

			project(m_Jf,cs->featurerange,cs->featurerange)=cs->task->getJf();
			//std::cout << "Jf = " << Jf << std::endl;
			//Transform the reference frame of this jacobian to the world reference frame
			Eigen::Block<e_matrix> Jf_part = project(m_Jf,cs->featurerange,cs->featurerange);
			changeBase(Jf_part,ob1->base->getPose(ob1->baseFrameIndex)*ob1->object->getPose(cs->ee1index));
			//std::cout << "Jf_w = " << Jf << std::endl;

			//calculate the inverse of Jf
			KDL::svd_eigen_HH(project(m_Jf,cs->featurerange,cs->featurerange),m_Uf,m_Sf,m_Vf,m_tempf);
			for(unsigned int i=0;i<6;++i)
				if(m_Sf(i)<KDL::epsilon)
					m_Uf.col(i).setConstant(0.0);
				else
					m_Uf.col(i)*=(1/m_Sf(i));
			project(m_Jf_inv,cs->featurerange,cs->featurerange).noalias()=m_Vf*m_Uf.transpose();

			//Get the robotjacobian associated with this constraintset
			//Each jacobian is expressed in robot base frame => convert to world reference
			//and negate second robot because it is taken reversed when closing the loop:
			if(ob1->object->getType()==Object::Controlled){
				project(m_Jq,cs->featurerange,ob1->jointrange) = (((ControlledObject*)(ob1->object))->getJq(cs->ee1index));
				//Transform the reference frame of this jacobian to the world reference frame:
				Eigen::Block<e_matrix> Jq_part = project(m_Jq,cs->featurerange,ob1->jointrange);
				changeBase(Jq_part,ob1->base->getPose(ob1->baseFrameIndex));
				// if the base of this object is moving, get the Ju part
				if (ob1->base->getNrOfCoordinates() != 0) {
					// Ju is already computed for world reference frame
					project(m_Ju,cs->featurerange,ob1->coordinaterange)=ob1->base->getJu(ob1->baseFrameIndex);
				}
			} else if (ob1->object->getType() == Object::UnControlled && ((UncontrolledObject*)ob1->object)->getNrOfCoordinates() != 0) {
				// object1 is uncontrolled moving object
				project(m_Ju,cs->featurerange,ob1->coordinaterange)=((UncontrolledObject*)ob1->object)->getJu(cs->ee1index);
			}
			if(ob2->object->getType()==Object::Controlled){
				//Get the robotjacobian associated with this constraintset
				// process a special case where object2 and object1 are equal but using different end effector
				if (ob1->object == ob2->object) {
					// we must create a temporary matrix
					e_matrix JqTemp(((ControlledObject*)(ob2->object))->getJq(cs->ee2index));
					//Transform the reference frame of this jacobian to the world reference frame:
					changeBase(JqTemp,ob2->base->getPose(ob2->baseFrameIndex));
					// substract in place
					project(m_Jq,cs->featurerange,ob2->jointrange) -= JqTemp;
				} else {
					project(m_Jq,cs->featurerange,ob2->jointrange) = -(((ControlledObject*)(ob2->object))->getJq(cs->ee2index));
					//Transform the reference frame of this jacobian to the world reference frame:
					Eigen::Block<e_matrix> Jq_part = project(m_Jq,cs->featurerange,ob2->jointrange);
					changeBase(Jq_part,ob2->base->getPose(ob2->baseFrameIndex));
				}
				if (ob2->base->getNrOfCoordinates() != 0) {
					// if base is the same as first object or first object base, 
					// that portion of m_Ju has been set already => substract inplace
					if (ob2->base == ob1->base || ob2->base == ob1->object) {
						project(m_Ju,cs->featurerange,ob2->coordinaterange) -= ob2->base->getJu(ob2->baseFrameIndex);
					} else {
						project(m_Ju,cs->featurerange,ob2->coordinaterange) = -ob2->base->getJu(ob2->baseFrameIndex);
					}
				}
			} else if (ob2->object->getType() == Object::UnControlled && ((UncontrolledObject*)ob2->object)->getNrOfCoordinates() != 0) {
				if (ob2->object == ob1->base || ob2->object == ob1->object) {
					project(m_Ju,cs->featurerange,ob2->coordinaterange) -= ((UncontrolledObject*)ob2->object)->getJu(cs->ee2index);
				} else {
					project(m_Ju,cs->featurerange,ob2->coordinaterange) = -((UncontrolledObject*)ob2->object)->getJu(cs->ee2index);
				}
			}
		}

	    //Calculate A
		m_Atemp.noalias()=m_Cf*m_Jf_inv;
		m_A.noalias() = m_Cq-(m_Atemp*m_Jq);
		if (m_nuTotal > 0) {
			m_B.noalias()=m_Atemp*m_Ju;
			m_ydot.noalias() += m_B*m_xdot;
		}

		//Call the solver with A, Wq, Wy, ydot to solver qdot:
		if(!m_solver->solve(m_A,m_Wy,m_ydot,m_Wq,m_qdot,nlcoef))
			// this should never happen
			return false;
		//send result to the objects
		for(ObjectMap::iterator it=objects.begin();it!=objects.end();++it) {
			Object_struct* os = it->second;
			if(os->object->getType()==Object::Controlled)
				((ControlledObject*)(os->object))->setJointVelocity(project(m_qdot,os->jointrange));
		}
		// compute the constraint velocity
		for(ConstraintMap::iterator it=constraints.begin();it!=constraints.end();++it){
			ConstraintSet_struct* cs = it->second;
			Object_struct* ob1 = cs->object1->second;
			Object_struct* ob2 = cs->object2->second;
			//Calculate the twist of the world reference frame due to the robots (Jq*qdot+Ju*chiudot):
			e_vector6 external_vel = e_zero_vector(6);
			if (ob1->jointrange.count > 0)
				external_vel.noalias() += (project(m_Jq,cs->featurerange,ob1->jointrange)*project(m_qdot,ob1->jointrange));
			if (ob2->jointrange.count > 0)
				external_vel.noalias() += (project(m_Jq,cs->featurerange,ob2->jointrange)*project(m_qdot,ob2->jointrange));
			if (ob1->coordinaterange.count > 0)
				external_vel.noalias() += (project(m_Ju,cs->featurerange,ob1->coordinaterange)*project(m_xdot,ob1->coordinaterange));
			if (ob2->coordinaterange.count > 0)
				external_vel.noalias() += (project(m_Ju,cs->featurerange,ob2->coordinaterange)*project(m_xdot,ob2->coordinaterange));
			//the twist caused by the constraint must be opposite because of the closed loop
			//estimate the velocity of the joints using the inverse jacobian
			e_vector6 estimated_chidot = project(m_Jf_inv,cs->featurerange,cs->featurerange)*(-external_vel);
			cs->task->setJointVelocity(estimated_chidot);
		}

		if (autosubstep) {
			// automatic computing of substep based on maximum joint change
			// and joint limit gain variation
			// We will pass the joint velocity to each object and they will recommend a maximum timestep
			timesubstep = timeleft;
			// get armature max joint velocity to estimate the maximum duration of integration
			// maxqdot = m_qdot.cwise().abs().maxCoeff(); // UNUSED
			double maxsubstep = nlcoef*m_maxstep;
			if (maxsubstep < m_minstep)
				maxsubstep = m_minstep;
			if (timesubstep > maxsubstep)
				timesubstep = maxsubstep;
			for(ObjectMap::iterator it=objects.begin();it!=objects.end();++it){
				Object_struct* os = it->second;
				if(os->object->getType()==Object::Controlled)
					((ControlledObject*)(os->object))->getMaxTimestep(timesubstep);
			}
			for(ConstraintMap::iterator it=constraints.begin();it!=constraints.end();++it){
				ConstraintSet_struct* cs = it->second;
				cs->task->getMaxTimestep(timesubstep);
			}
			// use substep that are even dividers of timestep for more regularity
			maxsubstep = 2.0*floor(timestep/2.0/timesubstep-0.66666);
			timesubstep = (maxsubstep < 0.0) ? timestep : timestep/(2.0+maxsubstep);
			if (timesubstep >= timeleft-(m_minstep/2.0)) {
				timesubstep = timeleft;
				numsubstep = 1;
				timeleft = 0.;
			} else {
				numsubstep = 2;
				timeleft -= timesubstep;
			}
		}
		if (numsubstep > 1) {
			ts.substep = 1;
		} else {
			// set substep to false for last iteration so that controlled output 
			// can be updated in updateKinematics() and model_update)() before next call to Secne::update()
			ts.substep = 0;
		}
		// change timestep so that integration is done correctly
		ts.realTimestep = timesubstep;

		do {
			ObjectMap::iterator it;
			Object_struct* os;
			locked = false;
			for(it=objects.begin();it!=objects.end();++it){
				os = it->second;
				if (os->object->getType()==Object::Controlled) {
					lockCallback.setRange(os->jointrange);
					if (((ControlledObject*)os->object)->updateJoint(ts, lockCallback)) {
						// this means one of the joint was locked and we must rerun
						// the solver to update the remaining joints
						locked = true;
						break;
					}
				}
			}
			if (locked) {
				// Some rows of m_Wq have been cleared so that the corresponding joint will not move
				if(!m_solver->solve(m_A,m_Wy,m_ydot,m_Wq,m_qdot,nlcoef))
					// this should never happen
					return false;

				//send result to the objects
				for(it=objects.begin();it!=objects.end();++it) {
					os = it->second;
					if(os->object->getType()==Object::Controlled)
						((ControlledObject*)(os->object))->setJointVelocity(project(m_qdot,os->jointrange));
				}
			}
		} while (locked);

		//Update the Objects
		for(ObjectMap::iterator it=objects.begin();it!=objects.end();++it){
			it->second->object->updateKinematics(ts);
			// mark this object not updated since the constraint will be updated anyway
			// this flag is only useful to detect external updates
			it->second->object->updated(false);
		}
		//Update the Constraints
		for(ConstraintMap::iterator it=constraints.begin();it!=constraints.end();++it){
			ConstraintSet_struct* cs = it->second;
			//Calculate the external pose:
			getConstraintPose(cs->task, cs, external_pose);
			cs->task->modelUpdate(external_pose,ts);
			// update the constraint output and cache
			cs->task->updateKinematics(ts);
		}
		numsubstep--;
	}
	return true;
}

}
