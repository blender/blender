INCLUDE_DIRECTORIES( ${BULLET_PHYSICS_SOURCE_DIR}/src } )

SET(BulletCollision_SRCS
	BroadphaseCollision/btAxisSweep3.cpp
	BroadphaseCollision/btBroadphaseProxy.cpp
	BroadphaseCollision/btCollisionAlgorithm.cpp
	BroadphaseCollision/btDispatcher.cpp
	BroadphaseCollision/btDbvtBroadphase.cpp
	BroadphaseCollision/btDbvt.cpp
	BroadphaseCollision/btMultiSapBroadphase.cpp
	BroadphaseCollision/btOverlappingPairCache.cpp
	BroadphaseCollision/btQuantizedBvh.cpp
	BroadphaseCollision/btSimpleBroadphase.cpp
	CollisionDispatch/btActivatingCollisionAlgorithm.cpp
	CollisionDispatch/btCollisionDispatcher.cpp
	CollisionDispatch/btCollisionObject.cpp
	CollisionDispatch/btCollisionWorld.cpp
	CollisionDispatch/btCompoundCollisionAlgorithm.cpp
	CollisionDispatch/btConvexConcaveCollisionAlgorithm.cpp
	CollisionDispatch/btDefaultCollisionConfiguration.cpp
	CollisionDispatch/btSphereSphereCollisionAlgorithm.cpp
	CollisionDispatch/btBoxBoxCollisionAlgorithm.cpp
	CollisionDispatch/btBoxBoxDetector.cpp
	CollisionDispatch/btGhostObject.cpp
	CollisionDispatch/btSphereBoxCollisionAlgorithm.cpp
	CollisionDispatch/btConvexPlaneCollisionAlgorithm.cpp
	CollisionDispatch/btSphereTriangleCollisionAlgorithm.cpp
	CollisionDispatch/btConvexConvexAlgorithm.cpp
	CollisionDispatch/btEmptyCollisionAlgorithm.cpp
	CollisionDispatch/btManifoldResult.cpp
	CollisionDispatch/btSimulationIslandManager.cpp
	CollisionDispatch/btUnionFind.cpp
	CollisionDispatch/SphereTriangleDetector.cpp
	CollisionShapes/btBoxShape.cpp
	CollisionShapes/btBvhTriangleMeshShape.cpp
	CollisionShapes/btCapsuleShape.cpp
	CollisionShapes/btCollisionShape.cpp
	CollisionShapes/btCompoundShape.cpp
	CollisionShapes/btConcaveShape.cpp
	CollisionShapes/btConeShape.cpp
	CollisionShapes/btConvexHullShape.cpp
	CollisionShapes/btConvexPointCloudShape.cpp
	CollisionShapes/btConvexShape.cpp
	CollisionShapes/btConvexInternalShape.cpp
	CollisionShapes/btConvexTriangleMeshShape.cpp
	CollisionShapes/btCylinderShape.cpp
	CollisionShapes/btEmptyShape.cpp
	CollisionShapes/btHeightfieldTerrainShape.cpp
	CollisionShapes/btMinkowskiSumShape.cpp
	CollisionShapes/btMultimaterialTriangleMeshShape.cpp
	CollisionShapes/btMultiSphereShape.cpp
	CollisionShapes/btOptimizedBvh.cpp
	CollisionShapes/btPolyhedralConvexShape.cpp
	CollisionShapes/btScaledBvhTriangleMeshShape.cpp
	CollisionShapes/btTetrahedronShape.cpp
	CollisionShapes/btSphereShape.cpp
	CollisionShapes/btShapeHull.cpp
	CollisionShapes/btStaticPlaneShape.cpp
	CollisionShapes/btStridingMeshInterface.cpp
	CollisionShapes/btTriangleCallback.cpp
	CollisionShapes/btTriangleBuffer.cpp
	CollisionShapes/btTriangleIndexVertexArray.cpp
	CollisionShapes/btTriangleIndexVertexMaterialArray.cpp
	CollisionShapes/btTriangleMesh.cpp
	CollisionShapes/btTriangleMeshShape.cpp
	CollisionShapes/btUniformScalingShape.cpp
	Gimpact/btContactProcessing.cpp
	Gimpact/btGImpactShape.cpp
	Gimpact/gim_contact.cpp
	Gimpact/btGImpactBvh.cpp
	Gimpact/btGenericPoolAllocator.cpp
	Gimpact/gim_memory.cpp
	Gimpact/btGImpactCollisionAlgorithm.cpp
	Gimpact/btTriangleShapeEx.cpp
	Gimpact/gim_tri_collision.cpp
	Gimpact/btGImpactQuantizedBvh.cpp
	Gimpact/gim_box_set.cpp
	NarrowPhaseCollision/btContinuousConvexCollision.cpp
	NarrowPhaseCollision/btGjkEpa2.cpp
	NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.cpp
	NarrowPhaseCollision/btConvexCast.cpp
	NarrowPhaseCollision/btGjkConvexCast.cpp
	NarrowPhaseCollision/btGjkPairDetector.cpp
	NarrowPhaseCollision/btMinkowskiPenetrationDepthSolver.cpp
	NarrowPhaseCollision/btPersistentManifold.cpp
	NarrowPhaseCollision/btRaycastCallback.cpp
	NarrowPhaseCollision/btSubSimplexConvexCast.cpp
	NarrowPhaseCollision/btVoronoiSimplexSolver.cpp
)

SET(Root_HDRS
	../btBulletCollisionCommon.h
)
SET(BroadphaseCollision_HDRS
	BroadphaseCollision/btAxisSweep3.h
	BroadphaseCollision/btBroadphaseInterface.h
	BroadphaseCollision/btBroadphaseProxy.h
	BroadphaseCollision/btCollisionAlgorithm.h
	BroadphaseCollision/btDispatcher.h
	BroadphaseCollision/btDbvtBroadphase.h
	BroadphaseCollision/btDbvt.h
	BroadphaseCollision/btMultiSapBroadphase.h
	BroadphaseCollision/btOverlappingPairCache.h
	BroadphaseCollision/btOverlappingPairCallback.h
	BroadphaseCollision/btQuantizedBvh.h
	BroadphaseCollision/btSimpleBroadphase.h
)
SET(CollisionDispatch_HDRS
	CollisionDispatch/btActivatingCollisionAlgorithm.h
	CollisionDispatch/btCollisionConfiguration.h
	CollisionDispatch/btCollisionCreateFunc.h
	CollisionDispatch/btCollisionDispatcher.h
	CollisionDispatch/btCollisionObject.h
	CollisionDispatch/btCollisionWorld.h
	CollisionDispatch/btCompoundCollisionAlgorithm.h
	CollisionDispatch/btConvexConcaveCollisionAlgorithm.h
	CollisionDispatch/btDefaultCollisionConfiguration.h
	CollisionDispatch/btSphereSphereCollisionAlgorithm.h
	CollisionDispatch/btBoxBoxCollisionAlgorithm.h
	CollisionDispatch/btBoxBoxDetector.h
	CollisionDispatch/btGhostObject.h
	CollisionDispatch/btSphereBoxCollisionAlgorithm.h
	CollisionDispatch/btConvexPlaneCollisionAlgorithm.h
	CollisionDispatch/btSphereTriangleCollisionAlgorithm.h
	CollisionDispatch/btConvexConvexAlgorithm.h
	CollisionDispatch/btEmptyCollisionAlgorithm.h
	CollisionDispatch/btManifoldResult.h
	CollisionDispatch/btSimulationIslandManager.h
	CollisionDispatch/btUnionFind.h
	CollisionDispatch/SphereTriangleDetector.h
)
SET(CollisionShapes_HDRS
	CollisionShapes/btBoxShape.h
	CollisionShapes/btBvhTriangleMeshShape.h
	CollisionShapes/btCapsuleShape.h
	CollisionShapes/btCollisionMargin
	CollisionShapes/btCollisionShape.h
	CollisionShapes/btCompoundShape.h
	CollisionShapes/btConcaveShape.h
	CollisionShapes/btConeShape.h
	CollisionShapes/btConvexHullShape.h
	CollisionShapes/btConvexPointCloudShape.h
	CollisionShapes/btConvexShape.h
	CollisionShapes/btConvexInternalShape.h
	CollisionShapes/btConvexTriangleMeshShape.h
	CollisionShapes/btCylinderShape.h
	CollisionShapes/btEmptyShape.h
	CollisionShapes/btHeightfieldTerrainShape.h
	CollisionShapes/btMinkowskiSumShape.h
	CollisionShapes/btMaterial.h
	CollisionShapes/btMultimaterialTriangleMeshShape.h
	CollisionShapes/btMultiSphereShape.h
	CollisionShapes/btOptimizedBvh.h
	CollisionShapes/btPolyhedralConvexShape.h
	CollisionShapes/btScaledBvhTriangleMeshShape.h
	CollisionShapes/btTetrahedronShape.h
	CollisionShapes/btSphereShape.h
	CollisionShapes/btShapeHull.h
	CollisionShapes/btStaticPlaneShape.h
	CollisionShapes/btStridingMeshInterface.h
	CollisionShapes/btTriangleCallback.h
	CollisionShapes/btTriangleBuffer.h
	CollisionShapes/btTriangleIndexVertexArray.h
	CollisionShapes/btTriangleIndexVertexMaterialArray.h
	CollisionShapes/btTriangleMesh.h
	CollisionShapes/btTriangleMeshShape.h
	CollisionShapes/btUniformScalingShape.h
)
SET(Gimpact_HDRS
	Gimpact/btGImpactShape.h
	Gimpact/gim_contact.h
	Gimpact/btGImpactBvh.h
	Gimpact/btGenericPoolAllocator.h
	Gimpact/gim_memory.h
	Gimpact/btGImpactCollisionAlgorithm.h
	Gimpact/btTriangleShapeEx.h
	Gimpact/gim_tri_collision.h
	Gimpact/btGImpactQuantizedBvh.h
	Gimpact/gim_box_set.h
)
SET(NarrowPhaseCollision_HDRS
	NarrowPhaseCollision/btContinuousConvexCollision.h
	NarrowPhaseCollision/btConvexCast.h
	NarrowPhaseCollision/btConvexPenetrationDepthSolver.h
	NarrowPhaseCollision/btDiscreteCollisionDetectorInterface.h
	NarrowPhaseCollision/btGjkConvexCast.h
	NarrowPhaseCollision/btGjkEpa2.h
	NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h
	NarrowPhaseCollision/btGjkPairDetector.h
	NarrowPhaseCollision/btManifoldPoint.h
	NarrowPhaseCollision/btMinkowskiPenetrationDepthSolver.h
	NarrowPhaseCollision/btPersistentManifold.h
	NarrowPhaseCollision/btPointCollector.h
	NarrowPhaseCollision/btRaycastCallback.h
	NarrowPhaseCollision/btSimplexSolverInterface.h
	NarrowPhaseCollision/btSubSimplexConvexCast.h
	NarrowPhaseCollision/btVoronoiSimplexSolver.h
)

SET(BulletCollision_HDRS
	${Root_HDRS}
	${BroadphaseCollision_HDRS}
	${CollisionDispatch_HDRS}
	${CollisionShapes_HDRS}
	${Gimpact_HDRS}
	${NarrowPhaseCollision_HDRS}
)


ADD_LIBRARY(BulletCollision ${BulletCollision_SRCS} ${BulletCollision_HDRS})
SET_TARGET_PROPERTIES(BulletCollision PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletCollision PROPERTIES SOVERSION ${BULLET_VERSION})
IF (BUILD_SHARED_LIBS)
	TARGET_LINK_LIBRARIES(BulletCollision LinearMath)
ENDIF (BUILD_SHARED_LIBS)

#INSTALL of other files requires CMake 2.6
IF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)
	INSTALL(TARGETS BulletCollision DESTINATION lib)
				INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h")
ENDIF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.5)

IF (APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
	SET_TARGET_PROPERTIES(BulletCollision PROPERTIES FRAMEWORK true)

	SET_TARGET_PROPERTIES(BulletCollision PROPERTIES PUBLIC_HEADER "${Root_HDRS}")
	# Have to list out sub-directories manually:
	SET_PROPERTY(SOURCE ${BroadphaseCollision_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/BroadphaseCollision)
	SET_PROPERTY(SOURCE ${CollisionDispatch_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/CollisionDispatch)
	SET_PROPERTY(SOURCE ${CollisionShapes_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/CollisionShapes)
	SET_PROPERTY(SOURCE ${Gimpact_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/Gimpact)
	SET_PROPERTY(SOURCE ${NarrowPhaseCollision_HDRS} PROPERTY MACOSX_PACKAGE_LOCATION Headers/NarrowPhaseCollision)

ENDIF (APPLE AND BUILD_SHARED_LIBS AND FRAMEWORK)
