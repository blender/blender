INCLUDE_DIRECTORIES( ${BULLET_PHYSICS_SOURCE_DIR}/src } )

SET(BulletDynamics_SRCS
	ConstraintSolver/btContactConstraint.cpp
	ConstraintSolver/btConeTwistConstraint.cpp
	ConstraintSolver/btGeneric6DofConstraint.cpp
	ConstraintSolver/btHingeConstraint.cpp
	ConstraintSolver/btPoint2PointConstraint.cpp
	ConstraintSolver/btSequentialImpulseConstraintSolver.cpp
	ConstraintSolver/btSliderConstraint.cpp
	ConstraintSolver/btSolve2LinearConstraint.cpp
	ConstraintSolver/btTypedConstraint.cpp
	Dynamics/Bullet-C-API.cpp
	Dynamics/btDiscreteDynamicsWorld.cpp
	Dynamics/btSimpleDynamicsWorld.cpp
	Dynamics/btRigidBody.cpp
	Vehicle/btRaycastVehicle.cpp
	Vehicle/btWheelInfo.cpp
	Character/btKinematicCharacterController.cpp
)

SET(Root_HDRS
	../btBulletDynamicsCommon.h
	../btBulletCollisionCommon.h
)
SET(ConstraintSolver_HDRS
	ConstraintSolver/btConstraintSolver.h
	ConstraintSolver/btContactConstraint.h
	ConstraintSolver/btContactSolverInfo.h
	ConstraintSolver/btConeTwistConstraint.h
	ConstraintSolver/btGeneric6DofConstraint.h
	ConstraintSolver/btHingeConstraint.h
	ConstraintSolver/btJacobianEntry.h
	ConstraintSolver/btPoint2PointConstraint.h
	ConstraintSolver/btSequentialImpulseConstraintSolver.h
	ConstraintSolver/btSliderConstraint.h
	ConstraintSolver/btSolve2LinearConstraint.h
	ConstraintSolver/btSolverBody.h
	ConstraintSolver/btSolverConstraint.h
	ConstraintSolver/btTypedConstraint.h
)
SET(Dynamics_HDRS
	Dynamics/btContinuousDynamicsWorld.h
	Dynamics/btDiscreteDynamicsWorld.h
	Dynamics/btDynamicsWorld.h
	Dynamics/btSimpleDynamicsWorld.h
	Dynamics/btRigidBody.h
)
SET(Vehicle_HDRS
	Vehicle/btRaycastVehicle.h
	Vehicle/btVehicleRaycaster.h
	Vehicle/btWheelInfo.h
)

SET(Character_HDRS
	Character/btCharacterControllerInterface.h
	Character/btKinematicCharacterController.h
)



SET(BulletDynamics_HDRS
	${Root_HDRS}
	${ConstraintSolver_HDRS}
	${Dynamics_HDRS}
	${Vehicle_HDRS}
	${Character_HDRS}
)


ADD_LIBRARY(BulletDynamics ${BulletDynamics_SRCS} ${BulletDynamics_HDRS})
SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES SOVERSION ${BULLET_VERSION})
IF (BUILD_SHARED_LIBS)
	TARGET_LINK_LIBRARIES(BulletDynamics BulletCollision LinearMath)
ENDIF (BUILD_SHARED_LIBS)

IF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)
	INSTALL(TARGETS BulletDynamics DESTINATION lib)
	INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h")
ENDIF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)

IF (APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
	SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES FRAMEWORK true)
	
	SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES PUBLIC_HEADER "${Root_HDRS}")
	# Have to list out sub-directories manually:
	SET_PROPERTY(SOURCE ${ConstraintSolver_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/ConstraintSolver)
	SET_PROPERTY(SOURCE ${Dynamics_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/Dynamics)
	SET_PROPERTY(SOURCE ${Vehicle_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/Vehicle)
	SET_PROPERTY(SOURCE ${Character_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/Character)
		
ENDIF (APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
