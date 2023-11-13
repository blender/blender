# SPDX-FileCopyrightText: 2014-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_GTESTS)
  # Otherwise we get warnings here that we can't fix in external projects
  remove_strict_flags()

  # Build common test executable used by most tests
  add_subdirectory(runner)

  # Build utility library used by test executables
  add_subdirectory(testing)
endif()
