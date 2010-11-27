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

SET(SRC
	FX/AUD_AccumulatorFactory.cpp
	FX/AUD_BaseIIRFilterReader.cpp
	FX/AUD_ButterworthFactory.cpp
	FX/AUD_CallbackIIRFilterReader.cpp
	FX/AUD_DelayFactory.cpp
	FX/AUD_DelayReader.cpp
	FX/AUD_DoubleFactory.cpp
	FX/AUD_DoubleReader.cpp
	FX/AUD_EffectFactory.cpp
	FX/AUD_EffectReader.cpp
	FX/AUD_EnvelopeFactory.cpp
	FX/AUD_FaderFactory.cpp
	FX/AUD_FaderReader.cpp
	FX/AUD_HighpassFactory.cpp
	FX/AUD_IIRFilterFactory.cpp
	FX/AUD_IIRFilterReader.cpp
	FX/AUD_LimiterFactory.cpp
	FX/AUD_LimiterReader.cpp
	FX/AUD_LoopFactory.cpp
	FX/AUD_LoopReader.cpp
	FX/AUD_LowpassFactory.cpp
	FX/AUD_PingPongFactory.cpp
	FX/AUD_PitchFactory.cpp
	FX/AUD_PitchReader.cpp
	FX/AUD_RectifyFactory.cpp
	FX/AUD_ReverseFactory.cpp
	FX/AUD_ReverseReader.cpp
	FX/AUD_SquareFactory.cpp
	FX/AUD_SumFactory.cpp
	FX/AUD_SuperposeFactory.cpp
	FX/AUD_SuperposeReader.cpp
	FX/AUD_VolumeFactory.cpp
	intern/AUD_3DMath.h
	intern/AUD_Buffer.cpp
	intern/AUD_Buffer.h
	intern/AUD_BufferReader.cpp
	intern/AUD_BufferReader.h
	intern/AUD_C-API.cpp
	intern/AUD_C-API.h
	intern/AUD_ChannelMapperFactory.cpp
	intern/AUD_ChannelMapperFactory.h
	intern/AUD_ChannelMapperReader.cpp
	intern/AUD_ChannelMapperReader.h
	intern/AUD_ConverterFactory.cpp
	intern/AUD_ConverterFactory.h
	intern/AUD_ConverterFunctions.cpp
	intern/AUD_ConverterFunctions.h
	intern/AUD_ConverterReader.cpp
	intern/AUD_ConverterReader.h
	intern/AUD_DefaultMixer.cpp
	intern/AUD_DefaultMixer.h
	intern/AUD_FileFactory.cpp
	intern/AUD_FileFactory.h
	intern/AUD_I3DDevice.h
	intern/AUD_IDevice.h
	intern/AUD_IFactory.h
	intern/AUD_IReader.h
	intern/AUD_LinearResampleFactory.cpp
	intern/AUD_LinearResampleFactory.h
	intern/AUD_LinearResampleReader.cpp
	intern/AUD_LinearResampleReader.h
	intern/AUD_Mixer.cpp
	intern/AUD_Mixer.h
	intern/AUD_MixerFactory.cpp
	intern/AUD_MixerFactory.h
	intern/AUD_NULLDevice.cpp
	intern/AUD_NULLDevice.h
	intern/AUD_PyInit.h
	intern/AUD_ReadDevice.cpp
	intern/AUD_ReadDevice.h
	intern/AUD_Reference.h
	intern/AUD_ResampleFactory.h
	intern/AUD_SequencerFactory.cpp
	intern/AUD_SequencerFactory.h
	intern/AUD_SequencerReader.cpp
	intern/AUD_SequencerReader.h
	intern/AUD_SilenceFactory.cpp
	intern/AUD_SilenceFactory.h
	intern/AUD_SilenceReader.cpp
	intern/AUD_SilenceReader.h
	intern/AUD_SinusFactory.cpp
	intern/AUD_SinusFactory.h
	intern/AUD_SinusReader.cpp
	intern/AUD_SinusReader.h
	intern/AUD_SoftwareDevice.cpp
	intern/AUD_SoftwareDevice.h
	intern/AUD_Space.h
	intern/AUD_StreamBufferFactory.cpp
	intern/AUD_StreamBufferFactory.h
)

IF(WITH_FFMPEG)
	ADD_DEFINITIONS(-DWITH_FFMPEG)
	LIST(APPEND INC ffmpeg ${FFMPEG_INC})
	SET(FFMPEGSRC
		ffmpeg/AUD_FFMPEGFactory.cpp
		ffmpeg/AUD_FFMPEGReader.cpp
	)
ENDIF(WITH_FFMPEG)

IF(WITH_SDL)
	ADD_DEFINITIONS(-DWITH_SDL)
	LIST(APPEND INC SDL ${SDL_INCLUDE_DIR})
	SET(SDLSRC
		SDL/AUD_SDLDevice.cpp
	)
ENDIF(WITH_SDL)

IF(WITH_OPENAL)
	ADD_DEFINITIONS(-DWITH_OPENAL)
	LIST(APPEND INC OpenAL ${OPENAL_INCLUDE_DIR})
	SET(OPENALSRC
		OpenAL/AUD_OpenALDevice.cpp
	)	
ENDIF(WITH_OPENAL)

IF(WITH_JACK)
	ADD_DEFINITIONS(-DWITH_JACK)
	LIST(APPEND INC jack ${JACK_INC})
	SET(JACKSRC
		jack/AUD_JackDevice.cpp
	)	
ENDIF(WITH_JACK)

IF(WITH_SNDFILE)
	ADD_DEFINITIONS(-DWITH_SNDFILE)
	LIST(APPEND INC sndfile ${SNDFILE_INC})
	SET(SNDFILESRC
		sndfile/AUD_SndFileFactory.cpp
		sndfile/AUD_SndFileReader.cpp
	)
ENDIF(WITH_SNDFILE)

IF(WITH_SAMPLERATE)
	ADD_DEFINITIONS(-DWITH_SAMPLERATE)
	SET(SRCFILESRC
		SRC/AUD_SRCResampleFactory.cpp
		SRC/AUD_SRCResampleReader.cpp
	)
ENDIF(WITH_SAMPLERATE)

#IF(WITH_FFTW3)
#	ADD_DEFINITIONS(-DWITH_FFTW3)
#	LIST(APPEND INC fftw ${FFTW3_INC})
#	SET(FFTW3SRC
#		fftw/AUD_BandPassFactory.cpp
#		fftw/AUD_BandPassReader.cpp
#	)
#ENDIF(WITH_FFTW3)

IF(WITH_PYTHON)
	LIST(APPEND INC Python ${PYTHON_INC})
	SET(PYTHONSRC
		Python/AUD_PyAPI.cpp
	)
	ADD_DEFINITIONS(-DWITH_PYTHON)
ENDIF(WITH_PYTHON)

SET(SRC ${SRC} ${FFMPEGSRC} ${SNDFILESRC} ${SRCFILESRC} ${FFTW3SRC} ${SDLSRC} ${OPENALSRC} ${JACKSRC} ${PYTHONSRC})

BLENDERLIB(bf_intern_audaspace "${SRC}" "${INC}")
