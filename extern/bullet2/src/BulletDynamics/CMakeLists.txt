INCLUDE_DIRECTORIES(
${BULLET_PHYSICS_SOURCE_DIR}/src }
)
	
ADD_LIBRARY(LibBulletDynamics

	ConstraintSolver/btContactConstraint.cpp
	ConstraintSolver/btContactConstraint.h
	ConstraintSolver/btConeTwistConstraint.cpp
	ConstraintSolver/btConeTwistConstraint.h
	ConstraintSolver/btGeneric6DofConstraint.cpp
	ConstraintSolver/btGeneric6DofConstraint.h
	ConstraintSolver/btHingeConstraint.cpp
	ConstraintSolver/btHingeConstraint.h
	ConstraintSolver/btPoint2PointConstraint.cpp
	ConstraintSolver/btPoint2PointConstraint.h
	ConstraintSolver/btSequentialImpulseConstraintSolver.cpp
	ConstraintSolver/btSequentialImpulseConstraintSolver.h
	ConstraintSolver/btSliderConstraint.cpp
	ConstraintSolver/btSliderConstraint.h
	ConstraintSolver/btSolve2LinearConstraint.cpp
	ConstraintSolver/btSolve2LinearConstraint.h
	ConstraintSolver/btTypedConstraint.cpp
	ConstraintSolver/btTypedConstraint.h
	Dynamics/Bullet-C-API.cpp
	Dynamics/btDiscreteDynamicsWorld.cpp
	Dynamics/btDiscreteDynamicsWorld.h
	Dynamics/btSimpleDynamicsWorld.cpp
	Dynamics/btSimpleDynamicsWorld.h
	Dynamics/btRigidBody.cpp
	Dynamics/btRigidBody.h
	Vehicle/btRaycastVehicle.cpp
	Vehicle/btRaycastVehicle.h
	Vehicle/btWheelInfo.cpp
	Vehicle/btWheelInfo.h
)
