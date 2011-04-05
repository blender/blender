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

set(INC
	.
	../../../source/gameengine/Ketsji
	../../../source/gameengine/Expressions
	../../../source/gameengine/GameLogic
	../../../source/gameengine/SceneGraph
	../../../source/gameengine/Rasterizer
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer
	../../../source/gameengine/BlenderRoutines
	../../../source/blender/blenlib
	../../../source/blender/blenkernel
	../../../source/blender/makesdna
	../../../source/blender/editors/include
	../../../source/blender/imbuf
	../../../source/blender/python
	../../../source/blender/python/generic
	../../../source/blender/gpu
	../../../source/kernel/gen_system
	../../../intern/string
	../../../intern/moto/include
	../../../intern/guardedalloc
	${GLEW_INCLUDE_PATH}
)

set(SRC
	Exception.cpp
	FilterBase.cpp
	FilterBlueScreen.cpp
	FilterColor.cpp
	FilterNormal.cpp
	FilterSource.cpp
	ImageBase.cpp
	ImageBuff.cpp
	ImageMix.cpp
	ImageRender.cpp
	ImageViewport.cpp
	PyTypeList.cpp
	Texture.cpp
	VideoBase.cpp
	VideoFFmpeg.cpp
	blendVideoTex.cpp

	BlendType.h
	Common.h
	Exception.h
	FilterBase.h
	FilterBlueScreen.h
	FilterColor.h
	FilterNormal.h
	FilterSource.h
	ImageBase.h
	ImageBuff.h
	ImageMix.h
	ImageRender.h
	ImageViewport.h
	PyTypeList.h
	Texture.h
	VideoBase.h
	VideoFFmpeg.h
)

if(WITH_CODEC_FFMPEG)
	set(INC ${INC} ${FFMPEG_INC} ${PTHREADS_INC})
	add_definitions(-DWITH_FFMPEG)
	add_definitions(-D__STDC_CONSTANT_MACROS)
endif()

blender_add_lib(ge_videotex "${SRC}" "${INC}")
