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
	../avi
	../blenkernel
	../blenlib
	../blenloader
	../makesdna
	../../../intern/guardedalloc
	../../../intern/memutil
)

set(INC_SYS
	${JPEG_INCLUDE_DIR}
	${PNG_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/allocimbuf.c
	intern/anim_movie.c
	intern/bmp.c
	intern/cache.c
	intern/divers.c
	intern/filetype.c
	intern/filter.c
	intern/imageprocess.c
	intern/indexer.c
	intern/indexer_dv.c
	intern/iris.c
	intern/jpeg.c
	intern/metadata.c
	intern/module.c
	intern/moviecache.c
	intern/png.c
	intern/readimage.c
	intern/rectop.c
	intern/rotate.c
	intern/scaling.c
	intern/targa.c
	intern/thumbs.c
	intern/thumbs_blend.c
	intern/util.c
	intern/writeimage.c

	IMB_imbuf.h
	IMB_imbuf_types.h
	IMB_moviecache.h
	IMB_thumbs.h
	intern/IMB_allocimbuf.h
	intern/IMB_anim.h
	intern/IMB_filetype.h
	intern/IMB_filter.h
	intern/IMB_indexer.h
	intern/IMB_metadata.h
	intern/cineon/cin_debug_stuff.h
	intern/cineon/cineonfile.h
	intern/cineon/cineonlib.h
	intern/cineon/dpxfile.h
	intern/cineon/dpxlib.h
	intern/cineon/logImageCore.h
	intern/cineon/logImageLib.h
	intern/cineon/logmemfile.h
	intern/dds/BlockDXT.h
	intern/dds/Color.h
	intern/dds/ColorBlock.h
	intern/dds/Common.h
	intern/dds/DirectDrawSurface.h
	intern/dds/Image.h
	intern/dds/PixelFormat.h
	intern/dds/Stream.h
	intern/dds/dds_api.h
	intern/imbuf.h
	intern/openexr/openexr_api.h
	intern/openexr/openexr_multi.h
	
	# orphan include
	../../../intern/ffmpeg/ffmpeg_compat.h
)

if(WITH_IMAGE_OPENEXR)
	add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_TIFF)
	list(APPEND INC_SYS
		${TIFF_INCLUDE_DIR}
	)
	list(APPEND SRC
		intern/tiff.c
	)

	add_definitions(-DWITH_TIFF)
endif()

if(WITH_IMAGE_OPENJPEG)
	list(APPEND INC_SYS
		${OPENJPEG_INCLUDE_DIRS}
	)
	list(APPEND SRC
		intern/jp2.c
	)

	add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_REDCODE)
	list(APPEND INC_SYS
		${REDCODE_INC}
	)
	add_definitions(-DWITH_REDCODE)
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

if(WITH_CODEC_FFMPEG)
	list(APPEND INC
		../../../intern/ffmpeg
	)
	list(APPEND INC_SYS
		${FFMPEG_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_FFMPEG)

	remove_strict_flags_file(
		intern/indexer.c
		intern/util.c
		intern/anim_movie.c
	)
endif()

if(WITH_IMAGE_DDS)
	add_definitions(-DWITH_DDS)
endif()

if(WITH_IMAGE_CINEON)
	add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_HDR)
	list(APPEND SRC
		intern/radiance_hdr.c
	)
	add_definitions(-DWITH_HDR)
endif()

blender_add_lib(bf_imbuf "${SRC}" "${INC}" "${INC_SYS}")
