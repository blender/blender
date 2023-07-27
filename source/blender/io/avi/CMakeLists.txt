# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../imbuf
)

set(INC_SYS
  ${JPEG_INCLUDE_DIR}
)

set(SRC
  intern/avi.cc
  intern/avi_codecs.cc
  intern/avi_endian.cc
  intern/avi_mjpeg.cc
  intern/avi_options.cc
  intern/avi_rgb.cc
  intern/avi_rgb32.cc

  AVI_avi.h
  intern/avi_endian.h
  intern/avi_intern.h
  intern/avi_mjpeg.h
  intern/avi_rgb.h
  intern/avi_rgb32.h
)

set(LIB
  ${JPEG_LIBRARIES}
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_avi "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
