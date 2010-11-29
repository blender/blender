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
	../avi
	../blenlib
	../blenkernel
	../makesdna
	../../../intern/memutil
	../../../intern/guardedalloc
	${JPEG_INC}
	${PNG_INC}
	${ZLIB_INC}
)

SET(SRC
	intern/allocimbuf.c
	intern/anim_movie.c
	intern/bmp.c
	intern/cache.c
	intern/divers.c
	intern/filetype.c
	intern/filter.c
	intern/imageprocess.c
	intern/iris.c
	intern/jp2.c
	intern/jpeg.c
	intern/md5.c
	intern/metadata.c
	intern/module.c
	intern/png.c
	intern/radiance_hdr.c
	intern/readimage.c
	intern/rectop.c
	intern/rotate.c
	intern/scaling.c
	intern/targa.c
	intern/thumbs.c
	intern/thumbs_blend.c
	intern/tiff.c
	intern/util.c
	intern/writeimage.c

	IMB_imbuf.h
	IMB_imbuf_types.h
	IMB_thumbs.h
	intern/IMB_allocimbuf.h
	intern/IMB_anim.h
	intern/IMB_filetype.h
	intern/IMB_filter.h
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
	intern/md5.h
	intern/openexr/openexr_api.h
	intern/openexr/openexr_multi.h
)

IF(WITH_IMAGE_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_IMAGE_OPENEXR)

IF(WITH_IMAGE_TIFF)
	SET(INC ${INC} ${TIFF_INCLUDE_DIR})
	ADD_DEFINITIONS(-DWITH_TIFF)
ENDIF(WITH_IMAGE_TIFF)

IF(WITH_IMAGE_OPENJPEG)
	SET(INC ${INC} ${OPENJPEG_INC})
	ADD_DEFINITIONS(-DWITH_OPENJPEG)
ENDIF(WITH_IMAGE_OPENJPEG)

IF(WITH_QUICKTIME)
	SET(INC ${INC} ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WITH_FFMPEG)
	SET(INC ${INC} ${FFMPEG_INC})
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WITH_IMAGE_DDS)
	ADD_DEFINITIONS(-DWITH_DDS)
ENDIF(WITH_IMAGE_DDS)

IF(WITH_IMAGE_CINEON)
	ADD_DEFINITIONS(-DWITH_CINEON)
ENDIF(WITH_IMAGE_CINEON)

IF(WITH_IMAGE_HDR)
	ADD_DEFINITIONS(-DWITH_HDR)
ENDIF(WITH_IMAGE_HDR)

BLENDERLIB(bf_imbuf "${SRC}" "${INC}")
