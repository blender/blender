# ######## Global feature set settings ########

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_full.cmake")

# Default to only build Blender, not the player
set(WITH_BLENDER             ON  CACHE BOOL "" FORCE)
set(WITH_PLAYER              OFF CACHE BOOL "" FORCE)

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
# All the hardcoded libraru paths and such

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
    /usr/lib/libxvidcore.a
    /usr/lib/libx264.a
    /usr/lib/libmp3lame.a
    /usr/lib/libvpx.a
    /usr/lib/libvorbis.a
    /usr/lib/libogg.a
    /usr/lib/libvorbisenc.a
    /usr/lib/libtheora.a
    /usr/lib/libschroedinger-1.0.a
    /usr/lib/liborc-0.4.a
    CACHE STRING "" FORCE
)

# SndFile libraries
set(SNDFILE_LIBRARY          "/usr/lib/libsndfile.a;/usr/lib/libFLAC.a" CACHE STRING "" FORCE)

# OpenAL libraries
set(OPENAL_ROOT_DIR           "/opt/lib/openal" CACHE STRING "" FORCE)
set(OPENAL_INCLUDE_DIR        "${OPENAL_ROOT_DIR}/include" CACHE STRING "" FORCE)
set(OPENAL_LIBRARY
    ${OPENAL_ROOT_DIR}/lib/libopenal.a
    ${OPENAL_ROOT_DIR}/lib/libcommon.a
    CACHE STRING "" FORCE
)

# OpenCollada libraries
set(OPENCOLLADA_UTF_LIBRARY   ""                   CACHE STRING "" FORCE)
set(PCRE_INCLUDE_DIR          "/usr/include"       CACHE STRING "" FORCE)
set(PCRE_LIBRARY              "/usr/lib/libpcre.a" CACHE STRING "" FORCE)
set(XML2_INCLUDE_DIR          "/usr/include"       CACHE STRING "" FORCE)
set(XML2_LIBRARY              "/usr/lib/libxml2.a" CACHE STRING "" FORCE)

# OpenColorIO libraries
set(OPENCOLORIO_ROOT_DIR      "/opt/lib/ocio" CACHE STRING "" FORCE)
set(OPENCOLORIO_OPENCOLORIO_LIBRARY "${OPENCOLORIO_ROOT_DIR}/lib/libOpenColorIO.a" CACHE STRING "" FORCE)
set(OPENCOLORIO_TINYXML_LIBRARY "${OPENCOLORIO_ROOT_DIR}/lib/libtinyxml.a"         CACHE STRING "" FORCE)
set(OPENCOLORIO_YAML-CPP_LIBRARY "${OPENCOLORIO_ROOT_DIR}/lib/libyaml-cpp.a"       CACHE STRING "" FORCE)

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

# Foce some system libraries to be static
set(FFTW3_LIBRARY       "/usr/lib/libfftw3.a" CACHE STRING "" FORCE)
set(JPEG_LIBRARY        "/usr/lib/libjpeg.a"  CACHE STRING "" FORCE)
set(PNG_LIBRARY         "/usr/lib/libpng.a"   CACHE STRING "" FORCE)
set(TIFF_LIBRARY        "/usr/lib/libtiff.a"  CACHE STRING "" FORCE)
set(ZLIB_LIBRARY        "/usr/lib/libz.a"     CACHE STRING "" FORCE)

# Additional linking libraries
set(CMAKE_EXE_LINKER_FLAGS   "-lrt -static-libstdc++"  CACHE STRING "" FORCE)
