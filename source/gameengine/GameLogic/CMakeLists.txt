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
	../../../source/gameengine/Expressions 
	../../../source/gameengine/SceneGraph
	../../../intern/moto/include
	../../../source/gameengine/Rasterizer
)

SET(SRC
	Joystick/SCA_Joystick.cpp
	Joystick/SCA_JoystickEvents.cpp
	SCA_2DFilterActuator.cpp
	SCA_ANDController.cpp
	SCA_ActuatorEventManager.cpp
	SCA_ActuatorSensor.cpp
	SCA_AlwaysEventManager.cpp
	SCA_AlwaysSensor.cpp
	SCA_BasicEventManager.cpp
	SCA_DelaySensor.cpp
	SCA_EventManager.cpp
	SCA_ExpressionController.cpp
	SCA_IActuator.cpp
	SCA_IController.cpp
	SCA_IInputDevice.cpp
	SCA_ILogicBrick.cpp
	SCA_IObject.cpp
	SCA_IScene.cpp
	SCA_ISensor.cpp
	SCA_JoystickManager.cpp
	SCA_JoystickSensor.cpp
	SCA_KeyboardManager.cpp
	SCA_KeyboardSensor.cpp
	SCA_LogicManager.cpp
	SCA_MouseManager.cpp
	SCA_MouseSensor.cpp
	SCA_NANDController.cpp
	SCA_NORController.cpp
	SCA_ORController.cpp
	SCA_PropertyActuator.cpp
	SCA_PropertyEventManager.cpp
	SCA_PropertySensor.cpp
	SCA_PythonController.cpp
	SCA_PythonKeyboard.cpp
	SCA_PythonMouse.cpp
	SCA_RandomActuator.cpp
	SCA_RandomEventManager.cpp
	SCA_RandomNumberGenerator.cpp
	SCA_RandomSensor.cpp
	SCA_TimeEventManager.cpp
	SCA_XNORController.cpp
	SCA_XORController.cpp
)

IF(WITH_SDL)
	SET(INC ${INC} ${SDL_INCLUDE_DIR})
ELSE(WITH_SDL)
	ADD_DEFINITIONS(-DDISABLE_SDL)
ENDIF(WITH_SDL)

IF(WITH_PYTHON)
	LIST(APPEND INC ${PYTHON_INC})
	ADD_DEFINITIONS(-DWITH_PYTHON)
ENDIF(WITH_PYTHON)

BLENDERLIB(ge_logic "${SRC}" "${INC}")
