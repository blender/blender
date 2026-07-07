# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  include
)

set(INC_SYS

)

set(SRC
  source/sky_single_scattering.cpp
  source/sky_multiple_scattering.cpp
  source/sky_hosek.cpp
  source/sky_hosek_data.h

  include/sky_hosek.h
  include/sky_nishita.h
  source/sky_math.h
)

set(LIB
  PRIVATE bf::dependencies::optional::tbb
)

blender_add_lib(bf_intern_sky "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
