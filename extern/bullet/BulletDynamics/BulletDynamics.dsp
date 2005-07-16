# Microsoft Developer Studio Project File - Name="BulletDynamics" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BulletDynamics - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BulletDynamics.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BulletDynamics.mak" CFG="BulletDynamics - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BulletDynamics - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BulletDynamics - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BulletDynamics - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "BulletDynamics___Win32_Release"
# PROP BASE Intermediate_Dir "BulletDynamics___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "BulletDynamics___Win32_Release"
# PROP Intermediate_Dir "BulletDynamics___Win32_Release"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../LinearMath" /I "../Bullet" /I "../BulletDynamics" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BulletDynamics - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BulletDynamics___Win32_Debug"
# PROP BASE Intermediate_Dir "BulletDynamics___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "BulletDynamics___Win32_Debug"
# PROP Intermediate_Dir "BulletDynamics___Win32_Debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ  /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../LinearMath" /I "../Bullet" /I "../BulletDynamics" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ  /c
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BulletDynamics - Win32 Release"
# Name "BulletDynamics - Win32 Debug"
# Begin Group "ConstraintSolver"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ConstraintSolver\ConstraintSolver.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\ContactConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\ContactConstraint.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\ContactSolverInfo.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\JacobianEntry.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\OdeConstraintSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\OdeConstraintSolver.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Point2PointConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Point2PointConstraint.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\SimpleConstraintSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\SimpleConstraintSolver.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Solve2LinearConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Solve2LinearConstraint.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\SorLcp.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\SorLcp.h
# End Source File
# End Group
# Begin Group "CollisionDispatch"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\CollisionDispatch\ConvexConcaveCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\ConvexConcaveCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\ConvexConvexAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\ConvexConvexAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\EmptyCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\EmptyCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\ManifoldResult.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\ManifoldResult.h
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\ToiContactDispatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\ToiContactDispatcher.h
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\UnionFind.cpp
# End Source File
# Begin Source File

SOURCE=.\CollisionDispatch\UnionFind.h
# End Source File
# End Group
# Begin Group "Dynamics"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Dynamics\BU_Joint.cpp
# End Source File
# Begin Source File

SOURCE=.\Dynamics\BU_Joint.h
# End Source File
# Begin Source File

SOURCE=.\Dynamics\ContactJoint.cpp
# End Source File
# Begin Source File

SOURCE=.\Dynamics\ContactJoint.h
# End Source File
# Begin Source File

SOURCE=.\Dynamics\MassProps.h
# End Source File
# Begin Source File

SOURCE=.\Dynamics\RigidBody.cpp
# End Source File
# Begin Source File

SOURCE=.\Dynamics\RigidBody.h
# End Source File
# End Group
# End Target
# End Project
