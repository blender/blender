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
if(NOT PYTHON_ROOT_DIR AND NOT $ENV{PYTHON_ROOT_DIR} STREQUAL "")
	set(PYTHON_ROOT_DIR $ENV{PYTHON_ROOT_DIR})
endif()


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
)

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
		break()
	else()
		# ensure we dont find values from 2 different ABI versions
		unset(PYTHON_INCLUDE_DIR CACHE)
		unset(PYTHON_LIBRARY CACHE)
	endif()
endforeach()

unset(_CURRENT_ABI_FLAGS)
unset(_CURRENT_PATH)

unset(_python_ABI_FLAGS)
unset(_python_SEARCH_DIRS)

# handle the QUIETLY and REQUIRED arguments and set PYTHONLIBSUNIX_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PythonLibsUnix  DEFAULT_MSG
    PYTHON_LIBRARY PYTHON_INCLUDE_DIR)


if(PYTHONLIBSUNIX_FOUND)
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
endif()
