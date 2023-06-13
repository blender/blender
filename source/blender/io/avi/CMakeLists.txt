# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../blenlib
  ../../imbuf
  ../../makesdna
  ../../../../intern/guardedalloc
)

set(INC_SYS
  ${JPEG_INCLUDE_DIR}
)

set(SRC
  intern/avi.c
  intern/avi_codecs.c
  intern/avi_endian.c
  intern/avi_mjpeg.c
  intern/avi_options.c
  intern/avi_rgb.c
  intern/avi_rgb32.c

  AVI_avi.h
  intern/avi_endian.h
  intern/avi_intern.h
  intern/avi_mjpeg.h
  intern/avi_rgb.h
  intern/avi_rgb32.h
)

set(LIB
  ${JPEG_LIBRARIES}
)

blender_add_lib(bf_avi "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
