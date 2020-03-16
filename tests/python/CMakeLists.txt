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

# Use '--write-blend=/tmp/test.blend' to view output

# Some tests are interesting but take too long to run
# and don't give deterministic results
set(USE_EXPERIMENTAL_TESTS FALSE)

set(TEST_SRC_DIR ${CMAKE_SOURCE_DIR}/../lib/tests)
set(TEST_PYTHON_DIR ${CMAKE_SOURCE_DIR}/tests/python)
set(TEST_OUT_DIR ${CMAKE_BINARY_DIR}/tests)

# ugh, any better way to do this on testing only?
file(MAKE_DIRECTORY ${TEST_OUT_DIR})
file(MAKE_DIRECTORY ${TEST_OUT_DIR}/io_tests)
file(MAKE_DIRECTORY ${TEST_OUT_DIR}/blendfile_io)

# if(NOT IS_DIRECTORY ${TEST_SRC_DIR})
#   message(FATAL_ERROR "CMake test directory not found!")
# endif()

# Run Blender command with parameters.
function(add_blender_test testname)
  add_test(
    NAME ${testname}
    COMMAND "${TEST_BLENDER_EXE}" ${TEST_BLENDER_EXE_PARAMS} ${ARGN}
  )

  # Don't fail tests on leaks since these often happen in external libraries
  # that we can't fix.
  set_tests_properties(${testname} PROPERTIES ENVIRONMENT LSAN_OPTIONS=exitcode=0)
endfunction()

# Run Python script outside Blender.
function(add_python_test testname testscript)
  if(MSVC)
    add_test(
      NAME ${testname}
      COMMAND ${TEST_PYTHON_EXE} ${testscript} ${ARGN}
    )
  else()
    add_test(
      NAME ${testname}
      COMMAND ${testscript} ${ARGN}
    )
  endif()

  set_tests_properties(${testname} PROPERTIES ENVIRONMENT LSAN_OPTIONS=exitcode=0)
endfunction()

# ------------------------------------------------------------------------------
# GENERAL PYTHON CORRECTNESS TESTS
add_blender_test(
  script_load_keymap
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_keymap_completeness.py
)

add_blender_test(
  script_load_addons
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_load_addons.py
)

add_blender_test(
  script_load_modules
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_load_py_modules.py
)

add_blender_test(
  script_bundled_modules
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_bundled_modules.py
)

# test running operators doesn't segfault under various conditions
if(USE_EXPERIMENTAL_TESTS)
  add_blender_test(
    script_run_operators
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_run_operators.py
  )
endif()

# ------------------------------------------------------------------------------
# PY API TESTS
add_blender_test(
  script_pyapi_bpy_path
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_bpy_path.py
)

add_blender_test(
  script_pyapi_bpy_utils_units
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_bpy_utils_units.py
)

add_blender_test(
  script_pyapi_mathutils
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_mathutils.py
)

add_blender_test(
  script_pyapi_idprop
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_idprop.py
)

add_blender_test(
  script_pyapi_idprop_datablock
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_idprop_datablock.py
)

add_blender_test(
  script_pyapi_prop_array
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_prop_array.py
)

# ------------------------------------------------------------------------------
# DATA MANAGEMENT TESTS

add_blender_test(
  id_management
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_id_management.py
)

# ------------------------------------------------------------------------------
# BLEND IO & LINKING

add_blender_test(
  blendfile_io
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_io.py --
  --output-dir ${TEST_OUT_DIR}/blendfile_io/
)

add_blender_test(
  blendfile_liblink
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_liblink.py --
  --output-dir ${TEST_OUT_DIR}/blendfile_io/
)

# ------------------------------------------------------------------------------
# MODELING TESTS
add_blender_test(
  bmesh_bevel
  ${TEST_SRC_DIR}/modeling/bevel_regression.blend
  --python ${TEST_PYTHON_DIR}/bevel_operator.py
  --
  --run-all-tests
)

add_blender_test(
  bmesh_boolean
  ${TEST_SRC_DIR}/modeling/bool_regression.blend
  --python ${TEST_PYTHON_DIR}/boolean_operator.py
  --
  --run-all-tests
)

add_blender_test(
  bmesh_split_faces
  ${TEST_SRC_DIR}/modeling/split_faces_test.blend
  --python-text run_tests
)

# ------------------------------------------------------------------------------
# MODIFIERS TESTS
add_blender_test(
  object_modifier_array
  ${TEST_SRC_DIR}/modifier_stack/array_test.blend
  --python-text run_tests.py
)

add_blender_test(
  modifiers
  ${TEST_SRC_DIR}/modeling/modifiers.blend
  --python ${TEST_PYTHON_DIR}/modifiers.py
  --
  --run-all-tests
)

add_blender_test(
  constraints
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_constraints.py
  --
  --testdir "${TEST_SRC_DIR}/constraints"
)

# ------------------------------------------------------------------------------
# OPERATORS TESTS
add_blender_test(
  operators
  ${TEST_SRC_DIR}/modeling/operators.blend
  --python ${TEST_PYTHON_DIR}/operators.py
  --
  --run-all-tests
)

# ------------------------------------------------------------------------------
# IO TESTS

# OBJ Import tests
# disabled until updated & working
if(FALSE)
add_blender_test(
  import_obj_cube
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.obj\(filepath='${TEST_SRC_DIR}/io_tests/obj/cube.obj'\)
  --md5=39cce4bacac2d1b18fc470380279bc15 --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_obj_cube.blend
)

add_blender_test(
  import_obj_nurbs_cyclic
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.obj\(filepath='${TEST_SRC_DIR}/io_tests/obj/nurbs_cyclic.obj'\)
  --md5=ad3c307e5883224a0492378cd32691ab --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_obj_nurbs_cyclic.blend
)

add_blender_test(
  import_obj_makehuman
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.obj\(filepath='${TEST_SRC_DIR}/io_tests/obj/makehuman.obj'\)
  --md5=c9f78b185e58358daa4ecaecfa75464e --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_obj_makehuman.blend
)
endif()

# OBJ Export tests
add_blender_test(
  export_obj_cube
  ${TEST_SRC_DIR}/io_tests/blend_geometry/all_quads.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.obj\(filepath='${TEST_OUT_DIR}/io_tests/export_obj_cube.obj',use_selection=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_obj_cube.obj
  --md5_source=${TEST_OUT_DIR}/io_tests/export_obj_cube.mtl
  --md5=95832f81160f07101dc566cb286a9f76 --md5_method=FILE
)

add_blender_test(
  export_obj_nurbs
  ${TEST_SRC_DIR}/io_tests/blend_geometry/nurbs.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.obj\(filepath='${TEST_OUT_DIR}/io_tests/export_obj_nurbs.obj',use_selection=False,use_nurbs=True\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_obj_nurbs.obj
  --md5_source=${TEST_OUT_DIR}/io_tests/export_obj_nurbs.mtl
  --md5=a733ae4fa4a591ea9b0912da3af042de --md5_method=FILE
)

# disabled until updated & working
if(FALSE)
add_blender_test(
  export_obj_all_objects
  ${TEST_SRC_DIR}/io_tests/blend_scene/all_objects.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.obj\(filepath='${TEST_OUT_DIR}/io_tests/export_obj_all_objects.obj',use_selection=False,use_nurbs=True\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_obj_all_objects.obj
  --md5_source=${TEST_OUT_DIR}/io_tests/export_obj_all_objects.mtl
  --md5=04b3ed97cede07a19548fc518ce9f8ca --md5_method=FILE
)
endif()



# PLY Import tests
add_blender_test(
  import_ply_cube
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_mesh.ply\(filepath='${TEST_SRC_DIR}/io_tests/ply/cube_ascii.ply'\)
  --md5=527134343c27fc0ea73115b85fbfd3ac --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_ply_cube.blend
)

add_blender_test(
  import_ply_bunny
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_mesh.ply\(filepath='${TEST_SRC_DIR}/io_tests/ply/bunny2.ply'\)
  --md5=6ea5b8533400a17accf928b8fd024eaa --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_ply_bunny.blend
)

add_blender_test(
  import_ply_small_holes
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_mesh.ply\(filepath='${TEST_SRC_DIR}/io_tests/ply/many_small_holes.ply'\)
  --md5=c3093e26ecae5b6d59fbbcf2a0d0b39f --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_ply_small_holes.blend
)

# PLY Export
# disabled until updated & working
if(FALSE)
add_blender_test(
  export_ply_cube_all_data
  ${TEST_SRC_DIR}/io_tests/blend_geometry/cube_all_data.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_mesh.ply\(filepath='${TEST_OUT_DIR}/io_tests/export_ply_cube_all_data.ply'\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_ply_cube_all_data.ply
  --md5=6adc3748ceae8298496f99d0e7e76c15 --md5_method=FILE
)

add_blender_test(
  export_ply_suzanne_all_data
  ${TEST_SRC_DIR}/io_tests/blend_geometry/suzanne_all_data.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_mesh.ply\(filepath='${TEST_OUT_DIR}/io_tests/export_ply_suzanne_all_data.ply'\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_ply_suzanne_all_data.ply
  --md5=68ba23f02efd6511bfd093f45f703221 --md5_method=FILE
)
endif()

add_blender_test(
  export_ply_vertices # lame, add a better one
  ${TEST_SRC_DIR}/io_tests/blend_geometry/vertices.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_mesh.ply\(filepath='${TEST_OUT_DIR}/io_tests/export_ply_vertices.ply'\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_ply_vertices.ply
  --md5=37faba0aa2014451b27f951afa92f870 --md5_method=FILE
)


# STL Import tests
# disabled until updated & working
if(FALSE)
add_blender_test(
  import_stl_cube
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_mesh.stl\(filepath='${TEST_SRC_DIR}/io_tests/stl/cube.stl'\)
  --md5=8ceb5bb7e1cb5f4342fa1669988c66b4 --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_stl_cube.blend
)

add_blender_test(
  import_stl_conrod
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_mesh.stl\(filepath='${TEST_SRC_DIR}/io_tests/stl/conrod.stl'\)
  --md5=690a4b8eb9002dcd8631c5a575ea7348 --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_stl_conrod.blend
)

add_blender_test(
  import_stl_knot_max_simplified
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_mesh.stl\(filepath='${TEST_SRC_DIR}/io_tests/stl/knot_max_simplified.stl'\)
  --md5=baf82803f45a84ec4ddbad9cef57dd3e --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_stl_knot_max_simplified.blend
)
endif()

# STL Export
# disabled until updated & working
if(FALSE)
add_blender_test(
  export_stl_cube_all_data
  ${TEST_SRC_DIR}/io_tests/blend_geometry/cube_all_data.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_mesh.stl\(filepath='${TEST_OUT_DIR}/io_tests/export_stl_cube_all_data.stl'\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_stl_cube_all_data.stl
  --md5=64cb97c0cabb015e1c3f76369835075a --md5_method=FILE
)

add_blender_test(
  export_stl_suzanne_all_data
  ${TEST_SRC_DIR}/io_tests/blend_geometry/suzanne_all_data.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_mesh.stl\(filepath='${TEST_OUT_DIR}/io_tests/export_stl_suzanne_all_data.stl'\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_stl_suzanne_all_data.stl
  --md5=e9b23c97c139ad64961c635105bb9192 --md5_method=FILE
)

add_blender_test(
  export_stl_vertices # lame, add a better one
  ${TEST_SRC_DIR}/io_tests/blend_geometry/vertices.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_mesh.stl\(filepath='${TEST_OUT_DIR}/io_tests/export_stl_vertices.stl'\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_stl_vertices.stl
  --md5=3fd3c877e573beeebc782532cc005820 --md5_method=FILE
)
endif()


# X3D Import
# disabled until updated & working
if(FALSE)
add_blender_test(
  import_x3d_cube
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.x3d\(filepath='${TEST_SRC_DIR}/io_tests/x3d/color_cube.x3d'\)
  --md5=3fae9be004199c145941cd3f9f80ad7b --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_x3d_cube.blend
)

add_blender_test(
  import_x3d_teapot
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.x3d\(filepath='${TEST_SRC_DIR}/io_tests/x3d/teapot.x3d'\)
  --md5=8ee196c71947dce4199d55698501691e --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_x3d_teapot.blend
)

add_blender_test(
  import_x3d_suzanne_material
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.x3d\(filepath='${TEST_SRC_DIR}/io_tests/x3d/suzanne_material.x3d'\)
  --md5=3edea1353257d8b5a5f071942f417be6 --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_x3d_suzanne_material.blend
)

# X3D Export
add_blender_test(
  export_x3d_cube
  ${TEST_SRC_DIR}/io_tests/blend_geometry/all_quads.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.x3d\(filepath='${TEST_OUT_DIR}/io_tests/export_x3d_cube.x3d',use_selection=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_x3d_cube.x3d
  --md5=05312d278fe41da33560fdfb9bdb268f --md5_method=FILE
)

add_blender_test(
  export_x3d_nurbs
  ${TEST_SRC_DIR}/io_tests/blend_geometry/nurbs.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.x3d\(filepath='${TEST_OUT_DIR}/io_tests/export_x3d_nurbs.x3d',use_selection=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_x3d_nurbs.x3d
  --md5=4286d4a2aa507ef78b22ddcbdcc88481 --md5_method=FILE
)

add_blender_test(
  export_x3d_all_objects
  ${TEST_SRC_DIR}/io_tests/blend_scene/all_objects.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.x3d\(filepath='${TEST_OUT_DIR}/io_tests/export_x3d_all_objects.x3d',use_selection=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_x3d_all_objects.x3d
  --md5=f5f9fa4c5619a0eeab66685aafd2f7f0 --md5_method=FILE
)
endif()



# 3DS Import
# disabled until updated & working
if(FALSE)
add_blender_test(
  import_3ds_cube
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.autodesk_3ds\(filepath='${TEST_SRC_DIR}/io_tests/3ds/cube.3ds'\)
  --md5=cb5a45c35a343c3f5beca2a918472951 --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_3ds_cube.blend
)

add_blender_test(
  import_3ds_hierarchy_lara
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.autodesk_3ds\(filepath='${TEST_SRC_DIR}/io_tests/3ds/hierarchy_lara.3ds'\)
  --md5=766c873d9fdb5f190e43796cfbae63b6 --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_3ds_hierarchy_lara.blend
)

add_blender_test(
  import_3ds_hierarchy_greek_trireme
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.import_scene.autodesk_3ds\(filepath='${TEST_SRC_DIR}/io_tests/3ds/hierarchy_greek_trireme.3ds'\)
  --md5=b62ee30101e8999cb91ef4f8a8760056 --md5_method=SCENE
  --write-blend=${TEST_OUT_DIR}/io_tests/import_3ds_hierarchy_greek_trireme.blend
)
endif()

# 3DS Export
# disabled until updated & working
if(FALSE)
add_blender_test(
  export_3ds_cube
  ${TEST_SRC_DIR}/io_tests/blend_geometry/all_quads.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.autodesk_3ds\(filepath='${TEST_OUT_DIR}/io_tests/export_3ds_cube.3ds',use_selection=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_3ds_cube.3ds
  --md5=a31f5071b6c6dc7445b9099cdc7f63b3 --md5_method=FILE
)

add_blender_test(
  export_3ds_nurbs
  ${TEST_SRC_DIR}/io_tests/blend_geometry/nurbs.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.autodesk_3ds\(filepath='${TEST_OUT_DIR}/io_tests/export_3ds_nurbs.3ds',use_selection=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_3ds_nurbs.3ds
  --md5=5bdd21be3c80d814fbc83cb25edb08c2 --md5_method=FILE
)

add_blender_test(
  export_3ds_all_objects
  ${TEST_SRC_DIR}/io_tests/blend_scene/all_objects.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.autodesk_3ds\(filepath='${TEST_OUT_DIR}/io_tests/export_3ds_all_objects.3ds',use_selection=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_3ds_all_objects.3ds
  --md5=68447761ab0ca38e1e22e7c177ed48a8 --md5_method=FILE
)
endif()



# FBX Export
# 'use_metadata=False' for reliable md5's
# disabled until updated & working
if(FALSE)
add_blender_test(
  export_fbx_cube
  ${TEST_SRC_DIR}/io_tests/blend_geometry/all_quads.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.fbx\(filepath='${TEST_OUT_DIR}/io_tests/export_fbx_cube.fbx',use_selection=False,use_metadata=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_fbx_cube.fbx
  --md5=59a35577462f95f9a0b4e6035226ce9b --md5_method=FILE
)

add_blender_test(
  export_fbx_nurbs
  ${TEST_SRC_DIR}/io_tests/blend_geometry/nurbs.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.fbx\(filepath='${TEST_OUT_DIR}/io_tests/export_fbx_nurbs.fbx',use_selection=False,use_metadata=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_fbx_nurbs.fbx
  --md5=d31875f18f613fa0c3b16e978f87f6f8 --md5_method=FILE
)

add_blender_test(
  export_fbx_all_objects
  ${TEST_SRC_DIR}/io_tests/blend_scene/all_objects.blend
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
  --run={'FINISHED'}&bpy.ops.export_scene.fbx\(filepath='${TEST_OUT_DIR}/io_tests/export_fbx_all_objects.fbx',use_selection=False,use_metadata=False\)
  --md5_source=${TEST_OUT_DIR}/io_tests/export_fbx_all_objects.fbx
  --md5=b35eb2a9d0e73762ecae2278c25a38ac --md5_method=FILE
)
endif()

if(WITH_CYCLES OR WITH_OPENGL_RENDER_TESTS)
  if(NOT OPENIMAGEIO_IDIFF)
    MESSAGE(STATUS "Disabling render tests because OIIO idiff does not exist")
  elseif(NOT EXISTS "${TEST_SRC_DIR}/render/shader")
    MESSAGE(STATUS "Disabling render tests because tests folder does not exist at ${TEST_SRC_DIR}")
  else()
    set(render_tests
      bsdf
      denoise
      denoise_animation
      displacement
      hair
      image_colorspace
      image_data_types
      image_mapping
      image_texture_limit
      integrator
      light
      mesh
      motion_blur
      openvdb
      render_layer
      reports
      shader
      shadow_catcher
      sss
      volume
    )

    if(WITH_OPENGL_RENDER_TESTS)
      list(APPEND render_tests grease_pencil)
    endif()

    if(WITH_CYCLES)
      foreach(render_test bake;${render_tests})
        add_python_test(
          cycles_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/cycles_render_tests.py
          -blender "${TEST_BLENDER_EXE}"
          -testdir "${TEST_SRC_DIR}/render/${render_test}"
          -idiff "${OPENIMAGEIO_IDIFF}"
          -outdir "${TEST_OUT_DIR}/cycles"
        )
      endforeach()
    endif()

    if(WITH_OPENGL_RENDER_TESTS)
      foreach(render_test ${render_tests})
        add_python_test(
          eevee_${render_test}_test
          ${CMAKE_CURRENT_LIST_DIR}/eevee_render_tests.py
          -blender "${TEST_BLENDER_EXE}"
          -testdir "${TEST_SRC_DIR}/render/${render_test}"
          -idiff "${OPENIMAGEIO_IDIFF}"
          -outdir "${TEST_OUT_DIR}/eevee"
        )
      endforeach()

      foreach(render_test ${render_tests})
        add_python_test(
          workbench_${render_test}_test
          ${CMAKE_CURRENT_LIST_DIR}/workbench_render_tests.py
          -blender "${TEST_BLENDER_EXE}"
          -testdir "${TEST_SRC_DIR}/render/${render_test}"
          -idiff "${OPENIMAGEIO_IDIFF}"
          -outdir "${TEST_OUT_DIR}/workbench"
        )
      endforeach()
    endif()
  endif()
endif()

if(WITH_OPENGL_DRAW_TESTS)
  if(NOT OPENIMAGEIO_IDIFF)
    MESSAGE(STATUS "Disabling OpenGL draw tests because OIIO idiff does not exist")
  elseif(NOT EXISTS "${TEST_SRC_DIR}/opengl")
    MESSAGE(STATUS "Disabling OpenGL draw tests because tests folder does not exist at ${TEST_SRC_DIR}")
  else()
    # Use all subdirectories of opengl folder.
    file(GLOB children RELATIVE ${TEST_SRC_DIR}/opengl ${TEST_SRC_DIR}/opengl/*)
    foreach(child ${children})
      # Resolve symlinks, useful to test production files with linked libraries.
      get_filename_component(child_path ${TEST_SRC_DIR}/opengl/${child} REALPATH)
      if(IS_DIRECTORY ${child_path})
        file(GLOB_RECURSE blends "${child_path}/*.blend")
        if(blends)
          add_python_test(
            opengl_draw_${child}
            ${CMAKE_CURRENT_LIST_DIR}/opengl_draw_tests.py
            -blender "${TEST_BLENDER_EXE}"
            -testdir "${child_path}"
            -idiff "${OPENIMAGEIO_IDIFF}"
            -outdir "${TEST_OUT_DIR}/opengl_draw"
          )
        endif()
      endif()
    endforeach()
  endif()
endif()


if(WITH_ALEMBIC)
  find_package_wrapper(Alembic)
  if(NOT ALEMBIC_FOUND)
    message(FATAL_ERROR "Alembic is enabled but cannot be found")
  endif()
  get_filename_component(real_include_dir ${ALEMBIC_INCLUDE_DIR} REALPATH)
  get_filename_component(ALEMBIC_ROOT_DIR ${real_include_dir} DIRECTORY)

  add_python_test(
    alembic_tests
    ${CMAKE_CURRENT_LIST_DIR}/alembic_tests.py
    --blender "${TEST_BLENDER_EXE}"
    --testdir "${TEST_SRC_DIR}/alembic"
    --alembic-root "${ALEMBIC_ROOT_DIR}"
  )

  add_blender_test(
    script_alembic_io
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_alembic_io_test.py
    --
    --testdir "${TEST_SRC_DIR}/alembic"
  )
endif()

if(WITH_CODEC_FFMPEG)
  add_python_test(
    ffmpeg
    ${CMAKE_CURRENT_LIST_DIR}/ffmpeg_tests.py
    --blender "${TEST_BLENDER_EXE}"
    --testdir "${TEST_SRC_DIR}/ffmpeg"
  )
endif()

add_subdirectory(collada)

# TODO: disabled for now after collection unification
# add_subdirectory(view_layer)
