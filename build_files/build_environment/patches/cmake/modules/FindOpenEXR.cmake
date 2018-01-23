# Module to find OpenEXR.
#
# This module will first look into the directories defined by the variables:
#   - OPENEXR_HOME, OPENEXR_VERSION, OPENEXR_LIB_AREA
#
# It also supports non-standard names for the library components.
#
# To use a custom OpenEXR
#   - Set the variable OPENEXR_CUSTOM to True
#   - Set the variable OPENEXR_CUSTOM_LIBRARY to the name of the library to
#     use, e.g. "SpiIlmImf"
#   - Optionally set the variable OPENEXR_CUSTOM_INCLUDE_DIR to any
#     particularly weird place that the OpenEXR/*.h files may be found
#   - Optionally set the variable OPENEXR_CUSTOM_LIB_DIR to any
#     particularly weird place that the libraries files may be found
#
# This module defines the following variables:
#
# OPENEXR_INCLUDE_DIR - where to find ImfRgbaFile.h, OpenEXRConfig, etc.
# OPENEXR_LIBRARIES   - list of libraries to link against when using OpenEXR.
#                       This list does NOT include the IlmBase libraries.
#                       These are defined by the FindIlmBase module.
# OPENEXR_FOUND       - True if OpenEXR was found.

# Other standarnd issue macros
include(SelectLibraryConfigurations)
include(FindPackageHandleStandardArgs)
include(FindPackageMessage)

if(OPENEXR_USE_STATIC_LIBS)
  set(_openexr_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  if(WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a )
  endif()
endif()

# Macro to assemble a helper state variable
macro(SET_STATE_VAR varname)
  set(tmp_lst
    ${OPENEXR_CUSTOM} | ${OPENEXR_CUSTOM_LIBRARY} |
    ${OPENEXR_HOME} | ${OPENEXR_VERSION} | ${OPENEXR_LIB_AREA}
  )
  set(${varname} "${tmp_lst}")
  unset(tmp_lst)
endmacro()

# To enforce that find_* functions do not use inadvertently existing versions
if(OPENEXR_CUSTOM)
  set(OPENEXR_FIND_OPTIONS "NO_DEFAULT_PATH")
endif()

# Macro to search for an include directory
macro(PREFIX_FIND_INCLUDE_DIR prefix includefile libpath_var)
  string(TOUPPER ${prefix}_INCLUDE_DIR tmp_varname)
  find_path(${tmp_varname} ${includefile}
    HINTS ${${libpath_var}}
    PATH_SUFFIXES include
    ${OPENEXR_FIND_OPTIONS}
  )
  if(${tmp_varname})
    mark_as_advanced(${tmp_varname})
  endif()
  unset(tmp_varname)
endmacro()


# Macro to search for the given library and adds the cached
# variable names to the specified list
macro(PREFIX_FIND_LIB prefix libname libpath_var liblist_var cachelist_var)
  string(TOUPPER ${prefix}_${libname} tmp_prefix)
  # Handle new library names for OpenEXR 2.1 build via cmake
  string(REPLACE "." "_" _ILMBASE_VERSION ${ILMBASE_VERSION})
  string(SUBSTRING ${_ILMBASE_VERSION} 0 3 _ILMBASE_VERSION )
  find_library(${tmp_prefix}_LIBRARY_RELEASE
    NAMES ${libname} ${libname}-${_ILMBASE_VERSION}
    HINTS ${${libpath_var}}
    PATH_SUFFIXES lib
    ${OPENEXR_FIND_OPTIONS}
  )
  find_library(${tmp_prefix}_LIBRARY_DEBUG
    NAMES ${libname}d ${libname}_d ${libname}debug ${libname}_debug
    HINTS ${${libpath_var}}
    PATH_SUFFIXES lib
    ${OPENEXR_FIND_OPTIONS}
  )
  # Properly define ${tmp_prefix}_LIBRARY (cached) and ${tmp_prefix}_LIBRARIES
  select_library_configurations(${tmp_prefix})
  list(APPEND ${liblist_var} ${tmp_prefix}_LIBRARIES)

  # Add to the list of variables which should be reset
  list(APPEND ${cachelist_var}
    ${tmp_prefix}_LIBRARY
    ${tmp_prefix}_LIBRARY_RELEASE
    ${tmp_prefix}_LIBRARY_DEBUG)
  mark_as_advanced(
    ${tmp_prefix}_LIBRARY
    ${tmp_prefix}_LIBRARY_RELEASE
    ${tmp_prefix}_LIBRARY_DEBUG)
  unset(tmp_prefix)
endmacro()


# Encode the current state of the external variables into a string
SET_STATE_VAR(OPENEXR_CURRENT_STATE)

# If the state has changed, clear the cached variables
if(OPENEXR_CACHED_STATE AND
    NOT OPENEXR_CACHED_STATE STREQUAL OPENEXR_CURRENT_STATE)
  foreach(libvar ${OPENEXR_CACHED_VARS})
    unset(${libvar} CACHE)
  endforeach()
endif()

# Generic search paths
set(OpenEXR_generic_include_paths
  ${OPENEXR_CUSTOM_INCLUDE_DIR}
  /usr/include
  /usr/include/${CMAKE_LIBRARY_ARCHITECTURE}
  /usr/local/include
  /sw/include
  /opt/local/include
)
set(OpenEXR_generic_library_paths
  ${OPENEXR_CUSTOM_LIB_DIR}
  /usr/lib
  /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
  /usr/local/lib
  /usr/local/lib/${CMAKE_LIBRARY_ARCHITECTURE}
  /sw/lib
  /opt/local/lib
)

# Search paths for the OpenEXR files
if(OPENEXR_HOME)
  set(OpenEXR_library_paths
    ${OPENEXR_HOME}/lib
    ${OPENEXR_HOME}/lib64)
  if(OPENEXR_VERSION)
    set(OpenEXR_include_paths
      ${OPENEXR_HOME}/openexr-${OPENEXR_VERSION}/include
      ${OPENEXR_HOME}/include/openexr-${OPENEXR_VERSION})
    list(APPEND OpenEXR_library_paths
      ${OPENEXR_HOME}/openexr-${OPENEXR_VERSION}/lib
      ${OPENEXR_HOME}/lib/openexr-${OPENEXR_VERSION})
  endif()
  list(APPEND OpenEXR_include_paths ${OPENEXR_HOME}/include)
  if(OPENEXR_LIB_AREA)
    list(INSERT OpenEXR_library_paths 2 ${OPENEXR_LIB_AREA})
  endif()
endif()
if(ILMBASE_HOME AND OPENEXR_VERSION)
  list(APPEND OpenEXR_include_paths
    ${ILMBASE_HOME}/include/openexr-${OPENEXR_VERSION})
endif()
list(APPEND OpenEXR_include_paths ${OpenEXR_generic_include_paths})
list(APPEND OpenEXR_library_paths ${OpenEXR_generic_library_paths})

# Locate the header files
PREFIX_FIND_INCLUDE_DIR(OpenEXR
  OpenEXR/ImfArray.h OpenEXR_include_paths)

if(OPENEXR_INCLUDE_DIR)
  # Get the version from config file, if not already set.
  if(NOT OPENEXR_VERSION)
    FILE(STRINGS "${OPENEXR_INCLUDE_DIR}/OpenEXR/OpenEXRConfig.h" OPENEXR_BUILD_SPECIFICATION
         REGEX "^[ \t]*#define[ \t]+OPENEXR_VERSION_STRING[ \t]+\"[.0-9]+\".*$")

    if(OPENEXR_BUILD_SPECIFICATION)
      if(NOT OpenEXR_FIND_QUIETLY)
        message(STATUS "${OPENEXR_BUILD_SPECIFICATION}")
      endif()
      string(REGEX REPLACE ".*#define[ \t]+OPENEXR_VERSION_STRING[ \t]+\"([.0-9]+)\".*"
             "\\1" XYZ ${OPENEXR_BUILD_SPECIFICATION})
      set("OPENEXR_VERSION" ${XYZ} CACHE STRING "Version of OpenEXR lib")
    else()
      # Old versions (before 2.0?) do not have any version string, just assuming 2.0 should be fine though. 
      message(WARNING "Could not determine ILMBase library version, assuming 2.0.")
      set("OPENEXR_VERSION" "2.0" CACHE STRING "Version of OpenEXR lib")
    endif()
  endif()
endif()

if(OPENEXR_CUSTOM)
  if(NOT OPENEXR_CUSTOM_LIBRARY)
    message(FATAL_ERROR "Custom OpenEXR library requested but OPENEXR_CUSTOM_LIBRARY is not set.")
  endif()
  set(OpenEXR_Library ${OPENEXR_CUSTOM_LIBRARY})
else()
#elseif(${OPENEXR_VERSION} VERSION_LESS "2.1")
  set(OpenEXR_Library IlmImf)
#else()
#  string(REGEX REPLACE "([0-9]+)[.]([0-9]+).*" "\\1_\\2" _openexr_libs_ver ${OPENEXR_VERSION})
#  set(OpenEXR_Library IlmImf-${_openexr_libs_ver})
endif()

# Locate the OpenEXR library
set(OpenEXR_libvars "")
set(OpenEXR_cachevars "")
PREFIX_FIND_LIB(OpenEXR ${OpenEXR_Library}
  OpenEXR_library_paths OpenEXR_libvars OpenEXR_cachevars)

# Create the list of variables that might need to be cleared
set(OPENEXR_CACHED_VARS
  OPENEXR_INCLUDE_DIR ${OpenEXR_cachevars}
  CACHE INTERNAL "Variables set by FindOpenEXR.cmake" FORCE)

# Store the current state so that variables might be cleared if required
set(OPENEXR_CACHED_STATE ${OPENEXR_CURRENT_STATE}
  CACHE INTERNAL "State last seen by FindOpenEXR.cmake" FORCE)

# Always link explicitly with zlib
set(OPENEXR_ZLIB ${ZLIB_LIBRARIES})

# Use the standard function to handle OPENEXR_FOUND
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenEXR DEFAULT_MSG
  OPENEXR_INCLUDE_DIR ${OpenEXR_libvars})

if(OPENEXR_FOUND)
  set(OPENEXR_LIBRARIES "")
  foreach(tmplib ${OpenEXR_libvars})
    list(APPEND OPENEXR_LIBRARIES ${${tmplib}})
  endforeach()
  list(APPEND OPENEXR_LIBRARIES ${ZLIB_LIBRARIES})
  if(NOT OpenEXR_FIND_QUIETLY)
    FIND_PACKAGE_MESSAGE(OPENEXR
      "Found OpenEXR: ${OPENEXR_LIBRARIES}"
      "[${OPENEXR_INCLUDE_DIR}][${OPENEXR_LIBRARIES}][${OPENEXR_CURRENT_STATE}]"
      )
  endif()
endif()

# Restore the original find library ordering
if( OPENEXR_USE_STATIC_LIBS )
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_openexr_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# Unset the helper variables to avoid pollution
unset(OPENEXR_CURRENT_STATE)
unset(OpenEXR_include_paths)
unset(OpenEXR_library_paths)
unset(OpenEXR_generic_include_paths)
unset(OpenEXR_generic_library_paths)
unset(OpenEXR_libvars)
unset(OpenEXR_cachevars)
