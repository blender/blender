# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Common modern targets for the blender dependencies
#
# The optional dependencies in the bf::dependencies::optional namespace
# will always exist, but will only be populated if the dep is actually
# enabled. Doing it this way, prevents us from having to sprinkle
# if(WITH_SOMEDEP) all over cmake, and you can just add
# `bf::dependencies::optional::somedep` to the LIB section without
# having to worry if it's enabled or not at the consumer site.

# -----------------------------------------------------------------------------
# Configure TBB

add_library(bf_deps_optional_tbb INTERFACE)
add_library(bf::dependencies::optional::tbb ALIAS bf_deps_optional_tbb)

if(WITH_TBB)
  target_compile_definitions(bf_deps_optional_tbb INTERFACE WITH_TBB)
  target_include_directories(bf_deps_optional_tbb SYSTEM INTERFACE ${TBB_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_tbb INTERFACE ${TBB_LIBRARIES})
endif()

add_library(bf_deps_optional_manifold INTERFACE)
add_library(bf::dependencies::optional::manifold ALIAS bf_deps_optional_manifold)
if(WITH_MANIFOLD)
  if(WIN32)
    target_compile_definitions(bf_deps_optional_manifold INTERFACE WITH_MANIFOLD)
    target_include_directories(bf_deps_optional_manifold SYSTEM INTERFACE ${MANIFOLD_INCLUDE_DIRS})
    target_link_libraries(bf_deps_optional_manifold INTERFACE ${MANIFOLD_LIBRARIES} bf::dependencies::optional::tbb)
  else()
    if(TARGET manifold::manifold)
      target_compile_definitions(bf_deps_optional_manifold INTERFACE WITH_MANIFOLD)
      target_link_libraries(bf_deps_optional_manifold INTERFACE manifold::manifold)
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Configure Eigen

add_library(bf_deps_eigen INTERFACE)
add_library(bf::dependencies::eigen ALIAS bf_deps_eigen)

target_include_directories(bf_deps_eigen SYSTEM INTERFACE ${EIGEN3_INCLUDE_DIRS})

if(WITH_TBB)
  target_compile_definitions(bf_deps_eigen INTERFACE WITH_TBB)
  target_include_directories(bf_deps_eigen SYSTEM INTERFACE ${TBB_INCLUDE_DIRS})
  target_link_libraries(bf_deps_eigen INTERFACE ${TBB_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure OpenColorIO

add_library(bf_deps_optional_opencolorio INTERFACE)
add_library(bf::dependencies::optional::opencolorio ALIAS bf_deps_optional_opencolorio)

if(WITH_OPENCOLORIO)
  target_compile_definitions(bf_deps_optional_opencolorio INTERFACE WITH_OPENCOLORIO)
  target_include_directories(bf_deps_optional_opencolorio SYSTEM INTERFACE ${OPENCOLORIO_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_opencolorio INTERFACE ${OPENCOLORIO_LIBRARIES})
endif()
