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
	BoolValue.cpp
	ConstExpr.cpp
	EXP_C-Api.cpp
	EmptyValue.cpp
	ErrorValue.cpp
	Expression.cpp
	FloatValue.cpp
	IdentifierExpr.cpp
	IfExpr.cpp
	InputParser.cpp
	IntValue.cpp
	KX_HashedPtr.cpp
	ListValue.cpp
	Operator1Expr.cpp
	Operator2Expr.cpp
	PyObjectPlus.cpp
	StringValue.cpp
	Value.cpp
	VectorValue.cpp

	BoolValue.h
	ConstExpr.h
	EXP_C-Api.h
	EmptyValue.h
	ErrorValue.h
	Expression.h
	FloatValue.h
	IdentifierExpr.h
	IfExpr.h
	InputParser.h
	IntValue.h
	KX_HashedPtr.h
	KX_Python.h
	ListValue.h
	Operator1Expr.h
	Operator2Expr.h
	PyObjectPlus.h
	StringValue.h
	Value.h
	VectorValue.h
	VoidValue.h
)

blender_add_lib(ge_logic_expressions "${SRC}" "${INC}" "${INC_SYS}")
