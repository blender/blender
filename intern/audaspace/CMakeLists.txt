# $Id$
# ***** BEGIN LGPL LICENSE BLOCK *****
#
# Copyright 2009 Jörg Hermann Müller
#
# This file is part of AudaSpace.
#
# AudaSpace is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# AudaSpace is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
#
# ***** END LGPL LICENSE BLOCK *****

SET(INC . intern FX SRC ${PTHREADS_INC} ${LIBSAMPLERATE_INC})

FILE(GLOB SRC intern/*.cpp intern/*.h FX/*.cpp SRC/*.cpp)

IF(WITH_FFMPEG)
	SET(INC ${INC} ffmpeg ${FFMPEG_INC})
	FILE(GLOB FFMPEGSRC ffmpeg/*.cpp)
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WITH_SDL)
	SET(INC ${INC} SDL ${SDL_INCLUDE_DIR})
	FILE(GLOB SDLSRC SDL/*.cpp)
	ADD_DEFINITIONS(-DWITH_SDL)
ENDIF(WITH_SDL)

IF(WITH_OPENAL)
	SET(INC ${INC} OpenAL ${OPENAL_INCLUDE_DIR})
	FILE(GLOB OPENALSRC OpenAL/*.cpp)
	ADD_DEFINITIONS(-DWITH_OPENAL)

	STRING(REGEX MATCH ".*ramework.*" FRAMEWORK ${OPENAL_INCLUDE_DIR})
	IF(FRAMEWORK)
		ADD_DEFINITIONS(-DAPPLE_FRAMEWORK_FIX)
	ENDIF(FRAMEWORK)
ENDIF(WITH_OPENAL)

IF(WITH_JACK)
	SET(INC ${INC} jack ${JACK_INC})
	FILE(GLOB JACKSRC jack/*.cpp)
	ADD_DEFINITIONS(-DWITH_JACK)
ENDIF(WITH_JACK)

IF(WITH_SNDFILE)
	SET(INC ${INC} sndfile ${SNDFILE_INC})
	FILE(GLOB SNDFILESRC sndfile/*.cpp)
	ADD_DEFINITIONS(-DWITH_SNDFILE)
ENDIF(WITH_SNDFILE)

SET(SRC ${SRC} ${FFMPEGSRC} ${SNDFILESRC} ${SDLSRC} ${OPENALSRC} ${JACKSRC})

BLENDERLIB(bf_audaspace "${SRC}" "${INC}")
