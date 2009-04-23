/* $Id$
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

    e_matrix m_A,m_B,m_Atemp,m_Wq,m_Jf,m_Jq,m_Ju,m_Cf,m_Cq,m_Jf_inv;
	e_matrix6 m_Vf,m_Uf;
    e_vector m_Wy,m_ydot,m_qdot,m_xdot;
	e_vector6 m_Sf,m_tempf;

    unsigned int m_ncTotal,m_nqTotal,m_nuTotal,m_nsets;
	std::vector<bool> m_ytask;

    Solver* m_solver;
	Cache* m_cache;

Scene::Scene():
	m_A(), m_B(), m_Atemp(), m_Wq(), m_Jf(), m_Jq(), m_Ju(), m_Cf(), m_Cq(), m_Jf_inv(),
	m_Vf(),m_Uf(), m_Wy(), m_ydot(), m_qdot(), m_xdot(), m_Sf(),m_tempf(),
	m_ncTotal(0),m_nqTotal(0),m_nuTotal(0),m_nsets(0),
	m_solver(NULL),m_cache(NULL) 
{

}

Scene::~Scene() 
{
	ConstraintMap::iterator constraint_it;
	while ((constraint_it = constraints.begin()) != constraints.end()) {
		constraint_it->second->task->registerPoseCallback(NULL, NULL);
		delete constraint_it->second;
		constraints.erase(constraint_it);
	}
	ObjectMap::iterator object_it;
	while ((object_it = objects.begin()) != objects.end()) {
		delete object_it->second;
		objects.erase(object_it);
	}
}


bool Scene::addObject(const std::string& name, Object* object, UncontrolledObject* base, const std::string& baseFrame)
{
	// finalize the object before adding
	object->finalize();
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
	// set the callback function to get the external pose
	task->registerPoseCallback(getConstraintPose, result.first->second);
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
		it->second->object->initCache(m_cache);
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

bool Scene::update(double timestamp, double timestep, unsigned int numsubstep, bool reiterate)
{
	// we must have valid timestep and timestamp
	if (timestamp < KDL::epsilon || timestep < KDL::epsilon)
		return false;
	Timestamp ts;
	ts.realTimestamp = timestamp;
	ts.realTimestep = timestep;
	setCacheTimestamp(ts);
	ts.substep = 0;
	// for reiteration don't load cache
	// reiteration=additional iteration with same timestamp if application finds the convergence not good enough
	ts.reiterate = (reiterate) ? 1 : 0;
	bool autosubstep = (numsubstep == 0) ? true : false;
	if (numsubstep < 1)
		numsubstep = 1;
	double timesubstep = timestep/numsubstep;
	double timeleft = timestep;
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
				}
				if (!ts.substep && os->jointrange.count > 0) {
					// no need to update these when substepping
					project(m_Wq,os->jointrange,os->jointrange) = ((ControlledObject*)(os->object))->getWq();
					if (os->constraintrange.count > 0)
						project(m_Cq,os->constraintrange,os->jointrange) = (((ControlledObject*)(os->object))->getCq());
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

			cs->task->updateControlOutput(ts);
	        project(m_ydot,cs->constraintrange)=cs->task->getControlOutput();
			if (!ts.substep) {
				// they don't change during substepping
				project(m_Wy,cs->constraintrange)=(cs->task)->getWy();
				project(m_Cf,cs->constraintrange,cs->featurerange)=cs->task->getCf();
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
			project(m_Jf_inv,cs->featurerange,cs->featurerange)=(m_Vf*m_Uf.transpose()).lazy();

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
		m_Atemp=(m_Cf*m_Jf_inv).lazy();
		m_A = m_Cq-(m_Atemp*m_Jq).lazy();
		if (m_nuTotal > 0) {
			m_B=(m_Atemp*m_Ju).lazy();
			m_ydot += (m_B*m_xdot).lazy();
		}

		//Call the solver with A, Wq, Wy, ydot to solver qdot:
		if(!m_solver->solve(m_A,m_Wy,m_ydot,m_Wq,m_qdot))
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
				external_vel += (project(m_Jq,cs->featurerange,ob1->jointrange)*project(m_qdot,ob1->jointrange)).lazy();
			if (ob2->jointrange.count > 0)
				external_vel += (project(m_Jq,cs->featurerange,ob2->jointrange)*project(m_qdot,ob2->jointrange)).lazy();
			if (ob1->coordinaterange.count > 0)
				external_vel += (project(m_Ju,cs->featurerange,ob1->coordinaterange)*project(m_xdot,ob1->coordinaterange)).lazy();
			if (ob2->coordinaterange.count > 0)
				external_vel += (project(m_Ju,cs->featurerange,ob2->coordinaterange)*project(m_xdot,ob2->coordinaterange)).lazy();
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
			for(ObjectMap::iterator it=objects.begin();it!=objects.end();++it){
				Object_struct* os = it->second;
				if(os->object->getType()==Object::Controlled)
					((ControlledObject*)(os->object))->getMaxTimestep(timesubstep);
			}
			for(ConstraintMap::iterator it=constraints.begin();it!=constraints.end();++it){
				ConstraintSet_struct* cs = it->second;
				cs->task->getMaxTimestep(timesubstep);
			}
			if (timesubstep >= timeleft-KDL::epsilon) {
				timesubstep = timeleft;
				numsubstep = 1;
				timeleft = 0.;
			} else {
				numsubstep = 2;
				timeleft -= timesubstep;
			}
		}
		if (numsubstep > 1) {
			// change timestep so that for next substep, updateControlOutput() will have
			// the correct timestep (although it is not using it)
			ts.realTimestep = timesubstep;
			ts.substep = 1;
		} else {
			// set substep to false for last iteration so that controlled output 
			// can be updated in updateKinematics() and model_update)() before next call to Secne::update()
			ts.substep = 0;
		}

		//Update the Objects
		for(ObjectMap::iterator it=objects.begin();it!=objects.end();++it){
			it->second->object->updateKinematics(ts);
		}
		//Update the Constraints
		for(ConstraintMap::iterator it=constraints.begin();it!=constraints.end();++it){
			ConstraintSet_struct* cs = it->second;
			//Calculate the external pose:
			Frame external_pose;
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
