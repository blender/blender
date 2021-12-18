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
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

add_subdirectory(geometry)
add_subdirectory(shader)

set(INC
  .
  composite
  function
  geometry
  intern
  shader
  texture
  ../editors/include
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../functions
  ../geometry
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../render
  ../windowmanager
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
)


set(SRC
  composite/nodes/node_composite_alphaOver.cc
  composite/nodes/node_composite_antialiasing.cc
  composite/nodes/node_composite_bilateralblur.cc
  composite/nodes/node_composite_blur.cc
  composite/nodes/node_composite_bokehblur.cc
  composite/nodes/node_composite_bokehimage.cc
  composite/nodes/node_composite_boxmask.cc
  composite/nodes/node_composite_brightness.cc
  composite/nodes/node_composite_channelMatte.cc
  composite/nodes/node_composite_chromaMatte.cc
  composite/nodes/node_composite_colorMatte.cc
  composite/nodes/node_composite_colorSpill.cc
  composite/nodes/node_composite_colorbalance.cc
  composite/nodes/node_composite_colorcorrection.cc
  composite/nodes/node_composite_common.cc
  composite/nodes/node_composite_composite.cc
  composite/nodes/node_composite_cornerpin.cc
  composite/nodes/node_composite_crop.cc
  composite/nodes/node_composite_cryptomatte.cc
  composite/nodes/node_composite_curves.cc
  composite/nodes/node_composite_defocus.cc
  composite/nodes/node_composite_denoise.cc
  composite/nodes/node_composite_despeckle.cc
  composite/nodes/node_composite_diffMatte.cc
  composite/nodes/node_composite_dilate.cc
  composite/nodes/node_composite_directionalblur.cc
  composite/nodes/node_composite_displace.cc
  composite/nodes/node_composite_distanceMatte.cc
  composite/nodes/node_composite_doubleEdgeMask.cc
  composite/nodes/node_composite_ellipsemask.cc
  composite/nodes/node_composite_exposure.cc
  composite/nodes/node_composite_filter.cc
  composite/nodes/node_composite_flip.cc
  composite/nodes/node_composite_gamma.cc
  composite/nodes/node_composite_glare.cc
  composite/nodes/node_composite_hueSatVal.cc
  composite/nodes/node_composite_huecorrect.cc
  composite/nodes/node_composite_idMask.cc
  composite/nodes/node_composite_image.cc
  composite/nodes/node_composite_inpaint.cc
  composite/nodes/node_composite_invert.cc
  composite/nodes/node_composite_keying.cc
  composite/nodes/node_composite_keyingscreen.cc
  composite/nodes/node_composite_lensdist.cc
  composite/nodes/node_composite_levels.cc
  composite/nodes/node_composite_lummaMatte.cc
  composite/nodes/node_composite_mapRange.cc
  composite/nodes/node_composite_mapUV.cc
  composite/nodes/node_composite_mapValue.cc
  composite/nodes/node_composite_mask.cc
  composite/nodes/node_composite_math.cc
  composite/nodes/node_composite_mixrgb.cc
  composite/nodes/node_composite_movieclip.cc
  composite/nodes/node_composite_moviedistortion.cc
  composite/nodes/node_composite_normal.cc
  composite/nodes/node_composite_normalize.cc
  composite/nodes/node_composite_outputFile.cc
  composite/nodes/node_composite_pixelate.cc
  composite/nodes/node_composite_planetrackdeform.cc
  composite/nodes/node_composite_posterize.cc
  composite/nodes/node_composite_premulkey.cc
  composite/nodes/node_composite_rgb.cc
  composite/nodes/node_composite_rotate.cc
  composite/nodes/node_composite_scale.cc
  composite/nodes/node_composite_sepcombHSVA.cc
  composite/nodes/node_composite_sepcombRGBA.cc
  composite/nodes/node_composite_sepcombYCCA.cc
  composite/nodes/node_composite_sepcombYUVA.cc
  composite/nodes/node_composite_setalpha.cc
  composite/nodes/node_composite_splitViewer.cc
  composite/nodes/node_composite_stabilize2d.cc
  composite/nodes/node_composite_sunbeams.cc
  composite/nodes/node_composite_switch.cc
  composite/nodes/node_composite_switchview.cc
  composite/nodes/node_composite_texture.cc
  composite/nodes/node_composite_tonemap.cc
  composite/nodes/node_composite_trackpos.cc
  composite/nodes/node_composite_transform.cc
  composite/nodes/node_composite_translate.cc
  composite/nodes/node_composite_valToRgb.cc
  composite/nodes/node_composite_value.cc
  composite/nodes/node_composite_vecBlur.cc
  composite/nodes/node_composite_viewer.cc
  composite/nodes/node_composite_zcombine.cc

  composite/node_composite_tree.cc
  composite/node_composite_util.cc

  function/nodes/legacy/node_fn_random_float.cc

  function/nodes/node_fn_align_euler_to_vector.cc
  function/nodes/node_fn_boolean_math.cc
  function/nodes/node_fn_compare.cc
  function/nodes/node_fn_float_to_int.cc
  function/nodes/node_fn_input_bool.cc
  function/nodes/node_fn_input_color.cc
  function/nodes/node_fn_input_int.cc
  function/nodes/node_fn_input_special_characters.cc
  function/nodes/node_fn_input_string.cc
  function/nodes/node_fn_input_vector.cc
  function/nodes/node_fn_random_value.cc
  function/nodes/node_fn_replace_string.cc
  function/nodes/node_fn_rotate_euler.cc
  function/nodes/node_fn_slice_string.cc
  function/nodes/node_fn_string_length.cc
  function/nodes/node_fn_value_to_string.cc
  function/node_function_util.cc

  texture/nodes/node_texture_at.c
  texture/nodes/node_texture_bricks.c
  texture/nodes/node_texture_checker.c
  texture/nodes/node_texture_common.c
  texture/nodes/node_texture_compose.c
  texture/nodes/node_texture_coord.c
  texture/nodes/node_texture_curves.c
  texture/nodes/node_texture_decompose.c
  texture/nodes/node_texture_distance.c
  texture/nodes/node_texture_hueSatVal.c
  texture/nodes/node_texture_image.c
  texture/nodes/node_texture_invert.c
  texture/nodes/node_texture_math.c
  texture/nodes/node_texture_mixRgb.c
  texture/nodes/node_texture_output.c
  texture/nodes/node_texture_proc.c
  texture/nodes/node_texture_rotate.c
  texture/nodes/node_texture_scale.c
  texture/nodes/node_texture_texture.c
  texture/nodes/node_texture_translate.c
  texture/nodes/node_texture_valToNor.c
  texture/nodes/node_texture_valToRgb.c
  texture/nodes/node_texture_viewer.c
  texture/node_texture_tree.c
  texture/node_texture_util.c

  intern/derived_node_tree.cc
  intern/geometry_nodes_eval_log.cc
  intern/math_functions.cc
  intern/node_common.cc
  intern/node_declaration.cc
  intern/node_exec.cc
  intern/node_geometry_exec.cc
  intern/node_multi_function.cc
  intern/node_socket.cc
  intern/node_socket_declarations.cc
  intern/socket_search_link.cc
  intern/node_tree_ref.cc
  intern/node_util.c

  composite/node_composite_util.hh
  function/node_function_util.hh
  texture/node_texture_util.h

  NOD_common.h
  NOD_composite.h
  NOD_derived_node_tree.hh
  NOD_function.h
  NOD_geometry.h
  NOD_geometry_exec.hh
  NOD_geometry_nodes_eval_log.hh
  NOD_math_functions.hh
  NOD_multi_function.hh
  NOD_node_declaration.hh
  NOD_node_tree_ref.hh
  NOD_shader.h
  NOD_socket.h
  NOD_socket_declarations.hh
  NOD_socket_search_link.hh
  NOD_socket_declarations_geometry.hh
  NOD_static_types.h
  NOD_texture.h
  intern/node_common.h
  intern/node_exec.h
  intern/node_util.h
)

set(LIB
  bf_bmesh
  bf_functions
  bf_nodes_geometry
  bf_nodes_shader
)

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
    "../../../intern/rigidbody/"
  )
  if(NOT WITH_SYSTEM_BULLET)
  list(APPEND LIB
    extern_bullet
    )
  endif()

  list(APPEND LIB
    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_PYTHON)
  list(APPEND INC
    ../python
  )
  list(APPEND INC_SYS
    ${PYTHON_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${PYTHON_LINKFLAGS}
    ${PYTHON_LIBRARIES}
  )
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_TBB)
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_COMPOSITOR)
  list(APPEND INC
    ../compositor
  )
  add_definitions(-DWITH_COMPOSITOR)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)

  list(APPEND INC_SYS
    ${GMP_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${GMP_LIBRARIES}
  )
endif()

if(WITH_OPENVDB)
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
endif()

blender_add_lib(bf_nodes "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
