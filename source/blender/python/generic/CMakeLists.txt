# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../../blenkernel
  ../../blenlib
  ../../gpu
  ../../makesdna
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  bgl.c
  bl_math_py_api.c
  blf_py_api.c
  bpy_threads.c
  idprop_py_api.c
  imbuf_py_api.c
  py_capi_utils.c

  bgl.h
  bl_math_py_api.h
  blf_py_api.h
  idprop_py_api.h
  imbuf_py_api.h
  py_capi_utils.h

  # header-only
  python_utildefines.h
)

set(LIB
  ${GLEW_LIBRARY}
  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_python_ext "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
