# - Try to find audaspace
# Once done, this will define
#
#  AUDASPACE_FOUND - system has audaspace
#  AUDASPACE_INCLUDE_DIRS - the audaspace include directories
#  AUDASPACE_LIBRARIES - link these to use audaspace
#  AUDASPACE_C_FOUND - system has audaspace's C binding
#  AUDASPACE_C_INCLUDE_DIRS - the audaspace's C binding include directories
#  AUDASPACE_C_LIBRARIES - link these to use audaspace's C binding
#  AUDASPACE_PY_FOUND - system has audaspace's python binding
#  AUDASPACE_PY_INCLUDE_DIRS - the audaspace's python binding include directories
#  AUDASPACE_PY_LIBRARIES - link these to use audaspace's python binding

IF(NOT AUDASPACE_ROOT_DIR AND NOT $ENV{AUDASPACE_ROOT_DIR} STREQUAL "")
	SET(AUDASPACE_ROOT_DIR $ENV{AUDASPACE_ROOT_DIR})
ENDIF()

SET(_audaspace_SEARCH_DIRS
	${AUDASPACE_ROOT_DIR}
	/usr/local
	/sw # Fink
	/opt/local # DarwinPorts
	/opt/csw # Blastwave
)

# Use pkg-config to get hints about paths
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(AUDASPACE_PKGCONF audaspace)
endif(PKG_CONFIG_FOUND)

# Include dir
find_path(AUDASPACE_INCLUDE_DIR
	NAMES ISound.h
	HINTS ${_audaspace_SEARCH_DIRS}
	PATHS ${AUDASPACE_PKGCONF_INCLUDE_DIRS}
)

# Library
find_library(AUDASPACE_LIBRARY
	NAMES audaspace
	HINTS ${_audaspace_SEARCH_DIRS}
	PATHS ${AUDASPACE_PKGCONF_LIBRARY_DIRS}
)

# Include dir
find_path(AUDASPACE_C_INCLUDE_DIR
	NAMES AUD_Sound.h
	HINTS ${_audaspace_SEARCH_DIRS}
	PATHS ${AUDASPACE_PKGCONF_INCLUDE_DIRS}
)

# Library
find_library(AUDASPACE_C_LIBRARY
	NAMES audaspace-c
	HINTS ${_audaspace_SEARCH_DIRS}
	PATHS ${AUDASPACE_PKGCONF_LIBRARY_DIRS}
)

# Include dir
find_path(AUDASPACE_PY_INCLUDE_DIR
	NAMES python/PyAPI.h
	HINTS ${_audaspace_SEARCH_DIRS}
	PATHS ${AUDASPACE_PKGCONF_INCLUDE_DIRS}
)

# Library
find_library(AUDASPACE_PY_LIBRARY
	NAMES audaspace-py
	HINTS ${_audaspace_SEARCH_DIRS}
	PATHS ${AUDASPACE_PKGCONF_LIBRARY_DIRS}
)

find_package(PackageHandleStandardArgs)
find_package_handle_standard_args(Audaspace  DEFAULT_MSG  AUDASPACE_LIBRARY AUDASPACE_INCLUDE_DIR)
find_package_handle_standard_args(Audaspace_C  DEFAULT_MSG  AUDASPACE_C_LIBRARY AUDASPACE_C_INCLUDE_DIR)
find_package_handle_standard_args(Audaspace_Py  DEFAULT_MSG  AUDASPACE_PY_LIBRARY AUDASPACE_PY_INCLUDE_DIR)

if(AUDASPACE_FOUND)
	set(AUDASPACE_LIBRARIES ${AUDASPACE_LIBRARY})
	set(AUDASPACE_INCLUDE_DIRS ${AUDASPACE_INCLUDE_DIR})
endif(AUDASPACE_FOUND)

if(AUDASPACE_C_FOUND)
	set(AUDASPACE_C_LIBRARIES ${AUDASPACE_C_LIBRARY})
	set(AUDASPACE_C_INCLUDE_DIRS ${AUDASPACE_C_INCLUDE_DIR})
endif(AUDASPACE_C_FOUND)

if(AUDASPACE_PY_FOUND)
	set(AUDASPACE_PY_LIBRARIES ${AUDASPACE_PY_LIBRARY})
	set(AUDASPACE_PY_INCLUDE_DIRS ${AUDASPACE_PY_INCLUDE_DIR})
endif(AUDASPACE_PY_FOUND)

mark_as_advanced(
	AUDASPACE_LIBRARY
	AUDASPACE_LIBRARIES
	AUDASPACE_INCLUDE_DIR
	AUDASPACE_INCLUDE_DIRS
	AUDASPACE_C_LIBRARY
	AUDASPACE_C_LIBRARIES
	AUDASPACE_C_INCLUDE_DIR
	AUDASPACE_C_INCLUDE_DIRS
	AUDASPACE_PY_LIBRARY
	AUDASPACE_PY_LIBRARIES
	AUDASPACE_PY_INCLUDE_DIR
	AUDASPACE_PY_INCLUDE_DIRS
)
