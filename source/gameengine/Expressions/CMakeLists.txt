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
	../SceneGraph
	../../blender/blenlib
	../../../intern/guardedalloc
	../../../intern/string
)

set(INC_SYS
	../../../intern/moto/include
)

set(SRC
	intern/BoolValue.cpp
	intern/ConstExpr.cpp
	intern/EmptyValue.cpp
	intern/ErrorValue.cpp
	intern/Expression.cpp
	intern/FloatValue.cpp
	intern/IdentifierExpr.cpp
	intern/IfExpr.cpp
	intern/InputParser.cpp
	intern/IntValue.cpp
	intern/HashedPtr.cpp
	intern/ListValue.cpp
	intern/Operator1Expr.cpp
	intern/Operator2Expr.cpp
	intern/PyObjectPlus.cpp
	intern/StringValue.cpp
	intern/Value.cpp
	intern/VectorValue.cpp
	intern/ListWrapper.cpp

	EXP_BoolValue.h
	EXP_ConstExpr.h
	EXP_EmptyValue.h
	EXP_ErrorValue.h
	EXP_Expression.h
	EXP_FloatValue.h
	EXP_HashedPtr.h
	EXP_IdentifierExpr.h
	EXP_IfExpr.h
	EXP_InputParser.h
	EXP_IntValue.h
	EXP_ListValue.h
	EXP_Operator1Expr.h
	EXP_Operator2Expr.h
	EXP_PyObjectPlus.h
	EXP_Python.h
	EXP_StringValue.h
	EXP_Value.h
	EXP_VectorValue.h
	EXP_VoidValue.h
	EXP_ListWrapper.h

)

if(WITH_PYTHON)
	list(APPEND SRC
		intern/PythonCallBack.cpp

		EXP_PythonCallBack.h
	)
endif()

blender_add_lib(ge_logic_expressions "${SRC}" "${INC}" "${INC_SYS}")
