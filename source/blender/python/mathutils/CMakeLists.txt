# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../imbuf
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  mathutils.cc
  mathutils_Color.cc
  mathutils_Euler.cc
  mathutils_Matrix.cc
  mathutils_Quaternion.cc
  mathutils_Vector.cc
  mathutils_bvhtree.cc
  mathutils_geometry.cc
  mathutils_interpolate.cc
  mathutils_kdtree.cc
  mathutils_noise.cc

  mathutils.h
  mathutils_Color.h
  mathutils_Euler.h
  mathutils_Matrix.h
  mathutils_Quaternion.h
  mathutils_Vector.h
  mathutils_bvhtree.h
  mathutils_geometry.h
  mathutils_interpolate.h
  mathutils_kdtree.h
  mathutils_noise.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_imbuf
  PRIVATE bf::intern::guardedalloc
  bf_python_ext

  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
)


blender_add_lib(bf_python_mathutils "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
