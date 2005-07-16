# Microsoft Developer Studio Project File - Name="Bullet" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Bullet - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Bullet3.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Bullet3.mak" CFG="Bullet - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Bullet - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Bullet - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Bullet - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /Zd /O2 /I "../LinearMath" /I "." /D "NDEBUG" /D "_LIB" /D "WIN32" /D "_MBCS" /D "BUM_INLINED" /D "USE_ALGEBRAIC" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Bullet - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../LinearMath" /I "." /D "_DEBUG" /D "_LIB" /D "WIN32" /D "_MBCS" /D "BUM_INLINED" /D "USE_ALGEBRAIC" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "Bullet - Win32 Release"
# Name "Bullet - Win32 Debug"
# Begin Group "NarrowPhaseCollision"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_AlgebraicPolynomialSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_AlgebraicPolynomialSolver.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_Collidable.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_Collidable.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_CollisionPair.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_CollisionPair.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_EdgeEdge.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_EdgeEdge.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_MotionStateInterface.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_PolynomialSolverInterface.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_Screwing.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_Screwing.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_StaticMotionState.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_VertexPoly.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\BU_VertexPoly.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\CollisionMargin.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\ContinuousConvexCollision.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\ContinuousConvexCollision.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\ConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\ConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\ConvexPenetrationDepthSolver.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\DiscreteCollisionDetectorInterface.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\GjkConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\GjkConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\GjkPairDetector.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\GjkPairDetector.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\ManifoldPoint.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\MinkowskiPenetrationDepthSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\MinkowskiPenetrationDepthSolver.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\PersistentManifold.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\PersistentManifold.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\PointCollector.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\RaycastCallback.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\RaycastCallback.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\SimplexSolverInterface.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\SubSimplexConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\SubSimplexConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\VoronoiSimplexSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\NarrowPhaseCollision\VoronoiSimplexSolver.h
# End Source File
# End Group
# Begin Group "BroadphaseCollision"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\BroadphaseCollision\BroadPhaseInterface.h
# End Source File
# Begin Source File

SOURCE=.\BroadphaseCollision\BroadphaseProxy.cpp
# End Source File
# Begin Source File

SOURCE=.\BroadphaseCollision\BroadphaseProxy.h
# End Source File
# Begin Source File

SOURCE=.\BroadphaseCollision\CollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BroadphaseCollision\CollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BroadphaseCollision\CollisionDispatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\BroadphaseCollision\CollisionDispatcher.h
# End Source File
# Begin Source File

SOURCE=.\BroadphaseCollision\SimpleBroadphase.cpp
# End Source File
# Begin Source File

SOURCE=.\BroadphaseCollision\SimpleBroadphase.h
# End Source File
# End Group
# Begin Group "CollisionShapes"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\CollisionShapes\BoxShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\BoxShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\CollisionShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\CollisionShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\ConeShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\ConeShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\ConvexHullShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\ConvexHullShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\ConvexShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\ConvexShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\CylinderShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\CylinderShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\MinkowskiSumShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\MinkowskiSumShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\MultiSphereShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\MultiSphereShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\PolyhedralConvexShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\PolyhedralConvexShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\Simplex1to4Shape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\Simplex1to4Shape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\SphereShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\SphereShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\StridingMeshInterface.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\StridingMeshInterface.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\TriangleCallback.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\TriangleMesh.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\TriangleMesh.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\TriangleMeshShape.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\TriangleMeshShape.h
# End Source File
# Begin Source File

SOURCE=.\CollisionShapes\TriangleShape.h
# End Source File
# End Group
# End Target
# End Project
