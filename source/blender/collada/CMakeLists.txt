# $Id: CMakeLists.txt 21789 2009-07-22 05:35:12Z kazanbas $
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

REMOVE_STRICT_FLAGS()

SET(INC
	.
	../blenlib
	../blenkernel
	../windowmanager
	../makesdna
	../makesrna
	../editors/include
	../../../intern/guardedalloc
)

IF(APPLE)
	LIST(APPEND INC
		${OPENCOLLADA_INC}/COLLADAStreamWriter
		${OPENCOLLADA_INC}/COLLADABaseUtils
		${OPENCOLLADA_INC}/COLLADAFramework
		${OPENCOLLADA_INC}/COLLADASaxFrameworkLoader 
	)
ELSE(APPLE)
	LIST(APPEND INC
		${OPENCOLLADA_INC}/COLLADAStreamWriter/include
		${OPENCOLLADA_INC}/COLLADABaseUtils/include
		${OPENCOLLADA_INC}/COLLADAFramework/include
		${OPENCOLLADA_INC}/COLLADASaxFrameworkLoader/include 
	)
ENDIF(APPLE)

SET(SRC
	AnimationImporter.cpp
	ArmatureExporter.cpp
	ArmatureImporter.cpp
	CameraExporter.cpp
	DocumentExporter.cpp
	DocumentImporter.cpp
	EffectExporter.cpp
	GeometryExporter.cpp
	ImageExporter.cpp
	InstanceWriter.cpp
	LightExporter.cpp
	MaterialExporter.cpp
	MeshImporter.cpp
	SkinInfo.cpp
	TransformReader.cpp
	TransformWriter.cpp
	collada.cpp
	collada_internal.cpp
	collada_utils.cpp

	AnimationImporter.h
	ArmatureExporter.h
	ArmatureImporter.h
	CameraExporter.h
	DocumentExporter.h
	DocumentImporter.h
	EffectExporter.h
	GeometryExporter.h
	ImageExporter.h
	InstanceWriter.h
	LightExporter.h
	MaterialExporter.h
	MeshImporter.h
	SkinInfo.h
	TransformReader.h
	TransformWriter.h
	collada.h
	collada_internal.h
	collada_utils.h
)

IF(WITH_BUILDINFO)
	ADD_DEFINITIONS(-DNAN_BUILDINFO)
ENDIF(WITH_BUILDINFO)

BLENDERLIB(bf_collada "${SRC}" "${INC}")
