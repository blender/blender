INCLUDE_DIRECTORIES(
${BULLET_PHYSICS_SOURCE_DIR}/src }
)
	
ADD_LIBRARY(LibBulletDynamics

	ConstraintSolver/btContactConstraint.cpp
	ConstraintSolver/btGeneric6DofConstraint.cpp
	ConstraintSolver/btHingeConstraint.cpp
	ConstraintSolver/btPoint2PointConstraint.cpp
	ConstraintSolver/btSequentialImpulseConstraintSolver.cpp
	ConstraintSolver/btSolve2LinearConstraint.cpp
	ConstraintSolver/btTypedConstraint.cpp
	Dynamics/btDiscreteDynamicsWorld.cpp
	Dynamics/btSimpleDynamicsWorld.cpp
	Dynamics/Bullet-C-API.cpp
	Dynamics/btRigidBody.cpp
	Vehicle/btRaycastVehicle.cpp
	Vehicle/btWheelInfo.cpp
)
