# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

remove_strict_flags()

set(INC

)

set(INC_SYS

)

set(SRC
  minilzo/minilzo.c

  minilzo/lzoconf.h
  minilzo/lzodefs.h
  minilzo/minilzo.h
)

set(LIB
)

blender_add_lib(extern_minilzo "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
