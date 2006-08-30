# Microsoft Developer Studio Project File - Name="BulletDynamics" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BulletDynamics - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BLI_BulletDynamics.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BLI_BulletDynamics.mak" CFG="BulletDynamics - Win32 Debug"
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
MTL=midl.exe
LINK32=link.exe -lib
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
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../LinearMath" /I "../Bullet" /I "../BulletDynamics" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
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

SOURCE=.\ConstraintSolver\Generic6DofConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Generic6DofConstraint.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\HingeConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\HingeConstraint.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\JacobianEntry.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Point2PointConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Point2PointConstraint.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\SequentialImpulseConstraintSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\SequentialImpulseConstraintSolver.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Solve2LinearConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\Solve2LinearConstraint.h
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\TypedConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\ConstraintSolver\TypedConstraint.h
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
# Begin Group "Vehicle"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Vehicle\RaycastVehicle.cpp
# End Source File
# Begin Source File

SOURCE=.\Vehicle\RaycastVehicle.h
# End Source File
# Begin Source File

SOURCE=.\Vehicle\VehicleRaycaster.h
# End Source File
# Begin Source File

SOURCE=.\Vehicle\WheelInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\Vehicle\WheelInfo.h
# End Source File
# End Group
# Begin Group "Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_DynamicTypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_IMotionState.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_IMotionState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_IPhysicsController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_IPhysicsController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_IPhysicsEnvironment.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_IPhysicsEnvironment.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_IVehicle.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_IVehicle.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\common\PHY_Pro.h
# End Source File
# End Group
# End Target
# End Project
