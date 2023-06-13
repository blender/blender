# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later


# -----------------------------------------------------------------------------
# Build datatoc executable
set(SRC
  datatoc.c
)

# SRC_DNA_INC is defined in the parent dir
add_executable(datatoc ${SRC})


# -----------------------------------------------------------------------------
# Build datatoc_icon executable

if(NOT WITH_HEADLESS)

  set(SRC
    datatoc_icon.c
  )
  setup_platform_linker_flags(datatoc)
  if(WIN32)
    include_directories(
      ../blenlib
      ../../../intern/utfconv
    )

    # for winstuff_dir.c
    add_definitions(-DUSE_STANDALONE)

    list(APPEND SRC
      ../blenlib/intern/winstuff_dir.c
      ../../../intern/utfconv/utfconv.c
    )

  endif()

  include_directories(${PNG_INCLUDE_DIRS})

  add_executable(datatoc_icon ${SRC})
  setup_platform_linker_flags(datatoc_icon)

  target_link_libraries(datatoc_icon ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
  # PNG library uses pow() and floow(), so seems -lm is required for proper
  # working binary.
  if(UNIX AND NOT APPLE)
    target_link_libraries(datatoc_icon m)
  endif()
endif()
