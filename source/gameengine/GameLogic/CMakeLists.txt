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
	../Expressions
	../Rasterizer
	../SceneGraph
	../../blender/blenlib
	../../../intern/container
	../../../intern/string
	../../../intern/ghost
)

set(INC_SYS
	../../../intern/moto/include
)

set(SRC
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
	SCA_PythonJoystick.cpp
	SCA_PythonKeyboard.cpp
	SCA_PythonMouse.cpp
	SCA_RandomActuator.cpp
	SCA_RandomEventManager.cpp
	SCA_RandomNumberGenerator.cpp
	SCA_RandomSensor.cpp
	SCA_TimeEventManager.cpp
	SCA_XNORController.cpp
	SCA_XORController.cpp
	Joystick/SCA_Joystick.cpp
	Joystick/SCA_JoystickEvents.cpp

	SCA_2DFilterActuator.h
	SCA_ANDController.h
	SCA_ActuatorEventManager.h
	SCA_ActuatorSensor.h
	SCA_AlwaysEventManager.h
	SCA_AlwaysSensor.h
	SCA_BasicEventManager.h
	SCA_DelaySensor.h
	SCA_EventManager.h
	SCA_ExpressionController.h
	SCA_IActuator.h
	SCA_IController.h
	SCA_IInputDevice.h
	SCA_ILogicBrick.h
	SCA_IObject.h
	SCA_IScene.h
	SCA_ISensor.h
	SCA_JoystickManager.h
	SCA_JoystickSensor.h
	SCA_KeyboardManager.h
	SCA_KeyboardSensor.h
	SCA_LogicManager.h
	SCA_MouseManager.h
	SCA_MouseSensor.h
	SCA_NANDController.h
	SCA_NORController.h
	SCA_ORController.h
	SCA_PropertyActuator.h
	SCA_PropertyEventManager.h
	SCA_PropertySensor.h
	SCA_PythonController.h
	SCA_PythonJoystick.h
	SCA_PythonKeyboard.h
	SCA_PythonMouse.h
	SCA_RandomActuator.h
	SCA_RandomEventManager.h
	SCA_RandomNumberGenerator.h
	SCA_RandomSensor.h
	SCA_TimeEventManager.h
	SCA_XNORController.h
	SCA_XORController.h
	Joystick/SCA_Joystick.h
	Joystick/SCA_JoystickDefines.h
	Joystick/SCA_JoystickPrivate.h
)

if(WITH_SDL)
	list(APPEND INC_SYS
		${SDL_INCLUDE_DIR}
	)

	add_definitions(-DWITH_SDL)

	if(WITH_GHOST_SDL)
		add_definitions(-DWITH_GHOST_SDL)
	endif()
endif()

blender_add_lib(ge_logic "${SRC}" "${INC}" "${INC_SYS}")
