# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

set(INC
  .
  ../blenkernel
  ../blenlib
  ../blenloader
  ../gpu
  ../makesdna
  ../makesrna
  ../sequencer
  ../../../intern/guardedalloc
  ../../../intern/memutil
)

set(INC_SYS
  ${JPEG_INCLUDE_DIR}
  ${PNG_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  intern/allocimbuf.c
  intern/anim_movie.c
  intern/bmp.c
  intern/cache.c
  intern/colormanagement.c
  intern/colormanagement_inline.c
  intern/divers.c
  intern/filetype.c
  intern/filter.c
  intern/imageprocess.c
  intern/indexer.c
  intern/iris.c
  intern/jpeg.c
  intern/metadata.c
  intern/module.c
  intern/moviecache.cc
  intern/png.c
  intern/readimage.c
  intern/rectop.c
  intern/rotate.c
  intern/scaling.c
  intern/stereoimbuf.c
  intern/targa.c
  intern/thumbs.c
  intern/thumbs_blend.c
  intern/thumbs_font.c
  intern/transform.cc
  intern/util.c
  intern/util_gpu.c
  intern/writeimage.c

  IMB_colormanagement.h
  IMB_imbuf.h
  IMB_imbuf_types.h
  IMB_metadata.h
  IMB_moviecache.h
  IMB_thumbs.h
  intern/IMB_allocimbuf.h
  intern/IMB_anim.h
  intern/IMB_colormanagement_intern.h
  intern/IMB_filetype.h
  intern/IMB_filter.h
  intern/IMB_indexer.h
  intern/imbuf.h

  # orphan include
  ../../../intern/ffmpeg/ffmpeg_compat.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  bf_blenloader
  bf_intern_guardedalloc
  bf_intern_memutil
  bf_intern_opencolorio

  ${PNG_LIBRARIES}
  ${JPEG_LIBRARIES}
)

if(WITH_IMAGE_OPENEXR)
  list(APPEND LIB
    bf_imbuf_openexr
  )
  add_definitions(-DWITH_OPENEXR)
else()
  list(APPEND SRC
    intern/openexr/openexr_stub.cpp
  )
endif()

if(WITH_IMAGE_TIFF)
  list(APPEND INC_SYS
    ${TIFF_INCLUDE_DIR}
  )
  list(APPEND SRC
    intern/tiff.c
  )
  list(APPEND LIB
    ${TIFF_LIBRARY}
  )
  add_definitions(-DWITH_TIFF)
endif()


if(WITH_OPENIMAGEIO)
  list(APPEND LIB
    bf_imbuf_openimageio
  )
  add_definitions(-DWITH_OPENIMAGEIO)
endif()

if(WITH_IMAGE_OPENJPEG)
  list(APPEND INC_SYS
    ${OPENJPEG_INCLUDE_DIRS}
  )
  list(APPEND SRC
    intern/jp2.c
  )
  list(APPEND LIB
    ${OPENJPEG_LIBRARIES}
  )

  add_definitions(-DWITH_OPENJPEG ${OPENJPEG_DEFINES})
endif()

if(WITH_CODEC_AVI)
  list(APPEND INC
    ../io/avi
  )
  list(APPEND LIB
    bf_avi
  )
  add_definitions(-DWITH_AVI)
endif()

if(WITH_CODEC_FFMPEG)
  list(APPEND INC
    ../../../intern/ffmpeg
  )
  list(APPEND INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFMPEG_LIBRARIES}
    ${OPENJPEG_LIBRARIES}
  )
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_IMAGE_DDS)
  list(APPEND LIB
    bf_imbuf_dds
  )
  add_definitions(-DWITH_DDS)
endif()

if(WITH_IMAGE_CINEON)
  list(APPEND LIB
    bf_imbuf_cineon
  )
  add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_HDR)
  list(APPEND SRC
    intern/radiance_hdr.c
  )
  add_definitions(-DWITH_HDR)
endif()

list(APPEND INC
  ../../../intern/opencolorio
)

if(WIN32)
  list(APPEND INC
    ../../../intern/utfconv
  )
endif()

# no need to compile object files for inline headers.
set_source_files_properties(
  intern/colormanagement_inline.c
  PROPERTIES HEADER_FILE_ONLY TRUE
)

blender_add_lib(bf_imbuf "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
