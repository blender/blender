# $Id$
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurai, Erwin Coumans
#
# ***** END GPL LICENSE BLOCK *****

SET(INC
	.
	src
)

SET(SRC
	src/BulletCollision/BroadphaseCollision/btAxisSweep3.cpp
	src/BulletCollision/BroadphaseCollision/btBroadphaseProxy.cpp
	src/BulletCollision/BroadphaseCollision/btCollisionAlgorithm.cpp
	src/BulletCollision/BroadphaseCollision/btDbvt.cpp
	src/BulletCollision/BroadphaseCollision/btDbvtBroadphase.cpp
	src/BulletCollision/BroadphaseCollision/btDispatcher.cpp
	src/BulletCollision/BroadphaseCollision/btMultiSapBroadphase.cpp
	src/BulletCollision/BroadphaseCollision/btOverlappingPairCache.cpp
	src/BulletCollision/BroadphaseCollision/btQuantizedBvh.cpp
	src/BulletCollision/BroadphaseCollision/btSimpleBroadphase.cpp
	src/BulletCollision/CollisionDispatch/SphereTriangleDetector.cpp
	src/BulletCollision/CollisionDispatch/btActivatingCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btBoxBoxCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btBoxBoxDetector.cpp
	src/BulletCollision/CollisionDispatch/btCollisionDispatcher.cpp
	src/BulletCollision/CollisionDispatch/btCollisionObject.cpp
	src/BulletCollision/CollisionDispatch/btCollisionWorld.cpp
	src/BulletCollision/CollisionDispatch/btCompoundCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btConvexConcaveCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btConvexConvexAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btConvexPlaneCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.cpp
	src/BulletCollision/CollisionDispatch/btEmptyCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btGhostObject.cpp
	src/BulletCollision/CollisionDispatch/btManifoldResult.cpp
	src/BulletCollision/CollisionDispatch/btSimulationIslandManager.cpp
	src/BulletCollision/CollisionDispatch/btSphereBoxCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btSphereSphereCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btSphereTriangleCollisionAlgorithm.cpp
	src/BulletCollision/CollisionDispatch/btUnionFind.cpp
	src/BulletCollision/CollisionShapes/btBoxShape.cpp
	src/BulletCollision/CollisionShapes/btBvhTriangleMeshShape.cpp
	src/BulletCollision/CollisionShapes/btCapsuleShape.cpp
	src/BulletCollision/CollisionShapes/btCollisionShape.cpp
	src/BulletCollision/CollisionShapes/btCompoundShape.cpp
	src/BulletCollision/CollisionShapes/btConcaveShape.cpp
	src/BulletCollision/CollisionShapes/btConeShape.cpp
	src/BulletCollision/CollisionShapes/btConvexHullShape.cpp
	src/BulletCollision/CollisionShapes/btConvexInternalShape.cpp
	src/BulletCollision/CollisionShapes/btConvexPointCloudShape.cpp
	src/BulletCollision/CollisionShapes/btConvexShape.cpp
	src/BulletCollision/CollisionShapes/btConvexTriangleMeshShape.cpp
	src/BulletCollision/CollisionShapes/btCylinderShape.cpp
	src/BulletCollision/CollisionShapes/btEmptyShape.cpp
	src/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.cpp
	src/BulletCollision/CollisionShapes/btMinkowskiSumShape.cpp
	src/BulletCollision/CollisionShapes/btMultiSphereShape.cpp
	src/BulletCollision/CollisionShapes/btMultimaterialTriangleMeshShape.cpp
	src/BulletCollision/CollisionShapes/btOptimizedBvh.cpp
	src/BulletCollision/CollisionShapes/btPolyhedralConvexShape.cpp
	src/BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.cpp
	src/BulletCollision/CollisionShapes/btShapeHull.cpp
	src/BulletCollision/CollisionShapes/btSphereShape.cpp
	src/BulletCollision/CollisionShapes/btStaticPlaneShape.cpp
	src/BulletCollision/CollisionShapes/btStridingMeshInterface.cpp
	src/BulletCollision/CollisionShapes/btTetrahedronShape.cpp
	src/BulletCollision/CollisionShapes/btTriangleBuffer.cpp
	src/BulletCollision/CollisionShapes/btTriangleCallback.cpp
	src/BulletCollision/CollisionShapes/btTriangleIndexVertexArray.cpp
	src/BulletCollision/CollisionShapes/btTriangleIndexVertexMaterialArray.cpp
	src/BulletCollision/CollisionShapes/btTriangleMesh.cpp
	src/BulletCollision/CollisionShapes/btTriangleMeshShape.cpp
	src/BulletCollision/CollisionShapes/btUniformScalingShape.cpp
	src/BulletCollision/Gimpact/btContactProcessing.cpp
	src/BulletCollision/Gimpact/btGImpactBvh.cpp
	src/BulletCollision/Gimpact/btGImpactCollisionAlgorithm.cpp
	src/BulletCollision/Gimpact/btGImpactQuantizedBvh.cpp
	src/BulletCollision/Gimpact/btGImpactShape.cpp
	src/BulletCollision/Gimpact/btGenericPoolAllocator.cpp
	src/BulletCollision/Gimpact/btTriangleShapeEx.cpp
	src/BulletCollision/Gimpact/gim_box_set.cpp
	src/BulletCollision/Gimpact/gim_contact.cpp
	src/BulletCollision/Gimpact/gim_memory.cpp
	src/BulletCollision/Gimpact/gim_tri_collision.cpp
	src/BulletCollision/NarrowPhaseCollision/btContinuousConvexCollision.cpp
	src/BulletCollision/NarrowPhaseCollision/btConvexCast.cpp
	src/BulletCollision/NarrowPhaseCollision/btGjkConvexCast.cpp
	src/BulletCollision/NarrowPhaseCollision/btGjkEpa.cpp
	src/BulletCollision/NarrowPhaseCollision/btGjkEpa2.cpp
	src/BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.cpp
	src/BulletCollision/NarrowPhaseCollision/btGjkPairDetector.cpp
	src/BulletCollision/NarrowPhaseCollision/btMinkowskiPenetrationDepthSolver.cpp
	src/BulletCollision/NarrowPhaseCollision/btPersistentManifold.cpp
	src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.cpp
	src/BulletCollision/NarrowPhaseCollision/btSubSimplexConvexCast.cpp
	src/BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.cpp
	src/BulletDynamics/ConstraintSolver/btConeTwistConstraint.cpp
	src/BulletDynamics/ConstraintSolver/btContactConstraint.cpp
	src/BulletDynamics/ConstraintSolver/btGeneric6DofConstraint.cpp
	src/BulletDynamics/ConstraintSolver/btHingeConstraint.cpp
	src/BulletDynamics/ConstraintSolver/btPoint2PointConstraint.cpp
	src/BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.cpp
	src/BulletDynamics/ConstraintSolver/btSliderConstraint.cpp
	src/BulletDynamics/ConstraintSolver/btSolve2LinearConstraint.cpp
	src/BulletDynamics/ConstraintSolver/btTypedConstraint.cpp
	src/BulletDynamics/Dynamics/Bullet-C-API.cpp
	src/BulletDynamics/Dynamics/btContinuousDynamicsWorld.cpp
	src/BulletDynamics/Dynamics/btDiscreteDynamicsWorld.cpp
	src/BulletDynamics/Dynamics/btRigidBody.cpp
	src/BulletDynamics/Dynamics/btSimpleDynamicsWorld.cpp
	src/BulletDynamics/Vehicle/btRaycastVehicle.cpp
	src/BulletDynamics/Vehicle/btWheelInfo.cpp
	src/BulletSoftBody/btSoftBody.cpp
	src/BulletSoftBody/btSoftBodyConcaveCollisionAlgorithm.cpp
	src/BulletSoftBody/btSoftBodyHelpers.cpp
	src/BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.cpp
	src/BulletSoftBody/btSoftRigidCollisionAlgorithm.cpp
	src/BulletSoftBody/btSoftRigidDynamicsWorld.cpp
	src/BulletSoftBody/btSoftSoftCollisionAlgorithm.cpp
	src/LinearMath/btAlignedAllocator.cpp
	src/LinearMath/btConvexHull.cpp
	src/LinearMath/btGeometryUtil.cpp
	src/LinearMath/btQuickprof.cpp
)

BLENDERLIB(extern_bullet "${SRC}" "${INC}")
