# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SRC
  intern/common.cpp
  intern/decoder.cpp
  intern/encoder.cpp

  intern/common.h
  intern/decoder.h
  intern/encoder.h
)

set(LIB
  PRIVATE bf::dependencies::optional::draco
)

# Equivalent `blender_add_lib`, unrolled and stripped down to be able to set the library type to `SHARED`.
add_library(bf_intern_draco_bridge SHARED "${SRC}")
blender_link_libraries(bf_intern_draco_bridge "${LIB}")
blender_source_group(bf_intern_draco_bridge "${SRC}")

if(APPLE)
  # On macOS, prevent the draco library (already present in the application bundle) from being wrongly copied into the
  # Xcode archive used for application distribution, which pollutes the archive and prevents it from being properly
  # recognized. This effectively corrects a bad implicit CMake behavior for `SHARED` library targets on Xcode.
  set_target_properties(bf_intern_draco_bridge PROPERTIES
    XCODE_ATTRIBUTE_SKIP_INSTALL "YES"
    XCODE_ATTRIBUTE_INSTALL_PATH ""
  )
endif()
