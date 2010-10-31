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
	../../../intern/audaspace/intern
	../../../source/gameengine/Converter
	../../../source/gameengine/BlenderRoutines
	../../../source/blender/imbuf
	../../../intern/moto/include
	../../../source/gameengine/Ketsji
	../../../source/gameengine/Ketsji/KXNetwork
	../../../source/blender/blenlib
	../../../source/blender/blenkernel
	../../../source/blender/windowmanager
	../../../source/blender
	../../../source/blender/makesdna
	../../../source/blender/makesrna
	../../../source/gameengine/Rasterizer
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer
	../../../source/gameengine/GameLogic
	../../../source/gameengine/Expressions
	../../../source/gameengine/Network
	../../../source/gameengine/SceneGraph
	../../../source/gameengine/Physics/common
	../../../source/gameengine/Physics/Bullet
	../../../source/gameengine/Physics/Dummy
	../../../source/gameengine/Network/LoopBackNetwork
	../../../source/blender/blenloader
	../../../source/blender/gpu
	../../../source/blender/ikplugin
	../../../extern/bullet2/src
)

SET(SRC
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
)

IF(WITH_PYTHON)
	LIST(APPEND INC ${PYTHON_INC})
	ADD_DEFINITIONS(-DWITH_PYTHON)
ENDIF(WITH_PYTHON)

IF(WITH_BULLET)
	ADD_DEFINITIONS(-DUSE_BULLET)
ENDIF(WITH_BULLET)

BLENDERLIB(bf_converter "${SRC}" "${INC}")
