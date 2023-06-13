# SPDX-License-Identifier: GPL-2.0-or-later
import os

IGNORE_SOURCE = (
    "/scripts/",
    "/test/",
    "/tests/gtests/",

    # Specific source files.
    "extern/audaspace/",
    "extern/quadriflow/3rd/",
    "extern/sdlew/include/",
    "extern/mantaflow/",
    "extern/Eigen3/",

    # Use for `WIN32` only.
    "source/creator/blender_launcher_win32.c",

    # Pre-computed headers.
    "source/blender/compositor/COM_precomp.h",
    "source/blender/freestyle/FRS_precomp.h",

    # Specific source files.
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.cpp",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btConvex2dConvex2dAlgorithm.cpp",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btInternalEdgeUtility.cpp",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btBox2dShape.cpp",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btConvex2dShape.cpp",
    "extern/bullet2/src/BulletDynamics/Character/btKinematicCharacterController.cpp",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btHinge2Constraint.cpp",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btUniversalConstraint.cpp",

    # Specific source files.
    "extern/bullet2/src/BulletCollision/BroadphaseCollision/btAxisSweep3Internal.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btConvex2dConvex2dAlgorithm.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btInternalEdgeUtility.h",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btBox2dShape.h",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btConvex2dShape.h",
    "extern/bullet2/src/BulletCollision/Gimpact/btContactProcessingStructs.h",
    "extern/bullet2/src/BulletCollision/Gimpact/btGImpactBvhStructs.h",
    "extern/bullet2/src/BulletCollision/Gimpact/btGImpactQuantizedBvhStructs.h",
    "extern/bullet2/src/BulletCollision/Gimpact/gim_pair.h",
    "extern/bullet2/src/BulletDynamics/Character/btKinematicCharacterController.h",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btBatchedConstraints.h",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btHinge2Constraint.h",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btUniversalConstraint.h",
    "extern/bullet2/src/BulletDynamics/Dynamics/btDiscreteDynamicsWorldMt.h",
    "extern/bullet2/src/BulletDynamics/Dynamics/btSimulationIslandManagerMt.h",
    "extern/bullet2/src/BulletDynamics/Featherstone/btMultiBodyFixedConstraint.h",
    "extern/bullet2/src/BulletDynamics/Featherstone/btMultiBodyGearConstraint.h",
    "extern/bullet2/src/BulletDynamics/Featherstone/btMultiBodyInplaceSolverIslandCallback.h",
    "extern/bullet2/src/BulletDynamics/Featherstone/btMultiBodyMLCPConstraintSolver.h",
    "extern/bullet2/src/BulletDynamics/Featherstone/btMultiBodySliderConstraint.h",
    "extern/bullet2/src/BulletDynamics/Featherstone/btMultiBodySphericalJointMotor.h",
    "extern/bullet2/src/BulletSoftBody/DeformableBodyInplaceSolverIslandCallback.h",
    "extern/bullet2/src/BulletSoftBody/btCGProjection.h",
    "extern/bullet2/src/BulletSoftBody/btConjugateGradient.h",
    "extern/bullet2/src/BulletSoftBody/btConjugateResidual.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableBackwardEulerObjective.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableBodySolver.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableContactConstraint.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableContactProjection.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableCorotatedForce.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableGravityForce.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableLagrangianForce.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableLinearElasticityForce.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableMassSpringForce.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableMousePickingForce.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableMultiBodyConstraintSolver.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableMultiBodyDynamicsWorld.h",
    "extern/bullet2/src/BulletSoftBody/btDeformableNeoHookeanForce.h",
    "extern/bullet2/src/BulletSoftBody/btKrylovSolver.h",
    "extern/bullet2/src/BulletSoftBody/btPreconditioner.h",
    "extern/bullet2/src/BulletSoftBody/btSoftMultiBodyDynamicsWorld.h",
    "extern/bullet2/src/BulletSoftBody/poly34.h",
    "extern/bullet2/src/LinearMath/TaskScheduler/btThreadSupportInterface.h",
    "extern/bullet2/src/LinearMath/btImplicitQRSVD.h",
    "extern/bullet2/src/LinearMath/btModifiedGramSchmidt.h",
    "extern/bullet2/src/LinearMath/btReducedVector.h",
    "extern/bullet2/src/LinearMath/btThreads.h",

    "doc/doxygen/doxygen.extern.h",
    "doc/doxygen/doxygen.intern.h",
    "doc/doxygen/doxygen.main.h",
    "doc/doxygen/doxygen.source.h",

    "build_files/build_environment/patches/config_gmpxx.h",

    # These could be included but are not part of Blender's core.
    "intern/libmv/libmv/multiview/test_data_sets.h",
)

# Ignore cmake file, path pairs.
IGNORE_SOURCE_MISSING = (
    (   # Use for `WITH_NANOVDB`.
        "intern/cycles/kernel/CMakeLists.txt", (
            "nanovdb/util/CSampleFromVoxels.h",
            "nanovdb/util/SampleFromVoxels.h",
            "nanovdb/NanoVDB.h",
            "nanovdb/CNanoVDB.h",
        ),
    ),
)

IGNORE_CMAKE = (
    "extern/audaspace/CMakeLists.txt",
)

UTF8_CHECK = True

SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

# doesn't have to exist, just use as reference
BUILD_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(SOURCE_DIR, "..", "build"))))
