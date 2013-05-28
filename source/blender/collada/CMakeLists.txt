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

remove_strict_flags()

set(INC
	.
	../blenkernel
	../blenlib
	../blenfont
	../editors/include
	../makesdna
	../makesrna
	../windowmanager
	../imbuf
	../../../intern/guardedalloc
	../ikplugin
	../../../intern/iksolver/extern
	../bmesh
)

set(INC_SYS
	${OPENCOLLADA_INCLUDE_DIRS}
)

set(SRC
	AnimationImporter.cpp
	AnimationExporter.cpp
	ArmatureExporter.cpp
	ArmatureImporter.cpp
	CameraExporter.cpp
	ControllerExporter.cpp
	DocumentExporter.cpp
	DocumentImporter.cpp
	EffectExporter.cpp
	ErrorHandler.cpp
	ExportSettings.cpp
	ImportSettings.cpp
	ExtraHandler.cpp
	ExtraTags.cpp
	GeometryExporter.cpp
	ImageExporter.cpp
	InstanceWriter.cpp
	LightExporter.cpp
	MaterialExporter.cpp
	MeshImporter.cpp
	SkinInfo.cpp
	SceneExporter.cpp
	TransformReader.cpp
	TransformWriter.cpp
	collada.cpp
	collada_internal.cpp
	collada_utils.cpp

	AnimationImporter.h
	AnimationExporter.h
	ArmatureExporter.h
	ArmatureImporter.h
	CameraExporter.h
	ControllerExporter.h
	DocumentExporter.h
	DocumentImporter.h
	EffectExporter.h
	ErrorHandler.h
	ExportSettings.h
	ImportSettings.h
	ExtraHandler.h
	ExtraTags.h
	GeometryExporter.h
	ImageExporter.h
	InstanceWriter.h
	LightExporter.h
	MaterialExporter.h
	MeshImporter.h
	SkinInfo.h
	SceneExporter.h
	TransformReader.h
	TransformWriter.h
	collada.h
	collada_internal.h
	collada_utils.h
)

if(WITH_BUILDINFO)
	add_definitions(-DWITH_BUILDINFO)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
	# COLLADAFWArray.h gives error with gcc 4.5
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
endif()

blender_add_lib(bf_collada "${SRC}" "${INC}" "${INC_SYS}")
