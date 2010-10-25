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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

SET(INC
	.
	../../../source/kernel/gen_system
	../../../intern/string
	../../../intern/guardedalloc
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer 
	../../../source/gameengine/Converter
	../../../source/blender/imbuf
	../../../intern/moto/include
	../../../source/gameengine/Ketsji 
	../../../source/blender/blenlib
	../../../source/blender/blenkernel
	../../../source/blender/python
	../../../source/blender/python/generic
	../../../source/blender 
	../../../source/blender/makesdna 
	../../../source/gameengine/Rasterizer 
	../../../source/gameengine/GameLogic
	../../../source/gameengine/Expressions 
	../../../source/gameengine/Ketsji/KXNetwork
	../../../source/gameengine/Network
	../../../source/gameengine/SceneGraph 
	../../../source/gameengine/Physics/common
	../../../source/gameengine/Network/LoopBackNetwork
	../../../intern/audaspace/intern
	../../../source/blender/blenloader
	../../../source/blender/gpu
	../../../extern/glew/include
)

SET(SRC
	BL_BlenderShader.cpp
	BL_Material.cpp
	BL_Shader.cpp
	BL_Texture.cpp
	KX_ArmatureSensor.cpp
	KX_BlenderMaterial.cpp
	KX_BulletPhysicsController.cpp
	KX_Camera.cpp
	KX_CameraActuator.cpp
	KX_CameraIpoSGController.cpp
	KX_ConstraintActuator.cpp
	KX_ConstraintWrapper.cpp
	KX_ConvertPhysicsObjects.cpp
	KX_Dome.cpp
	KX_EmptyObject.cpp
	KX_GameActuator.cpp
	KX_GameObject.cpp
	KX_IPO_SGController.cpp
	KX_IPhysicsController.cpp
	KX_IpoActuator.cpp
	KX_KetsjiEngine.cpp
	KX_Light.cpp
	KX_LightIpoSGController.cpp
	KX_MaterialIpoController.cpp
	KX_MeshProxy.cpp
	KX_MotionState.cpp
	KX_MouseFocusSensor.cpp
	KX_NearSensor.cpp
	KX_ObColorIpoSGController.cpp
	KX_ObjectActuator.cpp
	KX_OrientationInterpolator.cpp
	KX_ParentActuator.cpp
	KX_PhysicsObjectWrapper.cpp
	KX_PolyProxy.cpp
	KX_PolygonMaterial.cpp
	KX_PositionInterpolator.cpp
	KX_PyConstraintBinding.cpp
	KX_PyMath.cpp
	KX_PythonInit.cpp
	KX_PythonInitTypes.cpp
	KX_PythonSeq.cpp
	KX_RadarSensor.cpp
	KX_RayCast.cpp
	KX_RayEventManager.cpp
	KX_RaySensor.cpp
	KX_SCA_AddObjectActuator.cpp
	KX_SCA_DynamicActuator.cpp
	KX_SCA_EndObjectActuator.cpp
	KX_SCA_ReplaceMeshActuator.cpp
	KX_SG_BoneParentNodeRelationship.cpp
	KX_SG_NodeRelationships.cpp
	KX_ScalarInterpolator.cpp
	KX_ScalingInterpolator.cpp
	KX_Scene.cpp
	KX_SceneActuator.cpp
	KX_SoundActuator.cpp
	KX_StateActuator.cpp
	KX_TimeCategoryLogger.cpp
	KX_TimeLogger.cpp
	KX_TouchEventManager.cpp
	KX_TouchSensor.cpp
	KX_TrackToActuator.cpp
	KX_VehicleWrapper.cpp
	KX_VertexProxy.cpp
	KX_VisibilityActuator.cpp
	KX_WorldInfo.cpp
	KX_WorldIpoController.cpp
)

ADD_DEFINITIONS(-DGLEW_STATIC)

IF(WITH_SDL)
	SET(INC ${INC} ${SDL_INCLUDE_DIR})  
ELSE(WITH_SDL)
	ADD_DEFINITIONS(-DDISABLE_SDL)
ENDIF(WITH_SDL)

IF(WITH_PYTHON)
	LIST(APPEND INC ${PYTHON_INC})
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

IF(WITH_FFMPEG)
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WITH_BULLET)
	ADD_DEFINITIONS(-DUSE_BULLET)
	LIST(APPEND INC ../../../extern/bullet2/src)
	LIST(APPEND INC ../../../source/gameengine/Physics/Bullet )
ENDIF(WITH_BULLET)

BLENDERLIB(bf_ketsji "${SRC}" "${INC}")
