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
