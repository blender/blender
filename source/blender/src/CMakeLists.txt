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

FILE(GLOB SRC *.c)

SET(INC 
  ../../../intern/guardedalloc ../../../intern/memutil
  ../blenlib ../makesdna ../blenkernel
  ../include ../../../intern/bmfont ../imbuf ../render/extern/include
  ../../../intern/bsp/extern ../radiosity/extern/include
  ../../../intern/decimation/extern ../blenloader ../python
  ../../kernel/gen_system ../../../intern/SoundSystem ../readstreamglue
  ../quicktime ../../../intern/elbeem/extern
  ../../../intern/ghost ../../../intern/opennl/extern
  ../nodes ../../../extern/glew/include ../gpu
  ${PYTHON_INC}
  ${SDL_INC}
)

IF(WITH_INTERNATIONAL)
  SET(INC ${INC} ../ftfont)
  ADD_DEFINITIONS(-DINTERNATIONAL)
  ADD_DEFINITIONS(-DFTGL_STATIC_LIBRARY)
ENDIF(WITH_INTERNATIONAL)

IF(WITH_OPENEXR)
  ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_OPENEXR)

IF(WITH_QUICKTIME)
  SET(INC ${INC} ${QUICKTIME_INC})
  ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WITH_FFMPEG)
  SET(INC ${INC} ${FFMPEG_INC})
  ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WIN32)
  SET(INC ${INC} ${PTHREADS_INC})
ENDIF(WIN32)

IF(WITH_VERSE)
  SET(INC ${INC} ${VERSE_INC})
  ADD_DEFINITIONS(-DWITH_VERSE)
ENDIF(WITH_VERSE)

# TODO buildinfo
IF(BF_BUILDINFO)
  ADD_DEFINITIONS(-DNAN_BUILDINFO)
ENDIF(BF_BUILDINFO)

BLENDERLIB_NOLIST(src "${SRC}" "${INC}")

IF(WITH_VERSE)
  ADD_DEPENDENCIES(src mkprot verse)
ENDIF(WITH_VERSE)

