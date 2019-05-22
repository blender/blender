################################################################################
# Copyright 2009-2016 Jörg Müller
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

cmake_minimum_required(VERSION 3.0)
include(CMakeDependentOption)

if(POLICY CMP0054)
	cmake_policy(SET CMP0054 NEW)
endif()

project(audaspace)

set(AUDASPACE_VERSION 1.3)
set(AUDASPACE_LONG_VERSION ${AUDASPACE_VERSION}.0)

if(DEFINED AUDASPACE_CMAKE_CFG)
	include(${AUDASPACE_CMAKE_CFG})
endif()

if(NOT DEFINED AUDASPACE_STANDALONE)
	set(AUDASPACE_STANDALONE TRUE)
endif()

# sources

set(SRC
	src/devices/DefaultSynchronizer.cpp
	src/devices/DeviceManager.cpp
	src/devices/NULLDevice.cpp
	src/devices/ReadDevice.cpp
	src/devices/SoftwareDevice.cpp
	src/Exception.cpp
	src/file/File.cpp
	src/file/FileManager.cpp
	src/file/FileWriter.cpp
	src/fx/Accumulator.cpp
	src/fx/ADSR.cpp
	src/fx/ADSRReader.cpp
	src/fx/BaseIIRFilterReader.cpp
	src/fx/ButterworthCalculator.cpp
	src/fx/Butterworth.cpp
	src/fx/CallbackIIRFilterReader.cpp
	src/fx/Delay.cpp
	src/fx/DelayReader.cpp
	src/fx/DynamicIIRFilter.cpp
	src/fx/DynamicIIRFilterReader.cpp
	src/fx/DynamicMusic.cpp
	src/fx/Effect.cpp
	src/fx/EffectReader.cpp
	src/fx/Envelope.cpp
	src/fx/Fader.cpp
	src/fx/FaderReader.cpp
	src/fx/HighpassCalculator.cpp
	src/fx/Highpass.cpp
	src/fx/IIRFilter.cpp
	src/fx/IIRFilterReader.cpp
	src/fx/Limiter.cpp
	src/fx/LimiterReader.cpp
	src/fx/Loop.cpp
	src/fx/LoopReader.cpp
	src/fx/LowpassCalculator.cpp
	src/fx/Lowpass.cpp
	src/fx/Modulator.cpp
	src/fx/ModulatorReader.cpp
	src/fx/MutableReader.cpp
	src/fx/MutableSound.cpp
	src/fx/Pitch.cpp
	src/fx/PitchReader.cpp
	src/fx/PlaybackManager.cpp
	src/fx/PlaybackCategory.cpp
	src/fx/Reverse.cpp
	src/fx/ReverseReader.cpp
	src/fx/SoundList.cpp
	src/fx/Source.cpp
	src/fx/Sum.cpp
	src/fx/Threshold.cpp
	src/fx/Volume.cpp
	src/fx/VolumeReader.cpp
	src/fx/VolumeSound.cpp
	src/fx/VolumeStorage.cpp
	src/generator/Sawtooth.cpp
	src/generator/SawtoothReader.cpp
	src/generator/Silence.cpp
	src/generator/SilenceReader.cpp
	src/generator/Sine.cpp
	src/generator/SineReader.cpp
	src/generator/Square.cpp
	src/generator/SquareReader.cpp
	src/generator/Triangle.cpp
	src/generator/TriangleReader.cpp
	src/respec/ChannelMapper.cpp
	src/respec/ChannelMapperReader.cpp
	src/respec/Converter.cpp
	src/respec/ConverterFunctions.cpp
	src/respec/ConverterReader.cpp
	src/respec/JOSResample.cpp
	src/respec/JOSResampleReaderCoeff.cpp
	src/respec/JOSResampleReader.cpp
	src/respec/LinearResample.cpp
	src/respec/LinearResampleReader.cpp
	src/respec/Mixer.cpp
	src/respec/ResampleReader.cpp
	src/respec/SpecsChanger.cpp
	src/sequence/AnimateableProperty.cpp
	src/sequence/Double.cpp
	src/sequence/DoubleReader.cpp
	src/sequence/PingPong.cpp
	src/sequence/Sequence.cpp
	src/sequence/SequenceData.cpp
	src/sequence/SequenceEntry.cpp
	src/sequence/SequenceHandle.cpp
	src/sequence/SequenceReader.cpp
	src/sequence/Superpose.cpp
	src/sequence/SuperposeReader.cpp
	src/util/Barrier.cpp
	src/util/Buffer.cpp
	src/util/BufferReader.cpp
	src/util/StreamBuffer.cpp
	src/util/ThreadPool.cpp
)

set(PRIVATE_HDR
	src/sequence/SequenceHandle.h
)

set(PUBLIC_HDR
	include/devices/DefaultSynchronizer.h
	include/devices/DeviceManager.h
	include/devices/I3DDevice.h
	include/devices/I3DHandle.h
	include/devices/IDeviceFactory.h
	include/devices/IDevice.h
	include/devices/IHandle.h
	include/devices/ISynchronizer.h
	include/devices/NULLDevice.h
	include/devices/ReadDevice.h
	include/devices/SoftwareDevice.h
	include/Exception.h
	include/file/File.h
	include/file/FileManager.h
	include/file/FileWriter.h
	include/file/IFileInput.h
	include/file/IFileOutput.h
	include/file/IWriter.h
	include/fx/Accumulator.h
	include/fx/ADSR.h
	include/fx/ADSRReader.h
	include/fx/BaseIIRFilterReader.h
	include/fx/ButterworthCalculator.h
	include/fx/Butterworth.h
	include/fx/CallbackIIRFilterReader.h
	include/fx/Delay.h
	include/fx/DelayReader.h
	include/fx/DynamicIIRFilter.h
	include/fx/DynamicIIRFilterReader.h
	include/fx/DynamicMusic.h
	include/fx/Effect.h
	include/fx/EffectReader.h
	include/fx/Envelope.h
	include/fx/Fader.h
	include/fx/FaderReader.h
	include/fx/HighpassCalculator.h
	include/fx/Highpass.h
	include/fx/IDynamicIIRFilterCalculator.h
	include/fx/IIRFilter.h
	include/fx/IIRFilterReader.h
	include/fx/Limiter.h
	include/fx/LimiterReader.h
	include/fx/Loop.h
	include/fx/LoopReader.h
	include/fx/LowpassCalculator.h
	include/fx/Lowpass.h
	include/fx/Modulator.h
	include/fx/ModulatorReader.h
	include/fx/MutableReader.h
	include/fx/MutableSound.h
	include/fx/Pitch.h
	include/fx/PitchReader.h
	include/fx/PlaybackManager.h
	include/fx/PlaybackCategory.h
	include/fx/Reverse.h
	include/fx/ReverseReader.h
	include/fx/SoundList.h
	include/fx/Source.h
	include/fx/Sum.h
	include/fx/Threshold.h
	include/fx/Volume.h
	include/fx/VolumeReader.h
	include/fx/VolumeSound.h
	include/fx/VolumeStorage.h
	include/generator/Sawtooth.h
	include/generator/SawtoothReader.h
	include/generator/Silence.h
	include/generator/SilenceReader.h
	include/generator/Sine.h
	include/generator/SineReader.h
	include/generator/Square.h
	include/generator/SquareReader.h
	include/generator/Triangle.h
	include/generator/TriangleReader.h
	include/IReader.h
	include/ISound.h
	include/plugin/PluginManager.h
	include/respec/ChannelMapper.h
	include/respec/ChannelMapperReader.h
	include/respec/ConverterFunctions.h
	include/respec/Converter.h
	include/respec/ConverterReader.h
	include/respec/JOSResample.h
	include/respec/JOSResampleReader.h
	include/respec/LinearResample.h
	include/respec/LinearResampleReader.h
	include/respec/Mixer.h
	include/respec/ResampleReader.h
	include/respec/Specification.h
	include/respec/SpecsChanger.h
	include/sequence/AnimateableProperty.h
	include/sequence/Double.h
	include/sequence/DoubleReader.h
	include/sequence/PingPong.h
	include/sequence/SequenceData.h
	include/sequence/SequenceEntry.h
	include/sequence/Sequence.h
	include/sequence/SequenceReader.h
	include/sequence/Superpose.h
	include/sequence/SuperposeReader.h
	include/util/Barrier.h
	include/util/Buffer.h
	include/util/BufferReader.h
	include/util/ILockable.h
	include/util/Math3D.h
	include/util/StreamBuffer.h
	include/util/ThreadPool.h
)

set(HDR ${PRIVATE_HDR} ${PUBLIC_HDR})

set(INCLUDE ${CMAKE_CURRENT_BINARY_DIR} include)

if(WIN32)
	set(LIBRARIES)
	if(AUDASPACE_STANDALONE)
		set(DLLS)
		set(LIBRARY_PATH "../lib" CACHE PATH "Path which contains the libraries.")
		file(GLOB LIBRARY_DIRS ${LIBRARY_PATH}/*)
		list(APPEND CMAKE_PREFIX_PATH ${LIBRARY_DIRS})
	endif()
else()
	set(LIBRARIES ${CMAKE_DL_LIBS} -lpthread)
endif()

set(STATIC_PLUGINS "")

# dependencies

if(AUDASPACE_STANDALONE)
	set(PACKAGE_OPTION QUIET)
	list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

	option(BUILD_DEMOS "Build and install demos" TRUE)

	option(SHARED_LIBRARY "Build Shared Library" TRUE)

	option(WITH_C "Build C Module" TRUE)
	option(WITH_DOCS "Build C++ HTML Documentation with Doxygen" TRUE)
	option(WITH_FFMPEG "Build With FFMPEG" TRUE)
	option(WITH_FFTW "Build With FFTW" TRUE)
	option(WITH_JACK "Build With Plugin" TRUE)
	option(WITH_LIBSNDFILE "Build With LibSndFile" TRUE)
	option(WITH_OPENAL "Build With OpenAL" TRUE)
	option(WITH_PYTHON "Build With Python Library" TRUE)
	option(WITH_SDL "Build With SDL" TRUE)
	option(WITH_STRICT_DEPENDENCIES "Error and abort instead of warning if a library is not found." FALSE)

	if(WITH_STRICT_DEPENDENCIES)
		set(PACKAGE_OPTION REQUIRED)
	endif()
endif()

if(AUDASPACE_STANDALONE)
	if(WIN32)
		set(DEFAULT_PLUGIN_PATH "." CACHE STRING "Default plugin installation and loading path.")
		set(DOCUMENTATION_INSTALL_PATH "doc" CACHE PATH "Path where the documentation is installed.")
	else()
		set(DEFAULT_PLUGIN_PATH "${CMAKE_INSTALL_PREFIX}/share/audaspace/plugins" CACHE STRING "Default plugin installation and loading path.")
		set(DOCUMENTATION_INSTALL_PATH "share/doc/audaspace" CACHE PATH "Path where the documentation is installed.")
	endif()
endif()

if(AUDASPACE_STANDALONE)
	cmake_dependent_option(SEPARATE_C "Build C Binding as separate library" TRUE "WITH_C" FALSE)
	cmake_dependent_option(PLUGIN_FFMPEG "Build FFMPEG Plugin" TRUE "WITH_FFMPEG;SHARED_LIBRARY" FALSE)
	cmake_dependent_option(PLUGIN_JACK "Build JACK Plugin" TRUE "WITH_JACK;SHARED_LIBRARY" FALSE)
	cmake_dependent_option(PLUGIN_LIBSNDFILE "Build LibSndFile Plugin" TRUE "WITH_LIBSNDFILE;SHARED_LIBRARY" FALSE)
	cmake_dependent_option(PLUGIN_OPENAL "Build OpenAL Plugin" TRUE "WITH_OPENAL;SHARED_LIBRARY" FALSE)
	cmake_dependent_option(PLUGIN_SDL "Build SDL Plugin" TRUE "WITH_SDL;SHARED_LIBRARY" FALSE)
	cmake_dependent_option(WITH_PYTHON_MODULE "Build Python Module" TRUE "WITH_PYTHON" FALSE)
	cmake_dependent_option(USE_SDL2 "Use SDL2 instead of 1 if available" TRUE "WITH_SDL" FALSE)
	cmake_dependent_option(DYNLOAD_JACK "Dynamically load JACK" FALSE "WITH_JACK" FALSE)
	cmake_dependent_option(WITH_BINDING_DOCS "Build C/Python HTML Documentation with Sphinx" TRUE "WITH_PYTHON_MODULE" FALSE)
endif()

# compiler options

if(AUDASPACE_STANDALONE)
	if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		add_definitions(-std=c++11)
		list(APPEND CMAKE_C_COMPILER_FLAGS "-fvisibility=hidden")
		list(APPEND CMAKE_CXX_COMPILER_FLAGS "-fvisibility=hidden")
	endif()

	if(MSVC)
		list(APPEND CMAKE_C_FLAGS_DEBUG "/Zi /Od")
		list(APPEND CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")
		list(APPEND CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG")
		list(APPEND CMAKE_STATIC_LINKER_FLAGS_DEBUG "/DEBUG")
		list(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG")

		if(SHARED_LIBRARY)
			include(GenerateExportHeader)
		endif()
	endif()

	if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "" FORCE)
	endif()
endif()

if(MSVC)
	add_definitions(
		/D_USE_MATH_DEFINES
		/EHsc
		/DNOMINMAX
		/D_STDINT_H
	)
endif()

# platform specific options

if(MSYS OR MINGW)
	add_definitions(-D_USE_MATH_DEFINES)
endif()

# C
if(WITH_C)
	set(C_SRC
		bindings/C/AUD_ThreadPool.cpp
		bindings/C/AUD_Source.cpp
		bindings/C/AUD_Device.cpp
		bindings/C/AUD_DynamicMusic.cpp
		bindings/C/AUD_Handle.cpp
		bindings/C/AUD_PlaybackManager.cpp
		bindings/C/AUD_Sequence.cpp
		bindings/C/AUD_Sound.cpp
		bindings/C/AUD_Special.cpp
	)
	set(C_HDR
		bindings/C/AUD_ThreadPool.h
		bindings/C/AUD_Source.h
		bindings/C/AUD_Device.h
		bindings/C/AUD_DynamicMusic.h
		bindings/C/AUD_Handle.h
		bindings/C/AUD_PlaybackManager.h
		bindings/C/AUD_Sequence.h
		bindings/C/AUD_Sound.h
		bindings/C/AUD_Special.h
		bindings/C/AUD_Types.h
	)

if(WITH_FFTW)
	list(APPEND C_SRC
			bindings/C/AUD_HRTF.cpp
			bindings/C/AUD_ImpulseResponse.cpp
			)

		list(APPEND C_HDR
			bindings/C/AUD_HRTF.h
			bindings/C/AUD_ImpulseResponse.h
			)
	endif()

	if(NOT SEPARATE_C)
		list(APPEND SRC ${C_SRC})
		list(APPEND HDR ${C_HDR})
	else()
		set(AUDASPACE_C_LIBRARY -laudaspace-c)
	endif()
endif()

# FFMPEG
if(WITH_FFMPEG)
	if(AUDASPACE_STANDALONE)
		find_package(FFMPEG ${PACKAGE_OPTION})
	endif()

	if(FFMPEG_FOUND)
		set(FFMPEG_SRC
			plugins/ffmpeg/FFMPEG.cpp
			plugins/ffmpeg/FFMPEGReader.cpp
			plugins/ffmpeg/FFMPEGWriter.cpp
		)
		set(FFMPEG_HDR
			plugins/ffmpeg/FFMPEG.h
			plugins/ffmpeg/FFMPEGReader.h
			plugins/ffmpeg/FFMPEGWriter.h
		)

		if(NOT PLUGIN_FFMPEG)
			list(APPEND INCLUDE ${FFMPEG_INCLUDE_DIRS})
			list(APPEND LIBRARIES ${FFMPEG_LIBRARIES})
			list(APPEND SRC ${FFMPEG_SRC})
			list(APPEND HDR ${FFMPEG_HDR})
			list(APPEND STATIC_PLUGINS FFMPEG)
		endif()

		if(WIN32 AND AUDASPACE_STANDALONE)
			file(GLOB FFMPEG_DLLS ${LIBRARY_PATH}/ffmpeg/bin/*.dll)
			list(APPEND DLLS ${FFMPEG_DLLS})
		endif()
	else()
		set(WITH_FFMPEG FALSE CACHE BOOL "Build With FFMPEG" FORCE)
		message(WARNING "FFMPEG not found, plugin will not be built.")
	endif()
endif()

# FFTW
if(WITH_FFTW)
	if(AUDASPACE_STANDALONE)
		find_package(FFTW ${PACKAGE_OPTION})
	endif()

	if(FFTW_FOUND)
		set(FFTW_SRC
			src/fx/BinauralSound.cpp
			src/fx/BinauralReader.cpp
			src/fx/Convolver.cpp
			src/fx/ConvolverReader.cpp
			src/fx/ConvolverSound.cpp
			src/fx/FFTConvolver.cpp
			src/fx/HRTF.cpp
			src/fx/ImpulseResponse.cpp
			src/util/FFTPlan.cpp
		)
	set(FFTW_HDR
		include/fx/BinauralSound.h
			include/fx/BinauralReader.h
			include/fx/Convolver.h
			include/fx/ConvolverReader.h
			include/fx/ConvolverSound.h
			include/fx/FFTConvolver.h
			include/fx/HRTF.h
			include/fx/HRTFLoader.h
			include/fx/ImpulseResponse.h
			include/util/FFTPlan.h
		)

		add_definitions(-DWITH_CONVOLUTION)

		list(APPEND INCLUDE ${FFTW_INCLUDE_DIR})
		list(APPEND LIBRARIES ${FFTW_LIBRARY})

		list(APPEND SRC ${FFTW_SRC})
		list(APPEND HDR ${FFTW_HDR})

		if(WIN32 AND AUDASPACE_STANDALONE)
			file(GLOB FFTW_DLLS ${LIBRARY_PATH}/fftw/bin/*.dll)
			list(APPEND DLLS ${FFTW_DLLS})
		endif()
	else()
		set(WITH_FFTW FALSE CACHE BOOL "Build With FFTW" FORCE)
		message(WARNING "FFTW not found, convolution functionality will not be built.")
	endif()
endif()

# JACK
if(WITH_JACK)
	if(AUDASPACE_STANDALONE)
		find_package(Jack ${PACKAGE_OPTION})
	endif()

	if(JACK_FOUND)
		set(JACK_SRC
			plugins/jack/JackDevice.cpp
			plugins/jack/JackSynchronizer.cpp
			plugins/jack/JackLibrary.cpp
		)
		set(JACK_HDR
			plugins/jack/JackDevice.h
			plugins/jack/JackSynchronizer.h
			plugins/jack/JackLibrary.h
			plugins/jack/JackSymbols.h
		)

	if(DYNLOAD_JACK)
			add_definitions(-DDYNLOAD_JACK)
		endif()

		if(NOT PLUGIN_JACK)
			list(APPEND INCLUDE ${JACK_INCLUDE_DIRS})
			if(NOT DYNLOAD_JACK)
				list(APPEND LIBRARIES ${JACK_LIBRARIES})
			endif()
			list(APPEND SRC ${JACK_SRC})
			list(APPEND HDR ${JACK_HDR})
			list(APPEND STATIC_PLUGINS JackDevice)
		endif()

		if(WIN32 AND AUDASPACE_STANDALONE)
			file(GLOB JACK_DLLS ${LIBRARY_PATH}/jack/bin/*.dll)
			list(APPEND DLLS ${JACK_DLLS})
		endif()
	else()
		set(WITH_JACK FALSE CACHE BOOL "Build With JACK" FORCE)
		message(WARNING "JACK not found, plugin will not be built.")
	endif()
endif()

# LibSndFile
if(WITH_LIBSNDFILE)
	if(AUDASPACE_STANDALONE)
		find_package(LibSndFile ${PACKAGE_OPTION})
	endif()

	if(LIBSNDFILE_FOUND)
		set(LIBSNDFILE_SRC
			plugins/libsndfile/SndFile.cpp
			plugins/libsndfile/SndFileReader.cpp
			plugins/libsndfile/SndFileWriter.cpp
		)
		set(LIBSNDFILE_HDR
			plugins/libsndfile/SndFile.h
			plugins/libsndfile/SndFileReader.h
			plugins/libsndfile/SndFileWriter.h
		)

		if(NOT PLUGIN_LIBSNDFILE)
			list(APPEND INCLUDE ${LIBSNDFILE_INCLUDE_DIRS})
			list(APPEND LIBRARIES ${LIBSNDFILE_LIBRARIES})
			list(APPEND SRC ${LIBSNDFILE_SRC})
			list(APPEND HDR ${LIBSNDFILE_HDR})
			list(APPEND STATIC_PLUGINS SndFile)
		endif()

		if(WIN32 AND AUDASPACE_STANDALONE)
			file(GLOB LIBSNDFILE_DLLS ${LIBRARY_PATH}/libsndfile/bin/*.dll)
			list(APPEND DLLS ${LIBSNDFILE_DLLS})
		endif()
	else()
		set(WITH_LIBSNDFILE FALSE CACHE BOOL "Build With LibSndFile" FORCE)
		message(WARNING "LibSndFile not found, plugin will not be built.")
	endif()
endif()

# OpenAL
if(WITH_OPENAL)
	if(AUDASPACE_STANDALONE)
		find_package(OpenAL ${PACKAGE_OPTION})
	endif()

	if(OPENAL_FOUND)
		set(OPENAL_SRC
			plugins/openal/OpenALDevice.cpp
			plugins/openal/OpenALReader.cpp
		)
		set(OPENAL_HDR
			plugins/openal/OpenALDevice.h
			plugins/openal/OpenALReader.h
		)

		if(NOT PLUGIN_OPENAL)
			list(APPEND INCLUDE ${OPENAL_INCLUDE_DIR})
			list(APPEND LIBRARIES ${OPENAL_LIBRARY})
			list(APPEND SRC ${OPENAL_SRC})
			list(APPEND HDR ${OPENAL_HDR})
			list(APPEND STATIC_PLUGINS OpenALDevice)
		endif()

		if(WIN32 AND AUDASPACE_STANDALONE)
			file(GLOB OPENAL_DLLS ${LIBRARY_PATH}/OpenAL/bin/*.dll)
			list(APPEND DLLS ${OPENAL_DLLS})
		endif()
	else()
		set(WITH_OPENAL FALSE CACHE BOOL "Build With OpenAL" FORCE)
		message(WARNING "OpenAL not found, plugin will not be built.")
	endif()
endif()

# Python
if(WITH_PYTHON)
	if(AUDASPACE_STANDALONE)
		find_package(PythonLibs 3.2 ${PACKAGE_OPTION})
		find_package(NumPy ${PACKAGE_OPTION})
	endif()

	if(PYTHONLIBS_FOUND AND NUMPY_FOUND)
		list(APPEND INCLUDE ${PYTHON_INCLUDE_DIRS} ${NUMPY_INCLUDE_DIRS})

		if(WITH_PYTHON_MODULE)
			find_package(PythonInterp 3.2 ${PACKAGE_OPTION})

			if(NOT PYTHONINTERP_FOUND)
				set(WITH_PYTHON_MODULE FALSE)
				message(WARNING "Python interpreter not found, module will not be built.")
			endif()
		endif()

		set(AUDASPACE_PY_LIBRARY -laudaspace-py)

		if(WIN32 AND AUDASPACE_STANDALONE)
			file(GLOB PYTHON_DLLS ${LIBRARY_PATH}/Python/bin/*.dll)
			list(APPEND DLLS ${PYTHON_DLLS})
		endif()
	else()
		set(WITH_PYTHON FALSE CACHE BOOL "Build With Python Library" FORCE)
		message(WARNING "Python libraries not found, language binding will not be built.")
	endif()
endif()

# SDL
if(WITH_SDL)
	if(AUDASPACE_STANDALONE)
		if(USE_SDL2)
			find_package(SDL2)
			if(SDL2_FOUND)
				set(SDL_INCLUDE_DIR ${SDL2_INCLUDE_DIR})
				set(SDL_LIBRARY ${SDL2_LIBRARY})
				set(SDL_FOUND TRUE)
			else()
				find_package(SDL ${PACKAGE_OPTION})
			endif()
		else()
			find_package(SDL ${PACKAGE_OPTION})
		endif()
	endif()

	if(SDL_FOUND)
		set(SDL_SRC
			plugins/sdl/SDLDevice.cpp
		)
		set(SDL_HDR
			plugins/sdl/SDLDevice.h
		)

		if(NOT PLUGIN_SDL)
			list(APPEND INCLUDE ${SDL_INCLUDE_DIR})
			list(APPEND LIBRARIES ${SDL_LIBRARY})
			list(APPEND SRC ${SDL_SRC})
			list(APPEND HDR ${SDL_HDR})
			list(APPEND STATIC_PLUGINS SDLDevice)
		endif()

		if(WIN32 AND AUDASPACE_STANDALONE)
			file(GLOB SDL_DLLS ${LIBRARY_PATH}/sdl/bin/*.dll)
			list(APPEND DLLS ${SDL_DLLS})
		endif()
	else()
		set(WITH_SDL FALSE CACHE BOOL "Build With SDL" FORCE)
		message(WARNING "SDL not found, plugin will not be built.")
	endif()
endif()

# library configuration

if(SHARED_LIBRARY)
	set(AUD_LIBRARY_TYPE AUD_SHARED_LIBRARY)
	set(LIBRARY_TYPE SHARED)
	add_definitions(-DAUD_BUILD_SHARED_LIBRARY)
else()
	set(AUD_LIBRARY_TYPE AUD_STATIC_LIBRARY)
	set(LIBRARY_TYPE STATIC)
endif()

# file configuration

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/Audaspace.h.in ${CMAKE_CURRENT_BINARY_DIR}/Audaspace.h ESCAPE_QUOTES @ONLY)

list(APPEND HDR ${CMAKE_CURRENT_BINARY_DIR}/Audaspace.h)

set(STATIC_PLUGIN_CLASSES "")
set(STATIC_PLUGIN_REGISTERS "")

foreach(PLUGIN ${STATIC_PLUGINS})
	list(APPEND STATIC_PLUGIN_CLASSES "STATIC_PLUGIN_CLASS(" ${PLUGIN} ")\n")
	list(APPEND STATIC_PLUGIN_REGISTERS "\tSTATIC_PLUGIN_REGISTER(" ${PLUGIN} ")\n")
endforeach()

string(CONCAT STATIC_PLUGIN_CLASSES ${STATIC_PLUGIN_CLASSES})
string(CONCAT STATIC_PLUGIN_REGISTERS ${STATIC_PLUGIN_REGISTERS})

if(WIN32)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/PluginManagerWindows.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/PluginManager.cpp ESCAPE_QUOTES @ONLY)
	if(WITH_FFTW)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/fx/HRTFLoaderWindows.cpp ${CMAKE_CURRENT_BINARY_DIR}/HRTFLoader.cpp COPYONLY)
	endif()
else()
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/PluginManagerUnix.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/PluginManager.cpp ESCAPE_QUOTES @ONLY)
	if(WITH_FFTW)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/fx/HRTFLoaderUnix.cpp ${CMAKE_CURRENT_BINARY_DIR}/HRTFLoader.cpp COPYONLY)
	endif()
endif()

list(APPEND SRC ${CMAKE_CURRENT_BINARY_DIR}/PluginManager.cpp)
if(WITH_FFTW)
	list(APPEND SRC ${CMAKE_CURRENT_BINARY_DIR}/HRTFLoader.cpp)
endif()

# directories

include_directories(${INCLUDE})
link_directories()

# install configuration

if(WIN32)
	set(BIN_DESTINATION ".")
else()
	set(BIN_DESTINATION "bin")
endif()

set(LIB_DESTINATION "lib${LIB_SUFFIX}")

# library

add_library(audaspace ${LIBRARY_TYPE} ${SRC} ${HDR})
target_link_libraries(audaspace ${LIBRARIES})
set_target_properties(audaspace PROPERTIES SOVERSION ${AUDASPACE_VERSION})

if(AUDASPACE_STANDALONE)
	install(TARGETS audaspace
		RUNTIME DESTINATION ${BIN_DESTINATION}
		LIBRARY DESTINATION ${LIB_DESTINATION}
		ARCHIVE DESTINATION ${LIB_DESTINATION}
	)

	install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/audaspace)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Audaspace.h DESTINATION include/audaspace)

	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/packages/pkgconfig/audaspace.pc.in ${CMAKE_CURRENT_BINARY_DIR}/audaspace.pc @ONLY)

	if(NOT WIN32 AND NOT APPLE)
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/audaspace.pc DESTINATION "lib${LIB_SUFFIX}/pkgconfig")
	endif()
endif()

# plugins

if(WITH_FFMPEG AND PLUGIN_FFMPEG)
	add_definitions(-DFFMPEG_PLUGIN)
	include_directories(${INCLUDE} ${FFMPEG_INCLUDE_DIRS})
	add_library(audffmpeg SHARED ${FFMPEG_SRC} ${FFMPEG_HDR} ${HDR})
	target_link_libraries(audffmpeg audaspace ${FFMPEG_LIBRARIES})
	set_target_properties(audffmpeg PROPERTIES SOVERSION ${AUDASPACE_VERSION})
	install(TARGETS audffmpeg DESTINATION ${DEFAULT_PLUGIN_PATH})
endif()

if(WITH_JACK AND PLUGIN_JACK)
	add_definitions(-DJACK_PLUGIN)
	include_directories(${INCLUDE} ${JACK_INCLUDE_DIRS})
	add_library(audjack SHARED ${JACK_SRC} ${JACK_HDR} ${HDR})
	if(DYNLOAD_JACK)
		target_link_libraries(audjack audaspace)
	else()
		target_link_libraries(audjack audaspace ${JACK_LIBRARIES})
	endif()
	set_target_properties(audjack PROPERTIES SOVERSION ${AUDASPACE_VERSION})
	install(TARGETS audjack DESTINATION ${DEFAULT_PLUGIN_PATH})
endif()

if(WITH_LIBSNDFILE AND PLUGIN_LIBSNDFILE)
	add_definitions(-DLIBSNDFILE_PLUGIN)
	include_directories(${INCLUDE} ${LIBSNDFILE_INCLUDE_DIRS})
	add_library(audlibsndfile SHARED ${LIBSNDFILE_SRC} ${LIBSNDFILE_HDR} ${HDR})
	set_target_properties(audlibsndfile PROPERTIES SOVERSION ${AUDASPACE_VERSION})
	target_link_libraries(audlibsndfile audaspace ${LIBSNDFILE_LIBRARIES})
	install(TARGETS audlibsndfile DESTINATION ${DEFAULT_PLUGIN_PATH})
endif()

if(WITH_OPENAL AND PLUGIN_OPENAL)
	add_definitions(-DOPENAL_PLUGIN)
	include_directories(${INCLUDE} ${OPENAL_INCLUDE_DIR})
	add_library(audopenal SHARED ${OPENAL_SRC} ${OPENAL_HDR} ${HDR})
	set_target_properties(audopenal PROPERTIES SOVERSION ${AUDASPACE_VERSION})
	target_link_libraries(audopenal audaspace ${OPENAL_LIBRARY})
	install(TARGETS audopenal DESTINATION ${DEFAULT_PLUGIN_PATH})
endif()

if(WITH_SDL AND PLUGIN_SDL)
	add_definitions(-DSDL_PLUGIN)
	include_directories(${INCLUDE} ${SDL_INCLUDE_DIR})
	add_library(audsdl SHARED ${SDL_SRC} ${SDL_HDR} ${HDR})
	set_target_properties(audsdl PROPERTIES SOVERSION ${AUDASPACE_VERSION})
	target_link_libraries(audsdl audaspace ${SDL_LIBRARY})
	install(TARGETS audsdl DESTINATION ${DEFAULT_PLUGIN_PATH})
endif()

# dlls

if(WIN32)
	if(DLLS)
		install(FILES ${DLLS} DESTINATION ${BIN_DESTINATION})
	endif()
endif()

# demos

if(BUILD_DEMOS)
	include_directories(${INCLUDE})

	set(DEMOS audaplay audaconvert audaremap signalgen randsounds dynamicmusic playbackmanager)

	add_executable(audaplay demos/audaplay.cpp)
	target_link_libraries(audaplay audaspace)

	add_executable(audaconvert demos/audaconvert.cpp)
	target_link_libraries(audaconvert audaspace)

	add_executable(audaremap demos/audaremap.cpp)
	target_link_libraries(audaremap audaspace)

	add_executable(signalgen demos/signalgen.cpp)
	target_link_libraries(signalgen audaspace)

	add_executable(randsounds demos/randsounds.cpp)
	target_link_libraries(randsounds audaspace)

	add_executable(dynamicmusic demos/dynamicmusic.cpp)
	target_link_libraries(dynamicmusic audaspace)

	add_executable(playbackmanager demos/playbackmanager.cpp)
	target_link_libraries(playbackmanager audaspace)

	if(WITH_FFTW)
		list(APPEND DEMOS convolution binaural)

		add_executable(convolution demos/convolution.cpp)
		target_link_libraries(convolution audaspace)

		add_executable(binaural demos/binaural.cpp)
		target_link_libraries(binaural audaspace)
	endif()

	if(WITH_OPENAL)
		list(APPEND DEMOS openaldevices)

		add_executable(openaldevices demos/openaldevices.cpp)
		if(PLUGIN_OPENAL)
			target_link_libraries(openaldevices audaspace audopenal)
		else()
			target_link_libraries(openaldevices audaspace)
		endif()
	endif()

	install(TARGETS ${DEMOS}
		RUNTIME DESTINATION ${BIN_DESTINATION}
		LIBRARY DESTINATION ${LIB_DESTINATION}
		ARCHIVE DESTINATION ${LIB_DESTINATION}
	)
endif()

# bindings

if(WITH_C)
	if(SEPARATE_C)
		add_library(audaspace-c ${LIBRARY_TYPE} ${C_SRC} ${C_HDR})
		target_link_libraries(audaspace-c audaspace)
		set_target_properties(audaspace-c PROPERTIES SOVERSION ${AUDASPACE_VERSION})
		install(TARGETS audaspace-c
			RUNTIME DESTINATION ${BIN_DESTINATION}
			LIBRARY DESTINATION ${LIB_DESTINATION}
			ARCHIVE DESTINATION ${LIB_DESTINATION}
		)
	endif()

	if(AUDASPACE_STANDALONE)
		install(FILES ${C_HDR} DESTINATION include/audaspace)
	endif()
endif()

if(WITH_PYTHON)
	set(PYTHON_SRC
		bindings/python/PyAPI.cpp
		bindings/python/PyDevice.cpp
		bindings/python/PyDynamicMusic.cpp
		bindings/python/PyHandle.cpp
		bindings/python/PyPlaybackManager.cpp
		bindings/python/PySequence.cpp
		bindings/python/PySequenceEntry.cpp
		bindings/python/PySound.cpp
		bindings/python/PySource.cpp
		bindings/python/PyThreadPool.cpp
	)
	set(PYTHON_HDR
		bindings/python/PyAPI.h
		bindings/python/PyDevice.h
		bindings/python/PyDynamicMusic.h
		bindings/python/PyHandle.h
		bindings/python/PyPlaybackManager.h
		bindings/python/PySequence.h
		bindings/python/PySequenceEntry.h
		bindings/python/PySound.h
		bindings/python/PySource.h
		bindings/python/PyThreadPool.h
	)

	if(WITH_FFTW)
		list(APPEND PYTHON_SRC
			bindings/python/PyHRTF.cpp
			bindings/python/PyImpulseResponse.cpp
			)
		list(APPEND PYTHON_HDR
			bindings/python/PyHRTF.h
			bindings/python/PyImpulseResponse.h
			)
	endif()

	add_library(audaspace-py ${LIBRARY_TYPE} ${PYTHON_SRC} ${PYTHON_HDR})
	target_link_libraries(audaspace-py audaspace ${PYTHON_LIBRARIES})
	set_target_properties(audaspace-py PROPERTIES SOVERSION ${AUDASPACE_VERSION})

	if(AUDASPACE_STANDALONE)
		install(TARGETS audaspace-py
			RUNTIME DESTINATION ${BIN_DESTINATION}
			LIBRARY DESTINATION ${LIB_DESTINATION}
			ARCHIVE DESTINATION ${LIB_DESTINATION}
		)

		install(FILES ${PYTHON_HDR} DESTINATION include/audaspace/python)
	endif()

	if(WITH_PYTHON_MODULE)
		set(PYTHON_SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python)
		configure_file(${PYTHON_SOURCE_DIRECTORY}/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/setup.py ESCAPE_QUOTES @ONLY)

		if(APPLE)
			add_custom_command(OUTPUT build COMMAND MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET} ${PYTHON_EXECUTABLE} setup.py build DEPENDS ${PYTHON_SRC} ${PYTHON_HDR})
		elseif(WIN32)
			set(ENV{VS100COMNTOOLS} $ENV{VS120COMNTOOLS})
			add_custom_command(OUTPUT build COMMAND ${PYTHON_EXECUTABLE} setup.py build DEPENDS ${PYTHON_SRC} ${PYTHON_HDR})
		else()
			add_custom_command(OUTPUT build COMMAND ${PYTHON_EXECUTABLE} setup.py build DEPENDS ${PYTHON_SRC} ${PYTHON_HDR})
		endif()
		add_custom_target(pythonmodule ALL DEPENDS build SOURCES ${PYTHON_SOURCE_DIRECTORY}/setup.py.in ${PYTHON_SRC} ${PYTHON_HDR})
		add_dependencies(pythonmodule audaspace)

		install(CODE "EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} setup.py install --root=\$ENV{DESTDIR} --prefix=${CMAKE_INSTALL_PREFIX})")
	endif()
endif()

# docs

if(WITH_DOCS)
	find_package(Doxygen ${PACKAGE_OPTION})

	if(DOXYGEN_FOUND AND DOXYGEN_DOT_FOUND)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

		add_custom_target(audaspace_doc ALL ${DOXYGEN_EXECUTABLE} Doxyfile COMMENT "Building C++ HTML documentation with Doxygen.")
	else()
		set(WITH_DOCS FALSE CACHE BOOL "Build C++ HTML Documentation with Doxygen" FORCE)
		message(WARNING "Doxygen (and/or dot) not found, documentation will not be built.")
	endif()
endif()

if(WITH_BINDING_DOCS)
	find_package(Sphinx ${PACKAGE_OPTION})

	if(SPHINX_FOUND)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bindings/doc/conf.py.in ${CMAKE_CURRENT_BINARY_DIR}/conf.py @ONLY)

		add_custom_target(bindings_doc ALL COMMAND ${PYTHON_EXECUTABLE} setup.py --build-docs ${SPHINX_EXECUTABLE} -q -b html -c "${CMAKE_CURRENT_BINARY_DIR}" -d "${CMAKE_CURRENT_BINARY_DIR}/_doctrees" "${CMAKE_CURRENT_SOURCE_DIR}/bindings/doc" "${CMAKE_CURRENT_BINARY_DIR}/doc/bindings" DEPENDS pythonmodule COMMENT "Building C/Python HTML documentation with Sphinx.")
	else()
		set(WITH_BINDING_DOCS FALSE CACHE BOOL "Build C/Python HTML Documentation with Sphinx" FORCE)
		message(WARNING "Sphinx not found, binding documentation will not be built.")
	endif()
endif()

if(WITH_DOCS OR WITH_BINDING_DOCS)
	install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/ DESTINATION ${DOCUMENTATION_INSTALL_PATH})
endif()
