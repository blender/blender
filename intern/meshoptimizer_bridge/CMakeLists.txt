# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SRC
  intern/common.cpp
  intern/encoder.cpp
  intern/decoder.cpp

  intern/common.h
  intern/encoder.h
  intern/decoder.h
)

set(LIB
  PRIVATE bf::dependencies::optional::meshoptimizer
)

# Equivalent `blender_add_lib`,
# unrolled and stripped down to be able to set the library type to `SHARED`.
add_library(bf_intern_meshopt_bridge SHARED "${SRC}")
blender_link_libraries(bf_intern_meshopt_bridge "${LIB}")
blender_source_group(bf_intern_meshopt_bridge "${SRC}")

# Keep in sync with `intern/draco_bridge/CMakeLists.txt`.
if(APPLE)
  # On macOS, prevent the `meshopt` library (already present in the application bundle)
  # from being wrongly copied into the Xcode archive used for application distribution,
  # which pollutes the archive and prevents it from being properly recognized.
  # This effectively corrects a bad implicit CMake behavior for `SHARED` library targets on Xcode.
  set_target_properties(bf_intern_meshopt_bridge PROPERTIES
    XCODE_ATTRIBUTE_SKIP_INSTALL "YES"
    XCODE_ATTRIBUTE_INSTALL_PATH ""
  )
elseif(UNIX AND PLATFORM_BUNDLED_LIBRARIES AND WITH_INSTALL_PORTABLE)
  # Portable builds install this in `lib` beside its bundled dependencies, found via `$ORIGIN`.
  set_target_properties(bf_intern_meshopt_bridge PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
  )
endif()
