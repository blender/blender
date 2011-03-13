# - Find python libraries
#
#  PYTHON_VERSION
#  PYTHON_INCLUDE_DIRS
#  PYTHON_LIBRARY
#  PYTHON_LIBPATH
#  PYTHON_LINKFLAGS

#=============================================================================

set(PYTHON_VERSION 3.2 CACHE STRING "")
mark_as_advanced(PYTHON_VERSION)

set(PYTHON_LINKFLAGS "-Xlinker -export-dynamic")
mark_as_advanced(PYTHON_LINKFLAGS)

set(_Python_ABI_FLAGS
	"m;mu;u; ")

string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})
set(_Python_PATHS
  "$ENV{HOME}/py${_PYTHON_VERSION_NO_DOTS}" "/opt/py${_PYTHON_VERSION_NO_DOTS}" "/usr" "/usr/local")

if(NOT DEFINED PYTHON_INCLUDE_DIRS)
	message(STATUS "Looking for include Python.h")
	set(_Found_PYTHON_H OFF)

	foreach(_CURRENT_PATH ${_Python_PATHS})
		foreach(_CURRENT_ABI_FLAGS ${_Python_ABI_FLAGS})
			if(CMAKE_BUILD_TYPE STREQUAL Debug)
				set(_CURRENT_ABI_FLAGS "d${_CURRENT_ABI_FLAGS}")
			endif()
			string(REPLACE " " "" _CURRENT_ABI_FLAGS ${_CURRENT_ABI_FLAGS})

			set(_Python_HEADER "${_CURRENT_PATH}/include/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}/Python.h")

			if(EXISTS ${_Python_HEADER})
				message(STATUS "Checking for header: ${_Python_HEADER} - found")
				set(_Found_PYTHON_H ON)
				set(PYTHON ${_CURRENT_PATH})
				set(PYTHON_ABI_FLAGS ${_CURRENT_ABI_FLAGS})
				break()
			else()
				message(STATUS "Checking for header: ${_Python_HEADER}")
			endif()
		endforeach()
		
		if(_Found_PYTHON_H)
			break()
		endif()
	endforeach()

	if(NOT _Found_PYTHON_H)
		message(FATAL_ERROR "Python.h not found")
	endif()
endif()

#=============================================================================
# now the python versions are found


set(PYTHON_INCLUDE_DIRS "${PYTHON}/include/python${PYTHON_VERSION}${PYTHON_ABI_FLAGS}" CACHE STRING "")
mark_as_advanced(PYTHON_INCLUDE_DIRS)
set(PYTHON_LIBRARY "python${PYTHON_VERSION}${PYTHON_ABI_FLAGS}" CACHE STRING "")
mark_as_advanced(PYTHON_LIBRARY)
set(PYTHON_LIBPATH ${PYTHON}/lib CACHE STRING "")
mark_as_advanced(PYTHON_LIBPATH)
# set(PYTHON_BINARY ${PYTHON_EXECUTABLE} CACHE STRING "")

if(NOT EXISTS "${PYTHON_INCLUDE_DIRS}/Python.h")
	message(FATAL_ERROR " Missing python header: ${PYTHON_INCLUDE_DIRS}/Python.h")
endif()
