# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Ensure there is enough cache assigned to CCACHE.
#
# NOTE: the function only exists to scope variables,
# replace it with `block()` once CMake 3.25 is the minimum supported version.
function(compiler_ccache_max_size_check)
  # NOTE(ideasman42): These sizes are configuration specific,
  # however the default cache (5GB) is small enough to give slow/poor performance.
  #
  # At the time of writing (Blender v5.3), a debug ASAN build on Linux can approach 4GB.
  # Assuming developers need to switch between two branches (release & main)
  # there needs to be at least 8GB of cache so cache can be reused when switching branches.
  # Use 16 GB as a minimum to give some head-room on top of this.
  set(ccache_size_gb_min 16)
  set(ccache_size_gb_recommended 32)

  # NOTE: `ccache --print-stats` reports `max_cache_size_kibibyte` which would avoid parsing
  # the formatted size, however this was added in CCACHE 4.9 (released 2023)
  # and older versions are still common, so parse the output of `--get-config` instead.
  #
  # CCACHE versions before 3.5 don't support `--get-config`, skip the check in this case.
  execute_process(
    COMMAND ${CCACHE_PROGRAM} --get-config max_size
    OUTPUT_VARIABLE ccache_max_size
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE ccache_result
    ERROR_QUIET
  )
  if(NOT (ccache_result EQUAL 0))
    return()
  endif()
  # Unlimited.
  if(ccache_max_size STREQUAL "0")
    return()
  endif()
  # Values are formatted as: `512.0 kB`, `500.0 MB`, `5.0 GB`, `16.0 GiB`, `1000.0 GB` etc.
  # Values under 1 kB are a byte count without a unit.
  if(NOT (ccache_max_size MATCHES "^([0-9]+)(\\.[0-9]+)? *[kKMGT]?i?B?$"))
    return()
  endif()
  set(ccache_max_size_int "${CMAKE_MATCH_1}")
  if(ccache_max_size MATCHES "T")
    return()
  endif()
  if((ccache_max_size MATCHES "G") AND (NOT (ccache_max_size_int LESS ccache_size_gb_min)))
    return()
  endif()
  message(WARNING
    "CCACHE maximum size is ${ccache_max_size}, below ${ccache_size_gb_min} GB, "
    "builds may not benefit from caching and may be slow. "
    "Consider increasing the size using:\n  ccache --max-size=${ccache_size_gb_recommended}G\n"
  )
endfunction()

compiler_ccache_max_size_check()
