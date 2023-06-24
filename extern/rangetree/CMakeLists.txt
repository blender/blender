# SPDX-FileCopyrightText: 2002-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(SRC
  range_tree.h
  intern/generic_alloc_impl.h

  intern/range_tree.c
)

set(LIB
)

blender_add_lib(extern_rangetree "${SRC}" "${INC}" "" "${LIB}")
