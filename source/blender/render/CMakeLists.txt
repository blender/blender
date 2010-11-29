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

# remove warning until render branch merged.
REMOVE_STRICT_FLAGS()

SET(INC 
	intern/include
	extern/include
	../blenlib
	../blenloader
	../makesdna
	../makesrna
	../blenkernel
	../imbuf
	../../kernel/gen_messaging
	../../../intern/smoke/extern
	../../../intern/guardedalloc
)

SET(SRC
	intern/raytrace/rayobject.cpp
	intern/raytrace/rayobject_qbvh.cpp
	intern/raytrace/rayobject_rtbuild.cpp
	intern/raytrace/rayobject_svbvh.cpp
	intern/raytrace/rayobject_vbvh.cpp
	intern/source/convertblender.c
	intern/source/envmap.c
	intern/source/gammaCorrectionTables.c
	intern/source/imagetexture.c
	intern/source/initrender.c
	intern/source/occlusion.c
	intern/source/pipeline.c
	intern/source/pixelblending.c
	intern/source/pixelshading.c
	intern/source/pointdensity.c
	intern/source/rayobject_blibvh.c
	intern/source/rayobject_instance.c
	intern/source/rayobject_octree.c
	intern/source/rayobject_raycounter.c
	intern/source/rayshade.c
	intern/source/rendercore.c
	intern/source/renderdatabase.c
	intern/source/shadbuf.c
	intern/source/shadeinput.c
	intern/source/shadeoutput.c
	intern/source/sss.c
	intern/source/strand.c
	intern/source/sunsky.c
	intern/source/texture.c
	intern/source/volume_precache.c
	intern/source/volumetric.c
	intern/source/voxeldata.c
	intern/source/zbuf.c

	extern/include/RE_pipeline.h
	extern/include/RE_raytrace.h
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
	intern/include/render_types.h
	intern/include/rendercore.h
	intern/include/renderdatabase.h
	intern/include/renderpipeline.h
	intern/include/shadbuf.h
	intern/include/shading.h
	intern/include/sss.h
	intern/include/strand.h
	intern/include/sunsky.h
	intern/include/texture.h
	intern/include/volume_precache.h
	intern/include/volumetric.h
	intern/include/voxeldata.h
	intern/include/zbuf.h
	intern/raytrace/bvh.h
	intern/raytrace/rayobject_hint.h
	intern/raytrace/rayobject_rtbuild.h
	intern/raytrace/reorganize.h
	intern/raytrace/svbvh.h
	intern/raytrace/vbvh.h
)

IF(WITH_IMAGE_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_IMAGE_OPENEXR)

IF(WITH_QUICKTIME)
	LIST(APPEND INC ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(APPLE)
	IF(CMAKE_OSX_ARCHITECTURES MATCHES "i386" OR CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
		SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mfpmath=sse")
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpmath=sse")
	ENDIF(CMAKE_OSX_ARCHITECTURES MATCHES "i386" OR CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
ENDIF(APPLE)

BLENDERLIB_NOLIST(bf_render "${SRC}" "${INC}")
