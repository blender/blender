# ######## Global feature set settings ########

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")

# For libc-2.24 we are using chroot which runs on a 64bit system.
# There we can not use CPU bitness check since it is always 64bit. So instead
# we check for a specific libraries.
#
# Other builders we are runnign in a bare virtual machine, and the libraries
# are installed to /opt/.
# We assume that only 64bit builders exists in such configuration.
if(EXISTS "/lib/x86_64-linux-gnu/libc-2.24.so")
  message(STATUS "Building in GLibc-2.24 environment")
  set(LIBDIR_NAME "linux_x86_64")
elseif(EXISTS "/lib/i386-linux-gnu//libc-2.24.so")
  message(STATUS "Building in GLibc-2.24 environment")
  set(LIBDIR_NAME "linux_i686")
else()
  message(STATUS "Building in generic 64bit environment")
  set(LIBDIR_NAME "linux_x86_64")
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

# Platform specific configuration, to ensure static linking against everything.

set(Boost_USE_STATIC_LIBS    ON CACHE BOOL "" FORCE)

# We need to link OpenCOLLADA against PCRE library. Even though it is not installed
# on /usr, we do not really care -- all we care is PCRE_FOUND be TRUE and its
# library pointing to a valid one.
set(PCRE_INCLUDE_DIR          "/usr/include"                        CACHE STRING "" FORCE)
set(PCRE_LIBRARY              "${LIBDIR}/opencollada/lib/libpcre.a" CACHE STRING "" FORCE)

# Additional linking libraries
set(CMAKE_EXE_LINKER_FLAGS   "-lrt -static-libstdc++ -no-pie"  CACHE STRING "" FORCE)
