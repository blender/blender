# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
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

  mathutils.hh
  mathutils_Color.hh
  mathutils_Euler.hh
  mathutils_Matrix.hh
  mathutils_Quaternion.hh
  mathutils_Vector.hh
  mathutils_bvhtree.hh
  mathutils_geometry.hh
  mathutils_interpolate.hh
  mathutils_kdtree.hh
  mathutils_noise.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  bf_python_ext

  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
)


blender_add_lib(bf_python_mathutils "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
