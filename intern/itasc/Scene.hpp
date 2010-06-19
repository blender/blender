/* $Id: Scene.hpp 20622 2009-06-04 12:47:59Z ben2610 $
 * Scene.hpp
 *
 *  Created on: Jan 5, 2009
 *      Author: rubensmits
 */

#ifndef SCENE_HPP_
#define SCENE_HPP_

#include "eigen_types.hpp"

#include "WorldObject.hpp"
#include "ConstraintSet.hpp"
#include "Solver.hpp"

#include <map>

namespace iTaSC {

class SceneLock;

class Scene {
	friend class SceneLock;	
public:
	enum SceneParam {
		MIN_TIMESTEP	= 0,
		MAX_TIMESTEP,

		COUNT
	};


	Scene();
    virtual ~Scene();

	bool addObject(const std::string& name, Object* object, UncontrolledObject* base=&Object::world, const std::string& baseFrame="");
	bool addConstraintSet(const std::string& name, ConstraintSet* task,const std::string& object1,const std::string& object2,const std::string& ee1="",const std::string& ee2="");
    bool addSolver(Solver* _solver);
    bool addCache(Cache* _cache);
    bool initialize();
    bool update(double timestamp, double timestep, unsigned int numsubstep=1, bool reiterate=false, bool cache=true, bool interpolate=true);
	bool setParam(SceneParam paramId, double value);

	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

private:
    e_matrix m_A,m_B,m_Atemp,m_Wq,m_Jf,m_Jq,m_Ju,m_Cf,m_Cq,m_Jf_inv;
	e_matrix6 m_Vf,m_Uf;
    e_vector m_Wy,m_ydot,m_qdot,m_xdot;
	e_vector6 m_Sf,m_tempf;
	double m_minstep;
	double m_maxstep;
    unsigned int m_ncTotal,m_nqTotal,m_nuTotal,m_nsets;
	std::vector<bool> m_ytask;

    Solver* m_solver;
	Cache* m_cache;


    struct Object_struct{
        Object* object;
        UncontrolledObject* base;
		unsigned int baseFrameIndex;
        Range constraintrange;
        Range jointrange;
		Range coordinaterange;	// Xu range of base when object is controlled
										// Xu range of object when object is uncontrolled 

        Object_struct(Object* _object,UncontrolledObject* _base,unsigned int _baseFrameIndex,Range nq_range,Range nc_range,Range nu_range):
            object(_object),base(_base),baseFrameIndex(_baseFrameIndex),constraintrange(nc_range),jointrange(nq_range),coordinaterange(nu_range)
            {};
    };
    typedef std::map<std::string,Object_struct*> ObjectMap;

    struct ConstraintSet_struct{
        ConstraintSet* task;
        ObjectMap::iterator object1;
        ObjectMap::iterator object2;
        Range constraintrange;
        Range featurerange;
		unsigned int ee1index;
		unsigned int ee2index;
        ConstraintSet_struct(ConstraintSet* _task,
			ObjectMap::iterator _object1,unsigned int _ee1index,
			ObjectMap::iterator _object2,unsigned int _ee2index,
			Range nc_range,Range coord_range):
				task(_task),
				object1(_object1),object2(_object2),
				constraintrange(nc_range),featurerange(coord_range),
				ee1index(_ee1index), ee2index(_ee2index)
            {};
    };
    typedef std::map<std::string,ConstraintSet_struct*> ConstraintMap;

    ObjectMap objects;
    ConstraintMap constraints;

	static bool getConstraintPose(ConstraintSet* constraint, void *_param, KDL::Frame& _pose);
};

}

#endif /* SCENE_HPP_ */
