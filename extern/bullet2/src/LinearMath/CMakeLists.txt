
include_directories(
${BULLET_PHYSICS_SOURCE_DIR}/src }
)

set(LinearMath_SRCS
		btConvexHull.cpp
		btQuickprof.cpp
		btGeometryUtil.cpp
		btAlignedAllocator.cpp
)

set(LinearMath_HDRS
		btAlignedObjectArray.h
		btList.h
		btPoolAllocator.h
		btRandom.h
		btVector3.h
		btDefaultMotionState.h
		btMatrix3x3.h
		btQuadWord.h
		btHashMap.h
		btScalar.h
		btAabbUtil2.h
		btConvexHull.h
		btMinMax.h
		btQuaternion.h
		btStackAlloc.h
		btGeometryUtil.h
		btMotionState.h
		btTransform.h
		btAlignedAllocator.h
		btIDebugDraw.h
		btQuickprof.h
		btTransformUtil.h
)

add_library(LinearMath ${LinearMath_SRCS} ${LinearMath_HDRS})
SET_TARGET_PROPERTIES(LinearMath PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(LinearMath PROPERTIES SOVERSION ${BULLET_VERSION})

#FILES_MATCHING requires CMake 2.6
if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)
	install(TARGETS LinearMath DESTINATION lib)
	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h")
endif()

if(APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
	SET_TARGET_PROPERTIES(LinearMath PROPERTIES FRAMEWORK true)
	SET_TARGET_PROPERTIES(LinearMath PROPERTIES PUBLIC_HEADER "${LinearMath_HDRS}")
endif()
