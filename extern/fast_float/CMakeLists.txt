# SPDX-FileCopyrightText: 2026 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(extern_fast_float INTERFACE)
target_include_directories(extern_fast_float SYSTEM INTERFACE ".")
add_library(bf::extern::fast_float ALIAS extern_fast_float)
