import os

IGNORE = (
    "/test/",
    "/tests/gtests/",
    "/release/",

    # specific source files
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.cpp",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btConvex2dConvex2dAlgorithm.cpp",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btInternalEdgeUtility.cpp",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btBox2dShape.cpp",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btConvex2dShape.cpp",
    "extern/bullet2/src/BulletDynamics/Character/btKinematicCharacterController.cpp",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btHinge2Constraint.cpp",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btUniversalConstraint.cpp",
    "intern/audaspace/SRC/AUD_SRCResampleFactory.cpp",
    "intern/audaspace/SRC/AUD_SRCResampleReader.cpp",

    "extern/bullet2/src/BulletCollision/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btConvex2dConvex2dAlgorithm.h",
    "extern/bullet2/src/BulletCollision/CollisionDispatch/btInternalEdgeUtility.h",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btBox2dShape.h",
    "extern/bullet2/src/BulletCollision/CollisionShapes/btConvex2dShape.h",
    "extern/bullet2/src/BulletDynamics/Character/btKinematicCharacterController.h",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btHinge2Constraint.h",
    "extern/bullet2/src/BulletDynamics/ConstraintSolver/btUniversalConstraint.h",
    "extern/carve/include/carve/config.h",
    "extern/carve/include/carve/random/random.h",
    "extern/carve/patches/files/config.h",
    "extern/carve/patches/files/random.h",
    "intern/audaspace/SRC/AUD_SRCResampleFactory.h",
    "intern/audaspace/SRC/AUD_SRCResampleReader.h",
)

UTF8_CHECK = True

SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))))

# doesn't have to exist, just use as reference
BUILD_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(SOURCE_DIR, "..", "build"))))
