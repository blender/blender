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
	extern/include
	intern/include
	../blenfont
	../blenkernel
	../blenlib
	../blenloader
	../imbuf
	../makesdna
	../makesrna
	../../../intern/guardedalloc
	../../../intern/mikktspace
	../../../intern/smoke/extern
)

set(INC_SYS

)

set(SRC
	intern/raytrace/rayobject.cpp
	intern/raytrace/rayobject_empty.cpp
	intern/raytrace/rayobject_octree.cpp
	intern/raytrace/rayobject_raycounter.cpp
	intern/raytrace/rayobject_svbvh.cpp
	intern/raytrace/rayobject_blibvh.cpp
	intern/raytrace/rayobject_instance.cpp
	intern/raytrace/rayobject_qbvh.cpp
	intern/raytrace/rayobject_rtbuild.cpp
	intern/raytrace/rayobject_vbvh.cpp
	intern/source/convertblender.c
	intern/source/envmap.c
	intern/source/external_engine.c
	intern/source/gammaCorrectionTables.c
	intern/source/imagetexture.c
	intern/source/initrender.c
	intern/source/occlusion.c
	intern/source/pipeline.c
	intern/source/pixelblending.c
	intern/source/pixelshading.c
	intern/source/pointdensity.c
	intern/source/rayshade.c
	intern/source/rendercore.c
	intern/source/render_result.c
	intern/source/render_texture.c
	intern/source/renderdatabase.c
	intern/source/shadbuf.c
	intern/source/shadeinput.c
	intern/source/shadeoutput.c
	intern/source/sss.c
	intern/source/strand.c
	intern/source/sunsky.c
	intern/source/texture_ocean.c
	intern/source/volume_precache.c
	intern/source/volumetric.c
	intern/source/voxeldata.c
	intern/source/zbuf.c

	extern/include/RE_engine.h
	extern/include/RE_pipeline.h
	extern/include/RE_render_ext.h
	extern/include/RE_shader_ext.h
	intern/include/envmap.h
	intern/include/gammaCorrectionTables.h
	intern/include/initrender.h
	intern/include/occlusion.h
	intern/include/pixelblending.h
	intern/include/pixelshading.h
	intern/include/pointdensity.h
	intern/include/raycounter.h
	intern/include/rayobject.h
	intern/include/rayintersection.h
	intern/include/raycounter.h
	intern/include/render_types.h
	intern/include/render_result.h
	intern/include/rendercore.h
	intern/include/renderdatabase.h
	intern/include/renderpipeline.h
	intern/include/shadbuf.h
	intern/include/shading.h
	intern/include/sss.h
	intern/include/strand.h
	intern/include/sunsky.h
	intern/include/texture.h
	intern/include/texture_ocean.h
	intern/include/volume_precache.h
	intern/include/volumetric.h
	intern/include/voxeldata.h
	intern/include/zbuf.h
	intern/raytrace/bvh.h
	intern/raytrace/rayobject_hint.h
	intern/raytrace/rayobject_internal.h
	intern/raytrace/rayobject_rtbuild.h
	intern/raytrace/reorganize.h
	intern/raytrace/svbvh.h
	intern/raytrace/vbvh.h
)

if(WITH_PYTHON)
	add_definitions(-DWITH_PYTHON)
	list(APPEND INC
		../python
	)

	list(APPEND INC_SYS
		${PYTHON_INCLUDE_DIRS}
	)
endif()

if(WITH_IMAGE_OPENEXR)
	add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

if(WITH_CODEC_QUICKTIME)
	list(APPEND INC
		../quicktime
	)
	list(APPEND INC_SYS
		${QUICKTIME_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_QUICKTIME)
endif()

if(WITH_GAMEENGINE)
	add_definitions(-DWITH_GAMEENGINE)
endif()

if(APPLE)
	if(CMAKE_OSX_ARCHITECTURES MATCHES "i386" OR CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mfpmath=sse")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpmath=sse")
	endif()
endif()

blender_add_lib_nolist(bf_render "${SRC}" "${INC}" "${INC_SYS}")
