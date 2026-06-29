# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS

)

set(SRC
  PRF_profile.hh
)

set(LIB
  PUBLIC bf::dependencies::optional::tracy_client
)
add_library(bf_intern_profile INTERFACE)
target_include_directories(bf_intern_profile INTERFACE .)
add_library(bf::intern::profile ALIAS bf_intern_profile)
target_sources(bf_intern_profile PRIVATE ${SRC})
blender_source_group(bf_intern_profile ${SRC})

target_link_libraries(bf_intern_profile INTERFACE bf::dependencies::optional::tracy_client)
