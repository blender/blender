# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This script generates a modified symbols map,
# use this for situations where dynamic modifications of symbols map is needed,
# or done to suppress noisy output.

if(NOT DEFINED PLATFORM_SYMBOLS_MAP_SOURCE)
  message(FATAL_ERROR "PLATFORM_SYMBOLS_MAP_SOURCE was not defined!")
endif()
if(NOT DEFINED PLATFORM_SYMBOLS_MAP)
  message(FATAL_ERROR "PLATFORM_SYMBOLS_MAP was not defined!")
endif()

file(READ "${PLATFORM_SYMBOLS_MAP_SOURCE}" file_data)

if(WITH_LINKER_MOLD)
  # These generate noisy warnings.
  set(sym_list_remove
    "  _bss_start\;"
    "  __end\;"
    "  aligned_free\;"
  )
  foreach(sym ${sym_list_remove})
    string(REPLACE "${sym}" "/* ${sym} */" file_data "${file_data}")
  endforeach()
endif()

file(WRITE "${PLATFORM_SYMBOLS_MAP}" "${file_data}")
