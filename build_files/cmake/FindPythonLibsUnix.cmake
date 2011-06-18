# - Find python libraries
#
#  PYTHON_VERSION
#  PYTHON_INCLUDE_DIRS
#  PYTHON_LIBRARY
#  PYTHON_LIBPATH
#  PYTHON_LINKFLAGS
#  PYTHON_ROOT_DIR, The base directory to search for Python.
#                   This can also be an environment variable.

#=============================================================================

# If PYTHON_ROOT_DIR was defined in the environment, use it.
IF(NOT PYTHON_ROOT_DIR AND NOT $ENV{PYTHON_ROOT_DIR} STREQUAL "")
  SET(PYTHON_ROOT_DIR $ENV{PYTHON_ROOT_DIR})
ENDIF()


set(PYTHON_VERSION 3.2 CACHE STRING "")
mark_as_advanced(PYTHON_VERSION)

set(PYTHON_LINKFLAGS "-Xlinker -export-dynamic")
mark_as_advanced(PYTHON_LINKFLAGS)

set(_python_ABI_FLAGS
	"m;mu;u; ")

string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})

set(_python_SEARCH_DIRS
	${PYTHON_ROOT_DIR}
	"$ENV{HOME}/py${_PYTHON_VERSION_NO_DOTS}"
	"/opt/py${_PYTHON_VERSION_NO_DOTS}"
	"/usr"
	"/usr/local"
)

if(NOT DEFINED PYTHON_INCLUDE_DIRS OR
   NOT DEFINED PYTHON_LIBRARY OR
   NOT DEFINED PYTHON_LIBPATH)
   
	message(STATUS "Looking for include Python.h")

	foreach(_CURRENT_ABI_FLAGS ${_python_ABI_FLAGS})
		if(CMAKE_BUILD_TYPE STREQUAL Debug)
			set(_CURRENT_ABI_FLAGS "d${_CURRENT_ABI_FLAGS}")
		endif()
		string(REPLACE " " "" _CURRENT_ABI_FLAGS ${_CURRENT_ABI_FLAGS})

		find_path(PYTHON_INCLUDE_DIR
			NAMES Python.h
			HINTS ${_python_SEARCH_DIRS}
			PATH_SUFFIXES include/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
		)

		find_library(PYTHON_LIBRARY
			NAMES "python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}"
			HINTS ${_python_SEARCH_DIRS}
			PATH_SUFFIXES lib64 lib
		)

		if((EXISTS ${PYTHON_LIBRARY}) AND (EXISTS ${PYTHON_INCLUDE_DIR}))
			message(STATUS "Checking for header: ${PYTHON_INCLUDE_DIR} - found")
			break()
		else()
			message(STATUS "Checking for header: ${PYTHON_INCLUDE_DIR}")
		endif()

		# ensure we dont find values from 2 different ABI versions
		unset(PYTHON_INCLUDE_DIR CACHE)
		unset(PYTHON_LIBRARY CACHE)
	endforeach()

	if((EXISTS ${PYTHON_LIBRARY}) AND (EXISTS ${PYTHON_INCLUDE_DIR}))
		# Assign cache items
		set(PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDE_DIR} CACHE STRING "")
		set(PYTHON_LIBRARY ${PYTHON_LIBRARY} CACHE STRING "")
		# not used
		# set(PYTHON_BINARY ${PYTHON_EXECUTABLE} CACHE STRING "")

		mark_as_advanced(
			PYTHON_INCLUDE_DIRS
			PYTHON_INCLUDE_DIR
			PYTHON_LIBRARY
		)
	else()
		message(FATAL_ERROR "Python not found")
	endif()
	
	unset(_CURRENT_ABI_FLAGS)
	unset(_CURRENT_PATH)
endif()

unset(_python_ABI_FLAGS)
unset(_python_SEARCH_DIRS)

#=============================================================================
# now the python versions are found


if(NOT EXISTS "${PYTHON_INCLUDE_DIRS}/Python.h")
	message(FATAL_ERROR " Missing python header: ${PYTHON_INCLUDE_DIRS}/Python.h")
endif()
