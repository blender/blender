
INCLUDE_DIRECTORIES(
${BULLET_PHYSICS_SOURCE_DIR}/src }
)

SET(BulletSoftBody_SRCS
	btSoftBody.cpp
	btSoftBodyHelpers.cpp
	btSoftBodyRigidBodyCollisionConfiguration.cpp
	btSoftRigidCollisionAlgorithm.cpp
	btSoftSoftCollisionAlgorithm.cpp
	btSoftBodyConcaveCollisionAlgorithm.cpp
	btSoftRigidDynamicsWorld.cpp
)

SET(BulletSoftBody_HDRS
	btSoftBody.h
	btSparseSDF.h
	btSoftBodyHelpers.h
	btSoftRigidCollisionAlgorithm.h
	btSoftSoftCollisionAlgorithm.h
	btSoftBodyConcaveCollisionAlgorithm.h
	btSoftRigidDynamicsWorld.h
)



ADD_LIBRARY(BulletSoftBody  ${BulletSoftBody_SRCS} ${BulletSoftBody_HDRS})
SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES SOVERSION ${BULLET_VERSION})
IF (BUILD_SHARED_LIBS)
	TARGET_LINK_LIBRARIES(BulletSoftBody BulletDynamics)
ENDIF (BUILD_SHARED_LIBS)

IF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)
	INSTALL(TARGETS BulletSoftBody DESTINATION lib)
				INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h")
ENDIF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)

IF (APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
	SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES FRAMEWORK true)
	SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES PUBLIC_HEADER "${BulletSoftBody_HDRS}")
ENDIF (APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
