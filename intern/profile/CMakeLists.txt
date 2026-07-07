# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  INTERFACE .
)

set(INC_SYS

)

set(SRC
  PRF_profile.hh
)

set(LIB
  INTERFACE bf::dependencies::optional::tracy_client
)

blender_add_lib(bf_intern_profile "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::intern::profile ALIAS bf_intern_profile)


