cmake_minimum_required(VERSION 3.2)

if (MSVC)
  # Use statically linked versions of the MS standard libraries.
  if (NOT "${MSVC_RUNTIME}" STREQUAL "dll")
    foreach (flag_var
             CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
             CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if (${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif ()
    endforeach ()
  endif ()
endif ()
