
include_directories(
${BULLET_PHYSICS_SOURCE_DIR}/src }
)

set(BulletSoftBody_SRCS
	btSoftBody.cpp
	btSoftBodyHelpers.cpp
	btSoftBodyRigidBodyCollisionConfiguration.cpp
	btSoftRigidCollisionAlgorithm.cpp
	btSoftSoftCollisionAlgorithm.cpp
	btSoftBodyConcaveCollisionAlgorithm.cpp
	btSoftRigidDynamicsWorld.cpp
)

set(BulletSoftBody_HDRS
	btSoftBody.h
	btSparseSDF.h
	btSoftBodyHelpers.h
	btSoftRigidCollisionAlgorithm.h
	btSoftSoftCollisionAlgorithm.h
	btSoftBodyConcaveCollisionAlgorithm.h
	btSoftRigidDynamicsWorld.h
)



add_library(BulletSoftBody  ${BulletSoftBody_SRCS} ${BulletSoftBody_HDRS})
SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES SOVERSION ${BULLET_VERSION})
if(BUILD_SHARED_LIBS)
	target_link_libraries(BulletSoftBody BulletDynamics)
endif()

if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)
	install(TARGETS BulletSoftBody DESTINATION lib)
				install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h")
endif()

if(APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
	SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES FRAMEWORK true)
	SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES PUBLIC_HEADER "${BulletSoftBody_HDRS}")
endif()
