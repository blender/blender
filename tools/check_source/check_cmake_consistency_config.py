# SPDX-License-Identifier: GPL-2.0-or-later
import os

IGNORE_SOURCE = (
    "/test/",
    "/tests/gtests/",
    "/release/",

    # specific source files
    "extern/audaspace/",

    # Use for `WIN32` only.
    "source/creator/blender_launcher_win32.c",

    # specific source files
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.cpp",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btConvex2dConvex2dAlgorithm.cpp",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btInternalEdgeUtility.cpp",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btBox2dShape.cpp",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btConvex2dShape.cpp",
    "extern/bullet2/src/BulletDynamics/Character/btKinematicCharacterController.cpp",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btHinge2Constraint.cpp",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btUniversalConstraint.cpp",

    "doc/doxygen/doxygen.extern.h",
    "doc/doxygen/doxygen.intern.h",
    "doc/doxygen/doxygen.main.h",
    "doc/doxygen/doxygen.source.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btConvex2dConvex2dAlgorithm.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btInternalEdgeUtility.h",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btBox2dShape.h",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btConvex2dShape.h",
    "extern/bullet2/src/BulletDynamics/Character/btKinematicCharacterController.h",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btHinge2Constraint.h",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btUniversalConstraint.h",

    "build_files/build_environment/patches/config_gmpxx.h",
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
