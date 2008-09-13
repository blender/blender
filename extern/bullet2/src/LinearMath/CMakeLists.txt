
INCLUDE_DIRECTORIES(
${BULLET_PHYSICS_SOURCE_DIR}/src }
)

ADD_LIBRARY(LibLinearMath
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
		btConvexHull.cpp
		btMinMax.h
		btQuaternion.h
		btStackAlloc.h
		btGeometryUtil.h
		btMotionState.h
		btTransform.h
		btAlignedAllocator.h
		btIDebugDraw.h
		btPoint3.h
		btQuickprof.h
		btTransformUtil.h
		btQuickprof.cpp
		btGeometryUtil.cpp
		btAlignedAllocator.cpp
)

