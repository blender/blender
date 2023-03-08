# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../blenkernel
  ../../blenlib
  ../../bmesh
  ../../depsgraph
  ../../imbuf
  ../../makesdna
  ../../../../intern/guardedalloc
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  mathutils.c
  mathutils_Color.c
  mathutils_Euler.c
  mathutils_Matrix.c
  mathutils_Quaternion.c
  mathutils_Vector.c
  mathutils_bvhtree.cc
  mathutils_geometry.c
  mathutils_interpolate.c
  mathutils_kdtree.c
  mathutils_noise.c

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
  bf_blenlib
  bf_imbuf
  bf_python_ext

  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
)


blender_add_lib(bf_python_mathutils "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
