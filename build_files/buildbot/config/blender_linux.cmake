# ######## Global feature set settings ########

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")

message(STATUS "Building in CentOS 7 64bit environment")
set(LIBDIR_NAME "linux_centos7_x86_64")

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

set(LIBDIR "${CMAKE_CURRENT_LIST_DIR}/../../../../lib/${LIBDIR_NAME}" CACHE STRING "" FORCE)

# Platform specific configuration, to ensure static linking against everything.

set(Boost_USE_STATIC_LIBS    ON CACHE BOOL "" FORCE)

# We need to link OpenCOLLADA against PCRE library. Even though it is not installed
# on /usr, we do not really care -- all we care is PCRE_FOUND be TRUE and its
# library pointing to a valid one.
set(PCRE_INCLUDE_DIR          "/usr/include"                        CACHE STRING "" FORCE)
set(PCRE_LIBRARY              "${LIBDIR}/opencollada/lib/libpcre.a" CACHE STRING "" FORCE)

# Additional linking libraries
set(CMAKE_EXE_LINKER_FLAGS   "-lrt -static-libstdc++ -no-pie"  CACHE STRING "" FORCE)
