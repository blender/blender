# ######## Global feature set settings ########

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")

# Detect which libc we'll be linking against.
# Some of the paths will depend on this

if(EXISTS "/lib/x86_64-linux-gnu/libc-2.24.so")
  message(STATUS "Building in GLibc-2.24 environment")
  set(GLIBC "2.24")
  set(MULTILIB "/x86_64-linux-gnu")
  set(LIBDIR_NAME "linux_x86_64")
elseif(EXISTS "/lib/i386-linux-gnu//libc-2.24.so")
  message(STATUS "Building in GLibc-2.24 environment")
  set(GLIBC "2.24")
  set(MULTILIB "/i386-linux-gnu")
  set(LIBDIR_NAME "linux_i686")
elseif(EXISTS "/lib/x86_64-linux-gnu/libc-2.19.so")
  message(STATUS "Building in GLibc-2.19 environment")
  set(GLIBC "2.19")
  set(MULTILIB "/x86_64-linux-gnu")
elseif(EXISTS "/lib/i386-linux-gnu//libc-2.19.so")
  message(STATUS "Building in GLibc-2.19 environment")
  set(GLIBC "2.19")
  set(MULTILIB "/i386-linux-gnu")
elseif(EXISTS "/lib/libc-2.11.3.so")
  message(STATUS "Building in GLibc-2.11 environment")
  set(GLIBC "2.11")
  set(MULTILIB "")
else()
  message(FATAL_ERROR "Unknown build environment")
endif()

# Default to only build Blender
set(WITH_BLENDER             ON  CACHE BOOL "" FORCE)

# ######## Linux-specific build options ########
# Options which are specific to Linux-only platforms
set(WITH_DOC_MANPAGE         OFF CACHE BOOL "" FORCE)

# ######## Official release-specific build options ########
# Options which are specific to Linux release builds only
set(WITH_JACK_DYNLOAD        ON  CACHE BOOL "" FORCE)
set(WITH_SDL_DYNLOAD         ON  CACHE BOOL "" FORCE)
set(WITH_SYSTEM_GLEW         OFF CACHE BOOL "" FORCE)

set(WITH_OPENMP_STATIC       ON  CACHE BOOL "" FORCE)

set(WITH_PYTHON_INSTALL_NUMPY    ON CACHE BOOL "" FORCE)
set(WITH_PYTHON_INSTALL_REQUESTS ON CACHE BOOL "" FORCE)

# ######## Release environment specific settings ########

if(NOT ${GLIBC} STREQUAL "2.24")

  # All the hardcoded library paths and such

  # LLVM libraries
  set(LLVM_VERSION             "3.4"  CACHE STRING "" FORCE)
  set(LLVM_ROOT_DIR            "/opt/lib/llvm-${LLVM_VERSION}"  CACHE STRING "" FORCE)
  set(LLVM_STATIC              ON  CACHE BOOL "" FORCE)

  # BOOST libraries
  set(BOOST_ROOT               "/opt/lib/boost" CACHE STRING "" FORCE)
  set(Boost_USE_STATIC_LIBS    ON CACHE BOOL "" FORCE)

  # FFmpeg libraries
  set(FFMPEG                   "/opt/lib/ffmpeg" CACHE STRING "" FORCE)
  set(FFMPEG_LIBRARIES
    avdevice avformat avcodec avutil avfilter swscale swresample
    /usr/lib${MULTILIB}/libxvidcore.a
    /usr/lib${MULTILIB}/libx264.a
    /usr/lib${MULTILIB}/libmp3lame.a
    /usr/lib${MULTILIB}/libvpx.a
    /usr/lib${MULTILIB}/libvorbis.a
    /usr/lib${MULTILIB}/libogg.a
    /usr/lib${MULTILIB}/libvorbisenc.a
    /usr/lib${MULTILIB}/libtheora.a
    /usr/lib${MULTILIB}/libschroedinger-1.0.a
    /usr/lib${MULTILIB}/liborc-0.4.a
    CACHE STRING "" FORCE
  )

  # SndFile libraries
  set(SNDFILE_LIBRARY          "/usr/lib${MULTILIB}/libsndfile.a;/usr/lib${MULTILIB}/libFLAC.a" CACHE STRING "" FORCE)

  # OpenAL libraries
  set(OPENAL_ROOT_DIR           "/opt/lib/openal" CACHE STRING "" FORCE)
  set(OPENAL_INCLUDE_DIR        "${OPENAL_ROOT_DIR}/include/AL" CACHE STRING "" FORCE)
  set(OPENAL_LIBRARY
    ${OPENAL_ROOT_DIR}/lib/libopenal.a
    ${OPENAL_ROOT_DIR}/lib/libcommon.a
    CACHE STRING "" FORCE
  )

  # OpenCollada libraries
  set(OPENCOLLADA_UTF_LIBRARY   ""                              CACHE STRING "" FORCE)
  set(PCRE_INCLUDE_DIR          "/usr/include"                  CACHE STRING "" FORCE)
  set(PCRE_LIBRARY              "/usr/lib${MULTILIB}/libpcre.a" CACHE STRING "" FORCE)
  set(XML2_INCLUDE_DIR          "/usr/include"                  CACHE STRING "" FORCE)
  set(XML2_LIBRARY              "/usr/lib${MULTILIB}/libxml2.a" CACHE STRING "" FORCE)

  # OpenColorIO libraries
  set(OPENCOLORIO_ROOT_DIR      "/opt/lib/ocio" CACHE STRING "" FORCE)
  set(OPENCOLORIO_OPENCOLORIO_LIBRARY "${OPENCOLORIO_ROOT_DIR}/lib/libOpenColorIO.a" CACHE STRING "" FORCE)
  set(OPENCOLORIO_TINYXML_LIBRARY "${OPENCOLORIO_ROOT_DIR}/lib/libtinyxml.a"         CACHE STRING "" FORCE)
  set(OPENCOLORIO_YAML-CPP_LIBRARY "${OPENCOLORIO_ROOT_DIR}/lib/libyaml-cpp.a"       CACHE STRING "" FORCE)

  # Freetype
  set(FREETYPE_INCLUDE_DIRS "/usr/include/freetype2"       CACHE STRING "" FORCE)
  set(FREETYPE_LIBRARY "/usr/lib${MULTILIB}/libfreetype.a" CACHE STRING "" FORCE)

  # OpenImageIO
  if(GLIBC EQUAL "2.19")
    set(OPENIMAGEIO_LIBRARY
      /opt/lib/oiio/lib/libOpenImageIO.a
      /opt/lib/oiio/lib/libOpenImageIO_Util.a
      /usr/lib${MULTILIB}/libwebp.a
      /usr/lib${MULTILIB}/liblzma.a
      /usr/lib${MULTILIB}/libjbig.a
      ${FREETYPE_LIBRARY}
      CACHE STRING "" FORCE
    )
  endif()

  # OpenSubdiv libraries
  set(OPENSUBDIV_ROOT_DIR "/opt/lib/opensubdiv" CACHE STRING "" FORCE)
  set(OPENSUBDIV_OSDCPU_LIBRARY "${OPENSUBDIV_ROOT_DIR}/lib/libosdCPU.a" CACHE STRING "" FORCE)
  set(OPENSUBDIV_OSDGPU_LIBRARY "${OPENSUBDIV_ROOT_DIR}/lib/libosdGPU.a" CACHE STRING "" FORCE)

  # OpenEXR libraries
  set(OPENEXR_ROOT_DIR          "/opt/lib/openexr"                    CACHE STRING "" FORCE)
  set(OPENEXR_HALF_LIBRARY      "/opt/lib/openexr/lib/libHalf.a"      CACHE STRING "" FORCE)
  set(OPENEXR_IEX_LIBRARY       "/opt/lib/openexr/lib/libIex.a"       CACHE STRING "" FORCE)
  set(OPENEXR_ILMIMF_LIBRARY    "/opt/lib/openexr/lib/libIlmImf.a"    CACHE STRING "" FORCE)
  set(OPENEXR_ILMTHREAD_LIBRARY "/opt/lib/openexr/lib/libIlmThread.a" CACHE STRING "" FORCE)
  set(OPENEXR_IMATH_LIBRARY     "/opt/lib/openexr/lib/libImath.a"     CACHE STRING "" FORCE)

  # JeMalloc library
  set(JEMALLOC_LIBRARY    "/opt/lib/jemalloc/lib/libjemalloc.a" CACHE STRING "" FORCE)

  # Space navigation
  set(SPACENAV_ROOT_DIR       "/opt/lib/libspnav" CACHE STRING "" FORCE)

  # Force some system libraries to be static
  set(FFTW3_LIBRARY       "/usr/lib${MULTILIB}/libfftw3.a" CACHE STRING "" FORCE)
  set(JPEG_LIBRARY        "/usr/lib${MULTILIB}/libjpeg.a"  CACHE STRING "" FORCE)
  set(PNG_LIBRARY         "/usr/lib${MULTILIB}/libpng.a"   CACHE STRING "" FORCE)
  set(TIFF_LIBRARY        "/usr/lib${MULTILIB}/libtiff.a"  CACHE STRING "" FORCE)
  set(ZLIB_LIBRARY        "/usr/lib${MULTILIB}/libz.a"     CACHE STRING "" FORCE)

  # OpenVDB
  set(OPENVDB_LIBRARY
    /opt/lib/openvdb/lib/libopenvdb.a
    CACHE BOOL "" FORCE
  )

  set(BLOSC_LIBRARY
    /opt/lib/blosc/lib/libblosc.a
    CACHE BOOL "" FORCE
  )

else()

  set(LIBDIR "/opt/blender-deps/${LIBDIR_NAME}" CACHE BOOL "" FORCE)

  # TODO(sergey): Remove once Python is oficially bumped to 3.7.
  set(PYTHON_VERSION    3.7 CACHE BOOL "" FORCE)

  # Platform specific configuration, to ensure static linking against everything.

  set(Boost_USE_STATIC_LIBS    ON CACHE BOOL "" FORCE)

  # We need to link OpenCOLLADA against PCRE library. Even though it is not installed
  # on /usr, we do not really care -- all we care is PCRE_FOUND be TRUE and its
  # library pointing to a valid one.
  set(PCRE_INCLUDE_DIR          "/usr/include"                        CACHE STRING "" FORCE)
  set(PCRE_LIBRARY              "${LIBDIR}/opencollada/lib/libpcre.a" CACHE STRING "" FORCE)

endif()

# Additional linking libraries
set(CMAKE_EXE_LINKER_FLAGS   "-lrt -static-libstdc++ -no-pie"  CACHE STRING "" FORCE)
