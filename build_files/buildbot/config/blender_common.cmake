# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

include("${CMAKE_CURRENT_LIST_DIR}/blender_version.cmake")

function(get_cuda_toolkit_folder_version cuda_version out_variable)
  string(REGEX MATCH "^[0-9]+\\.[0-9]+" folder_version "${cuda_version}")
  set(${out_variable} "${folder_version}" PARENT_SCOPE)
endfunction()

function(buildbot_assert_path_exists path subject)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "${subject} does not exist: ${path}")
  endif()
endfunction()
