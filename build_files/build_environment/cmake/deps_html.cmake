# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# This outputs an html file with the name, version and homepage for
# every dependency we have, this is used by the release manager to
# update the licensing document.

set(HTMLCONTENTS)
get_cmake_property(_variableNames VARIABLES)
foreach(_variableName ${_variableNames})
  if(_variableName MATCHES "_URI$")
    string(REPLACE "_URI" "" DEP_NAME ${_variableName})
    set(DEP_VERSION "${${DEP_NAME}_VERSION}")
    # First see if DEP_HOMEPAGE is set, if it is use that.
    set(DEP_HOMEPAGE ${${DEP_NAME}_HOMEPAGE})
    if(NOT DEP_HOMEPAGE)
      # If the xxx_HOMEPAGE is not set but the URI for the archive is a known github format
      # extract the repository/project from the URI.
      string(REGEX MATCH "https:\/\/(.*)github\.com\/(.+)\/(archive|releases|release|tar.gz)\/(.*)" DEP_PROJECT "${${_variableName}}")
      if(CMAKE_MATCH_2)
        set(DEP_HOMEPAGE "https://www.github.com/${CMAKE_MATCH_2}")
      else() # If that is also not set, error out to ensure this information is supplied
        message(FATAL_ERROR "${DEP_NAME} No homepage set, please set ${DEP_NAME}_HOMEPAGE in versions.cmake")
      endif()
    endif()
    set(HTMLCONTENTS "${HTMLCONTENTS}<tr><td>${DEP_NAME}</td><td>${DEP_VERSION}</td><td><a href=\"${DEP_HOMEPAGE}\" target=\"_blank\">${DEP_HOMEPAGE}</a></td></tr>\n")
  endif()
endforeach()

configure_file(${CMAKE_SOURCE_DIR}/cmake/deps.html.in ${CMAKE_CURRENT_BINARY_DIR}/deps.html @ONLY)
