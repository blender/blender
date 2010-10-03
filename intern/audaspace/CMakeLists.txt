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

SET(INC
	.
	intern
	FX SRC
	${PTHREADS_INC}
	${LIBSAMPLERATE_INC}
)

FILE(GLOB SRC
	intern/*.cpp
	intern/*.h
	FX/*.cpp
	SRC/*.cpp
)

IF(WITH_FFMPEG)
	LIST(APPEND INC ffmpeg ${FFMPEG_INC})
	FILE(GLOB FFMPEGSRC ffmpeg/*.cpp)
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WITH_SDL)
	LIST(APPEND INC SDL ${SDL_INCLUDE_DIR})
	FILE(GLOB SDLSRC SDL/*.cpp)
	ADD_DEFINITIONS(-DWITH_SDL)
ENDIF(WITH_SDL)

IF(WITH_OPENAL)
	LIST(APPEND INC OpenAL ${OPENAL_INCLUDE_DIR})
	FILE(GLOB OPENALSRC OpenAL/*.cpp)
	ADD_DEFINITIONS(-DWITH_OPENAL)
ENDIF(WITH_OPENAL)

IF(WITH_JACK)
	LIST(APPEND INC jack ${JACK_INC})
	FILE(GLOB JACKSRC jack/*.cpp)
	ADD_DEFINITIONS(-DWITH_JACK)
ENDIF(WITH_JACK)

IF(WITH_SNDFILE)
	LIST(APPEND INC sndfile ${SNDFILE_INC})
	FILE(GLOB SNDFILESRC sndfile/*.cpp)
	ADD_DEFINITIONS(-DWITH_SNDFILE)
ENDIF(WITH_SNDFILE)

#IF(WITH_FFTW3)
#	LIST(APPEND INC fftw ${FFTW3_INC})
#	FILE(GLOB FFTW3SRC fftw/*.cpp)
#	ADD_DEFINITIONS(-DWITH_FFTW3)
#ENDIF(WITH_FFTW3)

IF(WITH_PYTHON)
	LIST(APPEND INC Python ${PYTHON_INC})
	FILE(GLOB PYTHONSRC Python/*.cpp)
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

SET(SRC ${SRC} ${FFMPEGSRC} ${SNDFILESRC} ${FFTW3SRC} ${SDLSRC} ${OPENALSRC} ${JACKSRC} ${PYTHONSRC})

BLENDERLIB(bf_intern_audaspace "${SRC}" "${INC}")
