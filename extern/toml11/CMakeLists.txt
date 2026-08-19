# SPDX-FileCopyrightText: 2026 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(extern_toml INTERFACE)
target_include_directories(extern_toml SYSTEM INTERFACE ".")
add_library(bf::extern::toml ALIAS extern_toml)
