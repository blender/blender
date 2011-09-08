# - Try to find XML2
# Once done this will define
#
#  XML2_FOUND - system has XML2
#  XML2_INCLUDE_DIRS - the XML2 include directory
#  XML2_LIBRARIES - Link these to use XML2
#  XML2_DEFINITIONS - Compiler switches required for using XML2
#
#  Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#


if (XML2_LIBRARIES AND XML2_INCLUDE_DIRS)
  # in cache already
  set(XML2_FOUND TRUE)
else (XML2_LIBRARIES AND XML2_INCLUDE_DIRS)
  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  if (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
    include(UsePkgConfig)
    pkgconfig(libxml-2.0 _XML2_INCLUDEDIR _XML2_LIBDIR _XML2_LDFLAGS _XML2_CFLAGS)
  else (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(_XML2 libxml-2.0)
    endif (PKG_CONFIG_FOUND)
  endif (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
  find_path(XML2_INCLUDE_DIR
    NAMES
      libxml/xpath.h
    PATHS
      ${_XML2_INCLUDEDIR}
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
    PATH_SUFFIXES
      libxml2
  )

  find_library(XML2_LIBRARY
    NAMES
      xml2
    PATHS
      ${_XML2_LIBDIR}
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (XML2_LIBRARY)
    set(XML2_FOUND TRUE)
  endif (XML2_LIBRARY)

  set(XML2_INCLUDE_DIRS
    ${XML2_INCLUDE_DIR}
  )

  if (XML2_FOUND)
    set(XML2_LIBRARIES
      ${XML2_LIBRARIES}
      ${XML2_LIBRARY}
    )
  endif (XML2_FOUND)

  if (XML2_INCLUDE_DIRS AND XML2_LIBRARIES)
     set(XML2_FOUND TRUE)
  endif (XML2_INCLUDE_DIRS AND XML2_LIBRARIES)

  if (XML2_FOUND)
    if (NOT XML2_FIND_QUIETLY)
      message(STATUS "Found XML2: ${XML2_LIBRARIES}")
    endif (NOT XML2_FIND_QUIETLY)
  else (XML2_FOUND)
    if (XML2_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find XML2")
    endif (XML2_FIND_REQUIRED)
  endif (XML2_FOUND)

  # show the XML2_INCLUDE_DIRS and XML2_LIBRARIES variables only in the advanced view
  mark_as_advanced(XML2_INCLUDE_DIRS XML2_LIBRARIES)

endif (XML2_LIBRARIES AND XML2_INCLUDE_DIRS)

