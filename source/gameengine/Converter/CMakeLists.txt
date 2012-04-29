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

set(INC
	.
	../BlenderRoutines
	../Expressions
	../GameLogic
	../Ketsji
	../Ketsji/KXNetwork
	../Network
	../Network/LoopBackNetwork
	../Physics/Bullet
	../Physics/Dummy
	../Physics/common
	../Rasterizer
	../Rasterizer/RAS_OpenGLRasterizer
	../Rasterizer/RAS_OpenGLRasterizer
	../SceneGraph
	../../blender
	../../blender/blenkernel
	../../blender/blenlib
	../../blender/blenloader
	../../blender/gpu
	../../blender/ikplugin
	../../blender/imbuf
	../../blender/makesdna
	../../blender/makesrna
	../../blender/windowmanager
	../../../extern/bullet2/src
	../../../extern/Eigen3
	../../../intern/container
	../../../intern/guardedalloc
	../../../intern/moto/include
	../../../intern/string
	../../../extern/recastnavigation/Detour/Include
)

set(INC_SYS
	${PTHREADS_INCLUDE_DIRS}

)

set(SRC
	BL_ActionActuator.cpp
	BL_ArmatureActuator.cpp
	BL_ArmatureChannel.cpp
	BL_ArmatureConstraint.cpp
	BL_ArmatureObject.cpp
	BL_BlenderDataConversion.cpp
	BL_DeformableGameObject.cpp
	BL_MeshDeformer.cpp
	BL_ModifierDeformer.cpp
	BL_ShapeActionActuator.cpp
	BL_ShapeDeformer.cpp
	BL_SkinDeformer.cpp
	BlenderWorldInfo.cpp
	KX_BlenderScalarInterpolator.cpp
	KX_BlenderSceneConverter.cpp
	KX_ConvertActuators.cpp
	KX_ConvertControllers.cpp
	KX_ConvertProperties.cpp
	KX_ConvertSensors.cpp
	KX_IpoConvert.cpp
	KX_SoftBodyDeformer.cpp

	BL_ActionActuator.h
	BL_ArmatureActuator.h
	BL_ArmatureChannel.h
	BL_ArmatureConstraint.h
	BL_ArmatureObject.h
	BL_BlenderDataConversion.h
	BL_DeformableGameObject.h
	BL_MeshDeformer.h
	BL_ModifierDeformer.h
	BL_ShapeActionActuator.h
	BL_ShapeDeformer.h
	BL_SkinDeformer.h
	BlenderWorldInfo.h
	KX_BlenderScalarInterpolator.h
	KX_BlenderSceneConverter.h
	KX_ConvertActuators.h
	KX_ConvertControllers.h
	KX_ConvertProperties.h
	KX_ConvertSensors.h
	KX_IpoConvert.h
	KX_SoftBodyDeformer.h
)

if(WITH_BULLET)
	add_definitions(-DUSE_BULLET)
endif()

if(WITH_AUDASPACE)
	list(APPEND INC
		../../../intern/audaspace/intern
	)
	add_definitions(-DWITH_AUDASPACE)
endif()

blender_add_lib(ge_converter "${SRC}" "${INC}" "${INC_SYS}")
