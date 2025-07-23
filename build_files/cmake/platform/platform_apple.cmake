# SPDX-FileCopyrightText: 2016 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Libraries configuration for Apple.

macro(find_package_wrapper)
  # do nothing, just satisfy the macro
endmacro()

function(print_found_status
  lib_name
  lib_path
  )

  if(FIRST_RUN)
    if(lib_path)
      message(STATUS "Found ${lib_name}: ${lib_path}")
    else()
      message(WARNING "Could NOT find ${lib_name}")
    endif()
  endif()
endfunction()

# -------------------------------------------------------------------------------
# Apple cross-platform device build config
if(WITH_APPLE_CROSSPLATFORM)
  # Disable modules with no planned/required support on iOS.
  set(NO_PLATFORM_SUPPORT_MSG "Auto disabled as APPLE_TARGET_DEVICE=ios")
  set(WITH_VULKAN_BACKEND  OFF CACHE BOOL ${NO_PLATFORM_SUPPORT_MSG} FORCE)
  set(WITH_OPENGL_BACKEND  OFF CACHE BOOL ${NO_PLATFORM_SUPPORT_MSG} FORCE)
  set(WITH_SDL OFF CACHE BOOL ${NO_PLATFORM_SUPPORT_MSG} FORCE)
  set(WITH_INPUT_NDOF OFF CACHE BOOL ${NO_PLATFORM_SUPPORT_MSG} FORCE)
  set(WITH_PYTHON_MODULE OFF CACHE BOOL ${NO_PLATFORM_SUPPORT_MSG} FORCE)
  # Disable these modules for now
  set(WITH_OPENSUBDIV OFF CACHE BOOL "Disable until OSD supports Metal" FORCE)
  set(WITH_PYTHON_INSTALL_ZSTANDARD OFF CACHE BOOL "Disable until iOS build supports SSL" FORCE)
  # Disable Audaspace as it had dependencies on CoreAudio components which do not exist on iOS
  # (like AudioToolbox/CoreAudioClock.h)
  set(WITH_AUDASPACE OFF CACHE BOOL "Auto disabled due to lack of specific CoreAudio support on iOS" FORCE)
  # Temp: Disabled pending compilation of HgI/Hydra Storm for Metal on iOS.
  #       (Set DPXR_ENABLE_METAL_SUPPORT=ON in usd.cmake for WITH_APPLE_CROSSPLATFORM platform)
  set(WITH_HYDRA  OFF CACHE BOOL "Auto disabled due to lack of HgI/Hydra Storm for Metal on iOS" FORCE)
  set(WITH_CYCLES_OSL OFF CACHE BOOL "Support for build time compilation of OSL Shaders not supported yet on iOS" FORCE)

  # --- Cross compile host tools ----

  # Enable cross-compiled tools (glsl_preprocess, makesdna, makesrna etc.)
  set(WITH_CROSSCOMPILED_TOOLS ON CACHE BOOL "" FORCE)

  # Fetch Cmake arguments for host build process, ensuring these are consistent with what is
  # locally disabled, but toggling APPLE_TARGET_DEVICE to macos.
  get_cmake_property(CACHE_VARS CACHE_VARIABLES)
  foreach(CACHE_VAR ${CACHE_VARS})
    get_property(CACHE_VAR_TYPE CACHE ${CACHE_VAR} PROPERTY TYPE)
    if(CACHE_VAR_TYPE STREQUAL "UNINITIALIZED")
      set(CACHE_VAR_TYPE)
    else()
      if(CACHE_VAR_TYPE STREQUAL "BOOL")
        # Remove IPAD arg
        if(NOT CACHE_VAR STREQUAL "APPLE_TARGET_DEVICE" AND NOT CACHE_VAR STREQUAL "WITH_CROSSCOMPILED_TOOLS")
          set(CMAKE_ARGS "${CMAKE_ARGS} -D${CACHE_VAR}=${${CACHE_VAR}}")
        else()
          # Disable iPad for tools compilation
          set(CMAKE_ARGS "${CMAKE_ARGS} -D${CACHE_VAR}=OFF")
        endif()
      endif()
    endif()
  endforeach()

  message(STATUS " \n---------------------------\n CROSS COMPILE TOOLS:\n\nDetect CMake configuration for host-tools-build (datatoc, datatoc_icon, makesdna, makesrna, msgformat, glsl_preprocess) \n\nInheriting CMAKE_ARGS:\n${CMAKE_ARGS}\n")


  # Run host build process to ensure host tools are up to date. (creating build_darwin_tools folder)
  # NOTE: ENV command used to isolate environment, as running inside Xcode otherhwise causes Cflags to be inherited.
  set(CROSSCOMPILE_TOOLDIR "${CMAKE_SOURCE_DIR}/../build_ios/build_darwin_tools/${CMAKE_BUILD_TYPE}")
  # Override the defines that are used for building Blender (make sure they come after CMAKE_ARGS)
  set(CMAKE_TOOLS_ARGS "${CMAKE_ARGS} -DWITH_CROSSCOMPILED_TOOLS=ON -DAPPLE_TARGET_DEVICE=macos ${CROSSCOMPILE_C_FLAGS} ${CROSSCOMPILE_CXX_FLAGS}")
  # IOS_FIXME - Add Cross-Compile defines to the C-Flags
  # This is a bit of a fudge to make sure that the cross-compiled tools know that we're building
  # in a cross-compile environment in order that all class and struct definitions match (specificially for RNA).
  # Ideally we'd build the tools to the same build-type but DEBUG tools would slow the compile process down.
  # That might still be a better option though. See "debug_size_" in BLI_vector.hh for an example of this.
  set(CMAKE_TOOLS_ARGS "${CMAKE_TOOLS_ARGS} -DCMAKE_C_FLAGS=\"-DWITH_CROSSCOMPILED_TOOLS -DWITH_APPLE_CROSSPLATFORM\"")
  set(CMAKE_TOOLS_ARGS "${CMAKE_TOOLS_ARGS} -DCMAKE_CXX_FLAGS=\"-DWITH_CROSSCOMPILED_TOOLS -DWITH_APPLE_CROSSPLATFORM\"")
  
  add_custom_target(blender_cross_tools_compile
    COMMENT "\n---------------------------\n Building Cross Compile Tools\n"
    COMMAND cd ${CMAKE_SOURCE_DIR}
    COMMAND env -i BUILD_CMAKE_ARGS=${CMAKE_TOOLS_ARGS} BUILD_DIR=${CROSSCOMPILE_TOOLDIR} make tools
  )
  
  # Configure executable dependencies and paths for cross-compiled tools.
  # NOTE: Local dependency creation e.g. makesdna should check WITH_CROSSCOMPILED_TOOLS before creating local target.
  #       We instead run a full build to generate the targets in a host-side build on macOS.
  add_executable(makesdna IMPORTED GLOBAL)
  add_executable(makesrna IMPORTED GLOBAL)
  add_executable(msgfmt IMPORTED GLOBAL)
  add_executable(datatoc IMPORTED GLOBAL)
  #add_executable(datatoc_icon IMPORTED GLOBAL)
  add_executable(glsl_preprocess IMPORTED GLOBAL)
  add_dependencies(makesdna blender_cross_tools_compile)
  add_dependencies(makesrna blender_cross_tools_compile)
  add_dependencies(msgfmt blender_cross_tools_compile)
  add_dependencies(datatoc blender_cross_tools_compile)
  #add_dependencies(datatoc_icon blender_cross_tools_compile)
  add_dependencies(glsl_preprocess blender_cross_tools_compile)
  message(STATUS "Host tools will be generated in: ${CROSSCOMPILE_TOOLDIR}")
  set_property(TARGET makesdna PROPERTY IMPORTED_LOCATION "${CROSSCOMPILE_TOOLDIR}/bin/makesdna")
  set_property(TARGET makesrna PROPERTY IMPORTED_LOCATION "${CROSSCOMPILE_TOOLDIR}/bin/makesrna")
  set_property(TARGET msgfmt PROPERTY IMPORTED_LOCATION "${CROSSCOMPILE_TOOLDIR}/bin/msgfmt")
  set_property(TARGET datatoc PROPERTY IMPORTED_LOCATION "${CROSSCOMPILE_TOOLDIR}/bin/datatoc")
  #set_property(TARGET datatoc_icon PROPERTY IMPORTED_LOCATION "${CROSSCOMPILE_TOOLDIR}/bin/datatoc_icon")
  set_property(TARGET glsl_preprocess PROPERTY IMPORTED_LOCATION "${CROSSCOMPILE_TOOLDIR}/bin/glsl_preprocess")
  message(STATUS "makesdna: ${CROSSCOMPILE_TOOLDIR}/bin/makesdna")
  message(STATUS "makesrna: ${CROSSCOMPILE_TOOLDIR}/bin/makesrna")
  message(STATUS "msgfmt: ${CROSSCOMPILE_TOOLDIR}/bin/msgfmt")
  message(STATUS "datatoc: ${CROSSCOMPILE_TOOLDIR}/bin/datatoc")
  #message(STATUS "datatoc_icon: ${CROSSCOMPILE_TOOLDIR}/bin/datatoc_icon")
  message(STATUS "glsl_preprocess: ${CROSSCOMPILE_TOOLDIR}/bin/glsl_preprocess")
  message(STATUS "\n---------------------------\n")
else()
  # Disable cross-compiled tools (glsl_preprocess, makesdna, makesrna etc.) if building on host.
  set(WITH_CROSSCOMPILED_TOOLS OFF CACHE BOOL "" FORCE)
endif()

# ------------------------------------------------------------------------
# Find system provided libraries.

# Find system ZLIB
if(NOT WITH_APPLE_CROSSPLATFORM)

  # Ensure we only find the library associated with the macOS SDK.
  set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

  set(ZLIB_ROOT /usr)
  find_package(ZLIB REQUIRED)
  find_package(BZip2 REQUIRED)
  list(APPEND ZLIB_LIBRARIES ${BZIP2_LIBRARIES})

  # Reset search mode.
  set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
endif()

if(WITH_JACK)
  find_library(JACK_FRAMEWORK
    NAMES jackmp
  )

  if(JACK_FRAMEWORK)
    set(JACK_INCLUDE_DIRS ${JACK_FRAMEWORK}/headers)
  else()
    set_and_warn_library_found("JACK" JACK_FRAMEWORK WITH_JACK)
  endif()
endif()

if(NOT DEFINED LIBDIR)
  if(WITH_APPLE_CROSSPLATFORM)
    set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/${APPLE_TARGET_DEVICE}_${CMAKE_OSX_ARCHITECTURES})
  else()
    if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64")
      set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/macos_x64)
    else()
      set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/macos_${CMAKE_OSX_ARCHITECTURES})
    endif()
  endif()
else()
  if(FIRST_RUN)
    message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
  endif()
endif()

if(WITH_APPLE_CROSSPLATFORM)
  # Check whether python lib exists as prebuilt IOS libs will not have their own repo.
  if(NOT EXISTS "${LIBDIR}/python/")
    message(FATAL_ERROR "IOS build requires pre-compiled libs at: '${LIBDIR}'")
  endif()
else()
  if(NOT EXISTS "${LIBDIR}/python/")
    message(FATAL_ERROR "Mac OSX requires pre-compiled libs at: '${LIBDIR}'")
  endif()
endif()
if(FIRST_RUN)
  message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
endif()

message(STATUS "Searching pre-compiled LIBDIR: ${LIBDIR}")

# Avoid searching for headers since this would otherwise override our lib
# directory as well as PYTHON_ROOT_DIR.
set(CMAKE_FIND_FRAMEWORK NEVER)

# Optionally use system Python if PYTHON_ROOT_DIR is specified.alisa
if(NOT WITH_APPLE_CROSSPLATFORM)
  if(WITH_PYTHON)
    if(WITH_PYTHON_MODULE AND PYTHON_ROOT_DIR)
      find_package(PythonLibsUnix REQUIRED)
    endif()
  else()
    # Python executable is needed as part of the build-process,
    # note that building without Python is quite unusual.
    find_program(PYTHON_EXECUTABLE "python3")
  endif()

else()
  # When building for iOS we use the MacOS version of Python from the macos libs dir
  set(CROSSCOMPILE_HOST_LIBDIR "${CMAKE_SOURCE_DIR}/lib/macos_arm64")
  if(NOT PYTHON_VERSION)
	# IOS_FIXME: This is not great why is PYTHON_VERSION not defined here?
	message("WARNING Manually defining Python Version to 3.11 for iOS build")
	set(PYTHON_EXECUTABLE "${CROSSCOMPILE_HOST_LIBDIR}/python/bin/python3.11")
  else()
    set(PYTHON_EXECUTABLE "${CROSSCOMPILE_HOST_LIBDIR}/python/bin/python${PYTHON_VERSION}")
  endif()
  if(NOT EXISTS ${PYTHON_EXECUTABLE})
    message(
      FATAL_ERROR
      "Missing: <${PYTHON_EXECUTABLE}>\n"
      "MacOS version of Python must exist to build iOS version\n"
	  "Try building MacOS version first: 'make update' or 'make deps'\n"
    )
  endif()
  
  message(STATUS "HOST PYTHON EXECUTABLE: ${PYTHON_EXECUTABLE}")
endif()

# Prefer lib directory paths
file(GLOB LIB_SUBDIRS ${LIBDIR}/*)
set(CMAKE_PREFIX_PATH ${LIB_SUBDIRS})

# -------------------------------------------------------------------------
# Find precompiled libraries, and avoid system or user-installed ones.

if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW) # CMake 3.27+ Always uses upper-case <PACKAGENAME>_ROOT
endif()

# Find pre-compiled ZLIB for MacOS
if(NOT WITH_APPLE_CROSSPLATFORM)
  set(ZLIB_ROOT /usr)
  find_package(ZLIB REQUIRED)
  find_package(BZip2 REQUIRED)
  list(APPEND ZLIB_LIBRARIES ${BZIP2_LIBRARIES})
else()
  set(ZLIB_ROOT ${LIBDIR}/zlib)
  find_package(ZLIB REQUIRED)
  # Why is this required for iOS? Get link errors otherwise
  add_bundled_libraries(zlib/lib)
endif()

if(EXISTS ${LIBDIR})
  include(platform_old_libs_update)
  without_system_libs_begin()
endif()

if(WITH_ALEMBIC)
  find_package(Alembic)
endif()

if(WITH_USD)
  find_package(USD REQUIRED)
endif()
add_bundled_libraries(usd/lib)

if(WITH_MATERIALX)
  find_package(MaterialX)
  set_and_warn_library_found("MaterialX" MaterialX_FOUND WITH_MATERIALX)
endif()
add_bundled_libraries(materialx/lib)

if(WITH_OPENSUBDIV)
  find_package(OpenSubdiv)
endif()
add_bundled_libraries(opensubdiv/lib)

if(WITH_APPLE_CROSSPLATFORM)
  set(OPENSUBDIV_INCLUDE_DIRS ${OPENSUBDIV_INCLUDE_DIR})
endif()
if(WITH_VULKAN_BACKEND)
  find_package(MoltenVK REQUIRED)
  find_package(ShaderC REQUIRED)
  find_package(Vulkan REQUIRED)
endif()

if(WITH_CODEC_SNDFILE)
  find_package(SndFile)
  find_library(_sndfile_FLAC_LIBRARY NAMES flac HINTS ${LIBDIR}/sndfile/lib)
  find_library(_sndfile_OGG_LIBRARY NAMES ogg HINTS ${LIBDIR}/ffmpeg/lib)
  find_library(_sndfile_VORBIS_LIBRARY NAMES vorbis HINTS ${LIBDIR}/ffmpeg/lib)
  find_library(_sndfile_VORBISENC_LIBRARY NAMES vorbisenc HINTS ${LIBDIR}/ffmpeg/lib)
  list(APPEND LIBSNDFILE_LIBRARIES
    ${_sndfile_FLAC_LIBRARY}
    ${_sndfile_OGG_LIBRARY}
    ${_sndfile_VORBIS_LIBRARY}
    ${_sndfile_VORBISENC_LIBRARY}
  )

  print_found_status("SndFile libraries" "${LIBSNDFILE_LIBRARIES}")
  unset(_sndfile_FLAC_LIBRARY)
  unset(_sndfile_OGG_LIBRARY)
  unset(_sndfile_VORBIS_LIBRARY)
  unset(_sndfile_VORBISENC_LIBRARY)
endif()

if(WITH_PYTHON)
  if(NOT (WITH_PYTHON_MODULE AND PYTHON_ROOT_DIR))
    find_package(PythonLibsUnix REQUIRED)
  endif()
endif()

if(WITH_FFTW3)
  set(FFTW3_ROOT_DIR ${LIBDIR}/fftw3)
  find_package(Fftw3)
endif()

# FreeType compiled with Brotli compression for woff2.
set(FREETYPE_ROOT_DIR ${LIBDIR}/freetype)
find_package(Freetype REQUIRED)
set(BROTLI_LIBRARIES
  ${LIBDIR}/brotli/lib/libbrotlicommon-static.a
  ${LIBDIR}/brotli/lib/libbrotlidec-static.a
)

if(WITH_HARFBUZZ)
  find_package(Harfbuzz)
endif()

if(WITH_FRIBIDI)
  find_package(Fribidi)
endif()

# Header dependency of required OpenImageIO.
find_package(OpenEXR REQUIRED)
add_bundled_libraries(openexr/lib)
add_bundled_libraries(imath/lib)

if(WITH_CODEC_FFMPEG)
  set(FFMPEG_ROOT_DIR ${LIBDIR}/ffmpeg)
  set(FFMPEG_FIND_COMPONENTS
    avcodec avdevice avformat avutil
    mp3lame ogg opus swresample swscale
    theora theoradec theoraenc vorbis vorbisenc
    vorbisfile vpx x264)
  if(EXISTS ${LIBDIR}/ffmpeg/lib/libaom.a)
    list(APPEND FFMPEG_FIND_COMPONENTS aom)
  endif()
  if(NOT WITH_APPLE_CROSSPLATFORM)
    # NOTE: Issue with library discovery for iOS.
    if(EXISTS ${LIBDIR}/ffmpeg/lib/libxvidcore.a)
      list(APPEND FFMPEG_FIND_COMPONENTS xvidcore)
    endif()
  endif()
  find_package(FFmpeg)
endif()

if(WITH_IMAGE_OPENJPEG OR WITH_CODEC_FFMPEG)
  # use openjpeg from libdir that is linked into ffmpeg
  find_package(OpenJPEG)
endif()

find_library(SYSTEMSTUBS_LIBRARY
  NAMES
  SystemStubs
  PATHS
)
mark_as_advanced(SYSTEMSTUBS_LIBRARY)
if(SYSTEMSTUBS_LIBRARY)
  list(APPEND PLATFORM_LINKLIBS SystemStubs)
endif()

string(APPEND PLATFORM_CFLAGS " -pipe -funsigned-char -fno-strict-aliasing -ffp-contract=off")

if(WITH_APPLE_CROSSPLATFORM)
  # Link different frameworks for iOS
  set(PLATFORM_LINKFLAGS
    "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework UIKit -framework AudioToolbox -framework CoreAudio -framework Metal -framework MetalKit -framework QuartzCore -framework ImageIO -framework GameController" 
  )
  list(APPEND PLATFORM_LINKLIBS "${LIBDIR}/libb2/lib/libb2.a")
else()
  set(PLATFORM_LINKFLAGS
    "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Cocoa -framework Carbon -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework Metal -framework QuartzCore"
  )

  if(WITH_OPENIMAGEDENOISE)
    if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
      # OpenImageDenoise uses BNNS from the Accelerate framework.
      string(APPEND PLATFORM_LINKFLAGS " -framework Accelerate")
    endif()
  endif()

  if(WITH_JACK)
    string(APPEND PLATFORM_LINKFLAGS " -F/Library/Frameworks -weak_framework jackmp")
  endif()

  if(WITH_SDL)
    find_package(SDL2)
    set(SDL_INCLUDE_DIR ${SDL2_INCLUDE_DIRS})
    set(SDL_LIBRARY ${SDL2_LIBRARIES})
    string(APPEND PLATFORM_LINKFLAGS " -framework ForceFeedback -framework GameController")
    if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
      # The minimum macOS version of the libraries makes it so this is included in SDL on arm64
      # but not x86_64.
      string(APPEND PLATFORM_LINKFLAGS " -framework CoreHaptics")
    endif()
  endif()
endif()

if(WITH_OPENCOLLADA)
  find_package(OpenCOLLADA)
  find_library(PCRE_LIBRARIES NAMES pcre HINTS ${LIBDIR}/opencollada/lib)
  find_library(XML2_LIBRARIES NAMES xml2 HINTS ${LIBDIR}/opencollada/lib)
  print_found_status("PCRE" "${PCRE_LIBRARIES}")
  print_found_status("XML2" "${XML2_LIBRARIES}")
endif()

list(APPEND PLATFORM_LINKLIBS c++)

set(EPOXY_ROOT_DIR ${LIBDIR}/epoxy)
if(NOT WITH_APPLE_CROSSPLATFORM)
  find_package(Epoxy REQUIRED)
endif()

set(PNG_ROOT ${LIBDIR}/png)
find_package(PNG REQUIRED)

set(JPEG_ROOT ${LIBDIR}/jpeg)
find_package(JPEG REQUIRED)

set(TIFF_ROOT ${LIBDIR}/tiff)
find_package(TIFF REQUIRED)

if(WITH_IMAGE_WEBP)
  set(WEBP_ROOT_DIR ${LIBDIR}/webp)
  find_package(WebP REQUIRED)
endif()

# With Blender 4.4 libraries there is no more Boost. This code is only
# here until we can reasonably assume everyone has upgraded to them.
if(WITH_BOOST)
  if(DEFINED LIBDIR AND NOT EXISTS "${LIBDIR}/boost")
    set(WITH_BOOST OFF)
    set(BOOST_LIBRARIES)
    set(BOOST_PYTHON_LIBRARIES)
    set(BOOST_INCLUDE_DIR)
  endif()
endif()

if(WITH_BOOST)
  set(Boost_NO_BOOST_CMAKE ON)
  set(Boost_ROOT ${LIBDIR}/boost)
  set(Boost_NO_SYSTEM_PATHS ON)
  set(_boost_FIND_COMPONENTS)
  if(WITH_USD AND USD_PYTHON_SUPPORT)
    list(APPEND _boost_FIND_COMPONENTS python${PYTHON_VERSION_NO_DOTS})
  endif()
  set(Boost_NO_WARN_NEW_VERSIONS ON)
  find_package(Boost COMPONENTS ${_boost_FIND_COMPONENTS})

  # Boost Python is the only library Blender directly depends on, though USD headers.
  if(WITH_USD AND USD_PYTHON_SUPPORT)
    set(BOOST_PYTHON_LIBRARIES ${Boost_PYTHON${PYTHON_VERSION_NO_DOTS}_LIBRARY})
  endif()
  set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
  set(BOOST_DEFINITIONS)

  mark_as_advanced(Boost_LIBRARIES)
  mark_as_advanced(Boost_INCLUDE_DIRS)
  unset(_boost_FIND_COMPONENTS)
endif()
add_bundled_libraries(boost/lib)

if(WITH_CODEC_FFMPEG)
  string(APPEND PLATFORM_LINKFLAGS " -liconv") # ffmpeg needs it !
endif()

if(WITH_PUGIXML)
  find_package(PugiXML REQUIRED)
endif()

find_package(OpenImageIO REQUIRED)
add_bundled_libraries(openimageio/lib)

if(WITH_OPENCOLORIO)
  find_package(OpenColorIO 2.0.0 REQUIRED)
endif()
add_bundled_libraries(opencolorio/lib)

if(WITH_OPENVDB)
  find_package(OpenVDB)
  if(WITH_APPLE_CROSSPLATFORM)
    # Use static library for iOS.
    find_library(BLOSC_LIBRARIES NAMES libblosc.a HINTS ${LIBDIR}/openvdb/lib)
  else()
    find_library(BLOSC_LIBRARIES NAMES blosc HINTS ${LIBDIR}/openvdb/lib)
  endif()
  if(BLOSC_LIBRARIES)
    list(APPEND OPENVDB_LIBRARIES ${BLOSC_LIBRARIES})
  else()
    unset(BLOSC_LIBRARIES CACHE)
  endif()
  set(OPENVDB_DEFINITIONS)
endif()
add_bundled_libraries(openvdb/lib)

if(WITH_NANOVDB)
  find_package(NanoVDB)
endif()

if(WITH_CPU_SIMD AND SUPPORT_NEON_BUILD)
  find_package(sse2neon)
endif()

if(WITH_LLVM)
  find_package(LLVM)
  if(NOT LLVM_FOUND)
    message(FATAL_ERROR "LLVM not found.")
  endif()
  if(WITH_CLANG)
    find_package(Clang)
    if(NOT CLANG_FOUND)
      message(FATAL_ERROR "Clang not found.")
    endif()
  endif()

endif()

if(WITH_CYCLES AND WITH_CYCLES_OSL)
  find_package(OSL 1.13.4 REQUIRED)
endif()
add_bundled_libraries(osl/lib)

if(WITH_CYCLES AND WITH_CYCLES_EMBREE)
  find_package(Embree 3.8.0 REQUIRED)
endif()
add_bundled_libraries(embree/lib)

if(WITH_OPENIMAGEDENOISE)
  find_package(OpenImageDenoise REQUIRED)
  add_bundled_libraries(openimagedenoise/lib)
endif()

if(WITH_TBB)
  find_package(TBB REQUIRED)
  if(TBB_FOUND)
    get_target_property(TBB_LIBRARIES TBB::tbb LOCATION)
    get_target_property(TBB_INCLUDE_DIRS TBB::tbb INTERFACE_INCLUDE_DIRECTORIES)
  endif()
  set_and_warn_library_found("TBB" TBB_FOUND WITH_TBB)
 endif()
add_bundled_libraries(tbb/lib)

if(WITH_POTRACE)
  find_package(Potrace REQUIRED)
endif()

# CMake FindOpenMP doesn't know about AppleClang before 3.12, so provide custom flags.
if(WITH_OPENMP)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # Use OpenMP from our precompiled libraries.
    message(STATUS "Using ${LIBDIR}/openmp for OpenMP")
    set(OPENMP_CUSTOM ON)
    set(OPENMP_FOUND ON)
    set(OpenMP_C_FLAGS "-Xclang -fopenmp -I'${LIBDIR}/openmp/include'")
    set(OpenMP_CXX_FLAGS "-Xclang -fopenmp -I'${LIBDIR}/openmp/include'")
    set(OpenMP_LIBRARY_DIR "${LIBDIR}/openmp/lib/")
    set(OpenMP_LINKER_FLAGS "-L'${OpenMP_LIBRARY_DIR}' -lomp")
    set(OpenMP_LIBRARY "${OpenMP_LIBRARY_DIR}/libomp.dylib")
  endif()
endif()
add_bundled_libraries(openmp/lib)

if(WITH_XR_OPENXR)
  find_package(XR_OpenXR_SDK REQUIRED)
endif()

if(WITH_GMP)
  find_package(GMP REQUIRED)
endif()

if(WITH_HARU)
  find_package(Haru REQUIRED)
endif()

if(WITH_MANIFOLD)
  find_package(manifold REQUIRED)
endif()

if(WITH_OPENAL)
  set(OpenAL_ROOT ${LIBDIR}/openal)
  find_package(OpenAL REQUIRED)
endif()

if(WITH_CYCLES AND WITH_CYCLES_PATH_GUIDING)
  find_package(openpgl QUIET)
  if(openpgl_FOUND)
    get_target_property(OPENPGL_LIBRARIES openpgl::openpgl LOCATION)
    get_target_property(OPENPGL_INCLUDE_DIR openpgl::openpgl INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Found OpenPGL: ${OPENPGL_LIBRARIES}")
  else()
    set(WITH_CYCLES_PATH_GUIDING OFF)
    message(STATUS "OpenPGL not found, disabling WITH_CYCLES_PATH_GUIDING")
  endif()
endif()

set(ZSTD_ROOT_DIR ${LIBDIR}/zstd)
find_package(Zstd REQUIRED)

if(EXISTS ${LIBDIR})
  without_system_libs_end()
endif()

# Restore to default.
set(CMAKE_FIND_FRAMEWORK FIRST)

# ---------------------------------------------------------------------
# Set compiler and linker flags.

set(EXETYPE MACOSX_BUNDLE)

set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" OR CMAKE_OSX_ARCHITECTURES MATCHES "i386")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -mdynamic-no-pic -msse -msse2 -msse3 -mssse3")
  set(CMAKE_C_FLAGS_RELEASE "-O2 -mdynamic-no-pic  -msse -msse2 -msse3 -mssse3")
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    string(APPEND CMAKE_C_FLAGS_RELEASE " -ftree-vectorize  -fvariable-expansion-in-unroller")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -ftree-vectorize  -fvariable-expansion-in-unroller")
  endif()
else()
  set(CMAKE_C_FLAGS_RELEASE "-O2 -mdynamic-no-pic")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -mdynamic-no-pic")
endif()

# Clang has too low template depth of 128 for libmv.
string(APPEND CMAKE_CXX_FLAGS " -ftemplate-depth=1024")

# Avoid conflicts with Luxrender, and other plug-ins that may use the same
# libraries as Blender with a different version or build options.
set(PLATFORM_SYMBOLS_MAP ${CMAKE_SOURCE_DIR}/source/creator/symbols_apple.map)
string(APPEND PLATFORM_LINKFLAGS
  " -Wl,-unexported_symbols_list,'${PLATFORM_SYMBOLS_MAP}'"
)

if(${XCODE_VERSION} VERSION_GREATER_EQUAL 15.0)
  if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64" AND WITH_LEGACY_MACOS_X64_LINKER)
    # Silence "no platform load command found in <static library>, assuming: macOS".
    #
    # NOTE: Using ld_classic costs minutes of extra linking time.
    string(APPEND PLATFORM_LINKFLAGS " -Wl,-ld_classic")
  else()
    # Silence "ld: warning: ignoring duplicate libraries".
    #
    # The warning is introduced with Xcode 15 and is triggered when the same library
    # is passed to the linker ultiple times. This situation could happen with either
    # cyclic libraries, or some transitive dependencies where CMake might decide to
    # pass library to the linker multiple times to force it re-scan symbols. It is
    # not neeed for Xcode linker to ensure all symbols from library are used and it
    # is corrected in CMake 3.29:
    #    https://gitlab.kitware.com/cmake/cmake/-/issues/25297
    string(APPEND PLATFORM_LINKFLAGS " -Xlinker -no_warn_duplicate_libraries")
  endif()
endif()

# Make stack size more similar to Embree, required for Embree.
string(APPEND PLATFORM_LINKFLAGS_EXECUTABLE " -Wl,-stack_size,0x100000")

# Suppress ranlib "has no symbols" warnings (workaround for #48250).
set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
# llvm-ranlib doesn't support this flag. Xcode's libtool does.
if(NOT ${CMAKE_RANLIB} MATCHES ".*llvm-ranlib$")
  set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
  set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

if(WITH_COMPILER_CCACHE)
  if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    find_program(CCACHE_PROGRAM ccache)
    mark_as_advanced(CCACHE_PROGRAM)
    if(CCACHE_PROGRAM)
      # Makefiles and ninja
      set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}" CACHE STRING "" FORCE)
      set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "" FORCE)
      mark_as_advanced(
        CMAKE_C_COMPILER_LAUNCHER
        CMAKE_CXX_COMPILER_LAUNCHER
      )
    else()
      message(WARNING "Ccache NOT found, disabling WITH_COMPILER_CCACHE")
      set(WITH_COMPILER_CCACHE OFF)
    endif()
  endif()
endif()

if(WITH_COMPILER_ASAN)
  list(APPEND PLATFORM_BUNDLED_LIBRARIES ${COMPILER_ASAN_LIBRARY})
endif()

if(PLATFORM_BUNDLED_LIBRARIES)
  # For the installed Python module and installed Blender executable, we set the
  # rpath to the location where install step will copy the shared libraries.
  set(CMAKE_SKIP_INSTALL_RPATH FALSE)
  if(WITH_PYTHON_MODULE OR WITH_APPLE_CROSSPLATFORM)
    list(APPEND CMAKE_INSTALL_RPATH "@loader_path/Assets/lib")
  else()
    list(APPEND CMAKE_INSTALL_RPATH "@loader_path/../Resources/lib")
  endif()

  # For binaries that are built but not installed (like makesdan or tests), we add
  # the original directory of all shared libraries to the rpath. This is needed because
  # these can be in different folders, and because the build and install folder may be
  # different.
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  list(APPEND CMAKE_BUILD_RPATH ${PLATFORM_BUNDLED_LIBRARY_DIRS})

  # Environment variables to run precompiled executables that needed libraries.
  list(JOIN PLATFORM_BUNDLED_LIBRARY_DIRS ":" _library_paths)
  # Intentionally double "$$" which expands into "$" when instantiated.
  set(PLATFORM_ENV_BUILD "DYLD_LIBRARY_PATH=\"${_library_paths};$$DYLD_LIBRARY_PATH\"")
  if(WITH_APPLE_CROSSPLATFORM)
    set(PLATFORM_ENV_INSTALL "DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/Blender.app/Assets/lib/;$DYLD_LIBRARY_PATH")
  else()
    set(PLATFORM_ENV_INSTALL "DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/Blender.app/Contents/Resources/lib/;$$DYLD_LIBRARY_PATH")
  endif()
  unset(_library_paths)
endif()

# Same as `CFBundleIdentifier` in Info.plist.
# IOS_FIXME: Change 'test' back to 'blenderfoundation' before release
# set(CMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.blenderfoundation.blender")

if(WITH_APPLE_CROSSPLATFORM)
  if(APPLE_TARGET_DEVICE STREQUAL "ios")
    set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    set(CMAKE_XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST NO)
    set(CMAKE_XCODE_ATTRIBUTE_SUPPORTS_MAC_DESIGNED_FOR_IPHONE_IPAD NO)
  endif()
  # Entitlements file reference
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/release/${APPLE_TARGET_DEVICE}/entitlements.plist")
endif()
