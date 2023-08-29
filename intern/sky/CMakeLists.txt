# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  include
)

set(INC_SYS

)

set(SRC
  source/sky_model.cpp
  source/sky_nishita.cpp

  include/sky_model.h
  source/sky_float3.h
  source/sky_model_data.h
)

set(LIB
)

blender_add_lib(bf_intern_sky "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
