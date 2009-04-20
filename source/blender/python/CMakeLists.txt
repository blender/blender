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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

FILE(GLOB SRC api2_2x/*.c)
SET(SRC ${SRC}
  BPY_interface.c
  BPY_menus.c
)

SET(INC 
  api2_2x ../blenkernel ../blenlib ../blenloader
  ../render/extern/include ../radiosity/extern/include
  ../makesdna ../../../intern/guardedalloc ../../../intern/bmfont ../imbuf ../include
  ${PYTHON_INC} ../../../extern/glew/include ../gpu
)

IF(WITH_QUICKTIME)
  SET(INC ${INC} ${QUICKTIME_INC})
  ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WITH_OPENEXR)
  ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_OPENEXR)

IF(WITH_FFMPEG)
  ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(BF_BUILDINFO)
  ADD_DEFINITIONS(-DNAN_BUILDINFO)
ENDIF(BF_BUILDINFO)

BLENDERLIB_NOLIST(blender_python "${SRC}" "${INC}")
#env.BlenderLib ( libname='blender_python', sources = Split(sources), includes = Split(incs), defines = defs, libtype=['core','game2'], priority = [60,115] )
