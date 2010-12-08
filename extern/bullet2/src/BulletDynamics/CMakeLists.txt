include_directories( ${BULLET_PHYSICS_SOURCE_DIR}/src } )

set(BulletDynamics_SRCS
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

set(Root_HDRS
	../btBulletDynamicsCommon.h
	../btBulletCollisionCommon.h
)
set(ConstraintSolver_HDRS
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
set(Dynamics_HDRS
	Dynamics/btContinuousDynamicsWorld.h
	Dynamics/btDiscreteDynamicsWorld.h
	Dynamics/btDynamicsWorld.h
	Dynamics/btSimpleDynamicsWorld.h
	Dynamics/btRigidBody.h
)
set(Vehicle_HDRS
	Vehicle/btRaycastVehicle.h
	Vehicle/btVehicleRaycaster.h
	Vehicle/btWheelInfo.h
)

set(Character_HDRS
	Character/btCharacterControllerInterface.h
	Character/btKinematicCharacterController.h
)



set(BulletDynamics_HDRS
	${Root_HDRS}
	${ConstraintSolver_HDRS}
	${Dynamics_HDRS}
	${Vehicle_HDRS}
	${Character_HDRS}
)


add_library(BulletDynamics ${BulletDynamics_SRCS} ${BulletDynamics_HDRS})
SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES SOVERSION ${BULLET_VERSION})
if(BUILD_SHARED_LIBS)
	target_link_libraries(BulletDynamics BulletCollision LinearMath)
endif()

if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)
	install(TARGETS BulletDynamics DESTINATION lib)
	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h")
endif()

if(APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
	SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES FRAMEWORK true)
	
	SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES PUBLIC_HEADER "${Root_HDRS}")
	# Have to list out sub-directories manually:
	set_property(SOURCE ${ConstraintSolver_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/ConstraintSolver)
	set_property(SOURCE ${Dynamics_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/Dynamics)
	set_property(SOURCE ${Vehicle_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/Vehicle)
	set_property(SOURCE ${Character_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/Character)
		
endif()
