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

set(INC
	.
	intern
	FX
	SRC
	../ffmpeg
)

set(INC_SYS
	${PTHREADS_INC}
)

set(SRC
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

	FX/AUD_AccumulatorFactory.h
	FX/AUD_BaseIIRFilterReader.h
	FX/AUD_ButterworthFactory.h
	FX/AUD_CallbackIIRFilterReader.h
	FX/AUD_DelayFactory.h
	FX/AUD_DelayReader.h
	FX/AUD_DoubleFactory.h
	FX/AUD_DoubleReader.h
	FX/AUD_EffectFactory.h
	FX/AUD_EffectReader.h
	FX/AUD_EnvelopeFactory.h
	FX/AUD_FaderFactory.h
	FX/AUD_FaderReader.h
	FX/AUD_HighpassFactory.h
	FX/AUD_IIRFilterFactory.h
	FX/AUD_IIRFilterReader.h
	FX/AUD_LimiterFactory.h
	FX/AUD_LimiterReader.h
	FX/AUD_LoopFactory.h
	FX/AUD_LoopReader.h
	FX/AUD_LowpassFactory.h
	FX/AUD_PingPongFactory.h
	FX/AUD_PitchFactory.h
	FX/AUD_PitchReader.h
	FX/AUD_RectifyFactory.h
	FX/AUD_ReverseFactory.h
	FX/AUD_ReverseReader.h
	FX/AUD_SquareFactory.h
	FX/AUD_SumFactory.h
	FX/AUD_SuperposeFactory.h
	FX/AUD_SuperposeReader.h
	FX/AUD_VolumeFactory.h
)

if(WITH_CODEC_FFMPEG)
	add_definitions(-DWITH_FFMPEG)
	list(APPEND INC ffmpeg)
	list(APPEND INC_SYS ${FFMPEG_INC})
	list(APPEND SRC
		ffmpeg/AUD_FFMPEGFactory.cpp
		ffmpeg/AUD_FFMPEGReader.cpp

		ffmpeg/AUD_FFMPEGFactory.h
		ffmpeg/AUD_FFMPEGReader.h
	)
endif()

if(WITH_SDL)
	add_definitions(-DWITH_SDL)
	list(APPEND INC SDL)
	list(APPEND INC_SYS ${SDL_INCLUDE_DIR})
	list(APPEND SRC
		SDL/AUD_SDLDevice.cpp

		SDL/AUD_SDLDevice.h
	)
endif()

if(WITH_OPENAL)
	add_definitions(-DWITH_OPENAL)
	list(APPEND INC OpenAL)
	list(APPEND INC_SYS ${OPENAL_INCLUDE_DIR})
	list(APPEND SRC
		OpenAL/AUD_OpenALDevice.cpp

		OpenAL/AUD_OpenALDevice.h
	)
endif()

if(WITH_JACK)
	add_definitions(-DWITH_JACK)
	list(APPEND INC jack)
	list(APPEND INC_SYS ${JACK_INC})
	list(APPEND SRC
		jack/AUD_JackDevice.cpp

		jack/AUD_JackDevice.h
	)	
endif()

if(WITH_CODEC_SNDFILE)
	add_definitions(-DWITH_SNDFILE)
	list(APPEND INC sndfile)
	list(APPEND INC_SYS ${SNDFILE_INC})
	set(SNDFILESRC
		sndfile/AUD_SndFileFactory.cpp
		sndfile/AUD_SndFileReader.cpp

		sndfile/AUD_SndFileFactory.h
		sndfile/AUD_SndFileReader.h
	)
endif()

if(WITH_SAMPLERATE)
	add_definitions(-DWITH_SAMPLERATE)
	list(APPEND INC_SYS ${LIBSAMPLERATE_INC})
	list(APPEND SRC
		SRC/AUD_SRCResampleFactory.cpp
		SRC/AUD_SRCResampleReader.cpp

		SRC/AUD_SRCResampleFactory.h
		SRC/AUD_SRCResampleReader.h
	)
endif()

if(WITH_FFTW3 AND FALSE)
	add_definitions(-DWITH_FFTW3)
	list(APPEND INC fftw)
	list(APPEND INC_SYS ${FFTW3_INC})
	list(APPEND SRC
		fftw/AUD_BandPassFactory.cpp
		fftw/AUD_BandPassReader.cpp

		fftw/AUD_BandPassFactory.h
		fftw/AUD_BandPassReader.h
	)
endif()

if(WITH_PYTHON)
	list(APPEND INC Python)
	list(APPEND INC_SYS ${PYTHON_INCLUDE_DIRS})
	list(APPEND SRC
		Python/AUD_PyAPI.cpp

		Python/AUD_PyAPI.h
	)
	add_definitions(-DWITH_PYTHON)
endif()

blender_add_lib(bf_intern_audaspace "${SRC}" "${INC}" "${INC_SYS}")
