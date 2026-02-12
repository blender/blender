# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# This outputs an markdown file with the name, version and homepage for
# every dependency we have, this is used by the release manager to
# update the licensing document.

# Create a table with name, version and url for each library.

set(TABLE_CONTENTS "")
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
    set(TABLE_CONTENTS "${TABLE_CONTENTS}| `${DEP_NAME}` | `${DEP_VERSION}` | <${DEP_HOMEPAGE}> |\n")
  endif()
endforeach()

# Create a list of dependencies for each library

set(DEPS_CONTENTS "")
get_property(targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)

foreach(target ${targets})
  string(REPLACE "external_" "" target_name ${target})
  get_target_property(deps ${target} MANUALLY_ADDED_DEPENDENCIES)
  if(NOT deps)
    list(APPEND NO_DEP_LIBS ${target_name})
    continue()
  endif()
  set(DEPS_CONTENTS "${DEPS_CONTENTS}```\n")
  set(DEPS_CONTENTS "${DEPS_CONTENTS}└── ${target_name}\n")
  list(LENGTH deps nr_of_deps)
  foreach(dep ${deps})
    string(REPLACE "external_" "" dep_name ${dep})
    math(EXPR nr_of_deps "${nr_of_deps} - 1")
    if(nr_of_deps)
      set(DEPS_CONTENTS "${DEPS_CONTENTS}   ├── ${dep_name}\n")
    else()
      set(DEPS_CONTENTS "${DEPS_CONTENTS}   └── ${dep_name}\n")
    endif()
    list(APPEND USED_DEPS ${dep_name})
  endforeach()
  set(DEPS_CONTENTS "${DEPS_CONTENTS}```\n")
endforeach()

set(DEPS_CONTENTS "${DEPS_CONTENTS}Libraries without any dependencies and not used by any other library:\n```\n")
list(REMOVE_DUPLICATES USED_DEPS)
foreach(no_dep_target ${NO_DEP_LIBS})
  if(NOT ${no_dep_target} IN_LIST USED_DEPS)
    set(DEPS_CONTENTS "${DEPS_CONTENTS}${no_dep_target}\n")
  endif()
endforeach()
set(DEPS_CONTENTS "${DEPS_CONTENTS}```\n")

configure_file(${CMAKE_SOURCE_DIR}/cmake/deps.md.in ${HARVEST_TARGET}/deps.md @ONLY)
