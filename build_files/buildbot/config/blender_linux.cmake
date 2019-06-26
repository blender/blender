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

# Additional linking libraries
set(CMAKE_EXE_LINKER_FLAGS   "-lrt -static-libstdc++ -no-pie"  CACHE STRING "" FORCE)
