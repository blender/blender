# SPDX-FileCopyrightText: 2026 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(extern_json INTERFACE)
target_include_directories(extern_json SYSTEM INTERFACE "include")
add_library(bf::extern::json ALIAS extern_json)
