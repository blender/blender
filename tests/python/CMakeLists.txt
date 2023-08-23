# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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

  # Don't fail tests on leaks since these often happen in external libraries that we can't fix.
  set(_lsan_options "exitcode=0")
  if(DEFINED ENV{LSAN_OPTIONS})
    set(_lsan_options "${_lsan_options}:$ENV{LSAN_OPTIONS}")
  endif()
  set_tests_properties(${testname} PROPERTIES ENVIRONMENT "LSAN_OPTIONS=${_lsan_options}")
  unset(_lsan_options)

  if(PLATFORM_ENV_INSTALL)
    set_tests_properties(${testname} PROPERTIES ENVIRONMENT "${PLATFORM_ENV_INSTALL}")
  endif()
endfunction()

# Run Python script outside Blender.
function(add_python_test testname testscript)
  if(NOT TEST_PYTHON_EXE)
    message(FATAL_ERROR "No Python configured for running tests, set TEST_PYTHON_EXE.")
  endif()

  add_test(
    NAME ${testname}
    COMMAND ${TEST_PYTHON_EXE} ${TEST_PYTHON_EXE_EXTRA_ARGS} ${testscript} ${ARGN}
    WORKING_DIRECTORY $<TARGET_FILE_DIR:blender>
  )

  set(_lsan_options "exitcode=0")
  if(DEFINED ENV{LSAN_OPTIONS})
    set(_lsan_options "${_lsan_options}:$ENV{LSAN_OPTIONS}")
  endif()
  set_tests_properties(${testname} PROPERTIES ENVIRONMENT "LSAN_OPTIONS=${_lsan_options}")
  unset(_lsan_options)

  if(PLATFORM_ENV_INSTALL)
    set_tests_properties(${testname} PROPERTIES ENVIRONMENT "${PLATFORM_ENV_INSTALL}")
  endif()
endfunction()

# ------------------------------------------------------------------------------
# GENERAL PYTHON CORRECTNESS TESTS
add_blender_test(
  script_load_keymap
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_keymap_completeness.py
)

add_blender_test(
  script_validate_keymap
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_keymap_validate.py
  -- --relaxed  # Disable minor things that should not cause tests to break.
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
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_bundled_modules.py -- --inside-blender
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
  script_pyapi_bpy_driver_secure_eval
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_bpy_driver_secure_eval.py
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

add_blender_test(
  script_pyapi_text
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_text.py
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

add_blender_test(
  blendfile_library_overrides
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_library_overrides.py --
  --output-dir ${TEST_OUT_DIR}/blendfile_io/
  --test-dir "${TEST_SRC_DIR}/libraries_and_linking"
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

add_blender_test(
  curve_to_mesh
  ${TEST_SRC_DIR}/modeling/curve_to_mesh.blend
  --python ${TEST_PYTHON_DIR}/curve_to_mesh.py
  --
  --run-all-tests
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
  physics_cloth
  ${TEST_SRC_DIR}/physics/cloth_test.blend
  --python ${TEST_PYTHON_DIR}/physics_cloth.py
  --
  --run-all-tests
)

add_blender_test(
  physics_softbody
  ${TEST_SRC_DIR}/physics/softbody_test.blend
  --python ${TEST_PYTHON_DIR}/physics_softbody.py
  --
  --run-all-tests
)

add_blender_test(
  physics_dynamic_paint
  ${TEST_SRC_DIR}/physics/dynamic_paint_test.blend
  --python ${TEST_PYTHON_DIR}/physics_dynamic_paint.py
  --
  --run-all-tests
)

add_blender_test(
  deform_modifiers
  ${TEST_SRC_DIR}/modeling/deform_modifiers.blend
  --python ${TEST_PYTHON_DIR}/deform_modifiers.py
  --
  --run-all-tests
)

if(WITH_MOD_OCEANSIM)
  add_blender_test(
    physics_ocean
    ${TEST_SRC_DIR}/physics/ocean_test.blend
    --python ${TEST_PYTHON_DIR}/physics_ocean.py
    --
    --run-all-tests
  )
endif()


add_blender_test(
  physics_particle_system
  ${TEST_SRC_DIR}/physics/physics_particle_test.blend
  --python ${TEST_PYTHON_DIR}/physics_particle_system.py
  --
  --run-all-tests
)

add_blender_test(
  physics_particle_instance
  ${TEST_SRC_DIR}/physics/physics_particle_instance.blend
  --python ${TEST_PYTHON_DIR}/physics_particle_instance.py
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
# ANIMATION TESTS
add_blender_test(
  bl_animation_fcurves
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_animation_fcurves.py
  --
  --testdir "${TEST_SRC_DIR}/animation"
)

add_blender_test(
  bl_rigging_symmetrize
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_rigging_symmetrize.py
  --
  --testdir "${TEST_SRC_DIR}/animation"
)

# ------------------------------------------------------------------------------
# IO TESTS

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

# SVG Import
if(TRUE)
  if(NOT OPENIMAGEIO_IDIFF)
    message(WARNING "Disabling SVG tests because OIIO idiff does not exist")
  else()
    set(_svg_render_tests complex path)

    foreach(render_test ${_svg_render_tests})
      add_python_test(
        io_curve_svg_${render_test}
        ${CMAKE_CURRENT_LIST_DIR}/bl_io_curve_svg_test.py
        -blender "${TEST_BLENDER_EXE}"
        -testdir "${TEST_SRC_DIR}/io_tests/svg/${render_test}"
        -idiff "${OPENIMAGEIO_IDIFF}"
        -outdir "${TEST_OUT_DIR}/io_curve_svg"
      )
    endforeach()

    unset(_svg_render_tests)
  endif()
endif()

if(WITH_CYCLES OR WITH_OPENGL_RENDER_TESTS)
  if(NOT OPENIMAGEIO_IDIFF)
    message(WARNING "Disabling render tests because OIIO idiff does not exist")
  elseif(NOT EXISTS "${TEST_SRC_DIR}/render/shader")
    message(WARNING "Disabling render tests because tests folder does not exist at ${TEST_SRC_DIR}")
  elseif(NOT WITH_COMPOSITOR_CPU)
    message(WARNING "Disabling render tests because WITH_COMPOSITOR_CPU is disabled")
  elseif(NOT WITH_OPENCOLORIO)
    message(WARNING "Disabling render tests because WITH_OPENCOLORIO is disabled")
  else()
    set(render_tests
      camera
      bsdf
      hair
      image_colorspace
      image_data_types
      image_mapping
      image_texture_limit
      integrator
      light
      light_group
      light_linking
      mesh
      pointcloud
      shader
      shadow_catcher
      sss
    )

    if(WITH_OPENSUBDIV)
      list(APPEND render_tests displacement)
    endif()

    if(WITH_FREESTYLE)
      list(APPEND render_tests render_layer)
    endif()

    if(WITH_MOD_FLUID)
      list(APPEND render_tests motion_blur reports volume)
    endif()

    if(WITH_OPENVDB)
      list(APPEND render_tests openvdb)
    endif()

    if(WITH_OPENIMAGEDENOISE)
      list(APPEND render_tests denoise)
    endif()

    # Disabled until new OpenGL version with deterministic results.
    #if(WITH_CYCLES_PATH_GUIDING)
    #  list(APPEND render_tests guiding)
    #endif()

    if(WITH_OPENGL_RENDER_TESTS)
      list(APPEND render_tests grease_pencil)
    endif()

    list(SORT render_tests)

    # Cycles
    if(WITH_CYCLES)
      set(_cycles_blacklist "")
      if(NOT WITH_CYCLES_OSL)
        set(_cycles_blacklist OSL)
      endif()
      foreach(_cycles_device ${CYCLES_TEST_DEVICES})
        string(TOLOWER "${_cycles_device}" _cycles_device_lower)
        set(_cycles_render_tests bake;${render_tests};osl)

        foreach(render_test ${_cycles_render_tests})
          set(_cycles_test_name "cycles_${render_test}_${_cycles_device_lower}")
          add_python_test(
            ${_cycles_test_name}
            ${CMAKE_CURRENT_LIST_DIR}/cycles_render_tests.py
            -blender "${TEST_BLENDER_EXE}"
            -testdir "${TEST_SRC_DIR}/render/${render_test}"
            -idiff "${OPENIMAGEIO_IDIFF}"
            -outdir "${TEST_OUT_DIR}/cycles"
            -device ${_cycles_device}
            -blacklist ${_cycles_blacklist}
          )

          if(NOT ("${_cycles_device_lower}" STREQUAL "cpu"))
            set_tests_properties(${_cycles_test_name} PROPERTIES RUN_SERIAL TRUE)
          endif()
          unset(_cycles_test_name)
        endforeach()
      endforeach()
      unset(_cycles_blacklist)
    endif()

    if(WITH_OPENGL_RENDER_TESTS)
      # Eevee
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
        # Workbench
        add_python_test(
          workbench_${render_test}_test
          ${CMAKE_CURRENT_LIST_DIR}/workbench_render_tests.py
          -blender "${TEST_BLENDER_EXE}"
          -testdir "${TEST_SRC_DIR}/render/${render_test}"
          -idiff "${OPENIMAGEIO_IDIFF}"
          -outdir "${TEST_OUT_DIR}/workbench"
        )
      endforeach()

      if(WITH_HYDRA)
        # Hydra Storm
        foreach(render_test ${render_tests})
          add_python_test(
            storm_hydra_${render_test}_test
            ${CMAKE_CURRENT_LIST_DIR}/storm_render_tests.py
            -blender "${TEST_BLENDER_EXE}"
            -testdir "${TEST_SRC_DIR}/render/${render_test}"
            -idiff "${OPENIMAGEIO_IDIFF}"
            -outdir "${TEST_OUT_DIR}/storm_hydra"
            -export_method "HYDRA"
          )
        endforeach()

        foreach(render_test ${render_tests})
          add_python_test(
            storm_usd_${render_test}_test
            ${CMAKE_CURRENT_LIST_DIR}/storm_render_tests.py
            -blender "${TEST_BLENDER_EXE}"
            -testdir "${TEST_SRC_DIR}/render/${render_test}"
            -idiff "${OPENIMAGEIO_IDIFF}"
            -outdir "${TEST_OUT_DIR}/storm_usd"
            -export_method "USD"
          )
        endforeach()
      endif()
    endif()
  endif()
endif()

if(WITH_COMPOSITOR_CPU)
  if(NOT OPENIMAGEIO_IDIFF)
    message(WARNING "Disabling Compositor CPU tests because OIIO idiff does not exist")
  else()
    set(compositor_tests
      color
      converter
      filter
      input
      output
      vector

      multiple_node_setups
    )

    if(WITH_LIBMV)
      list(APPEND compositor_tests distort matte)
    endif()

    foreach(comp_test ${compositor_tests})
      add_python_test(
        compositor_${comp_test}_cpu_test
        ${CMAKE_CURRENT_LIST_DIR}/compositor_cpu_render_tests.py
        -blender "${TEST_BLENDER_EXE}"
        -testdir "${TEST_SRC_DIR}/compositor/${comp_test}"
        -idiff "${OPENIMAGEIO_IDIFF}"
        -outdir "${TEST_OUT_DIR}/compositor_cpu"
      )
    endforeach()

  endif()
endif()

# NOTE: WITH_COMPOSITOR_CPU is needed for rendering. 
if(WITH_COMPOSITOR_REALTIME_TESTS AND WITH_COMPOSITOR_CPU)
  if(NOT OPENIMAGEIO_IDIFF)
      message(WARNING "Disabling realtime compositor tests because OIIO idiff does not exist")
    else()
      set(compositor_tests
          color
          converter
          filter
          input
          output
          vector

          multiple_node_setups
        )
      
      if(WITH_LIBMV)
        list(APPEND compositor_tests distort matte)
      endif()

      foreach(comp_test ${compositor_tests})
        add_python_test(
          compositor_${comp_test}_realtime_test
          ${CMAKE_CURRENT_LIST_DIR}/compositor_realtime_render_tests.py
          -blender "${TEST_BLENDER_EXE}"
          -testdir "${TEST_SRC_DIR}/compositor/${comp_test}"
          -idiff "${OPENIMAGEIO_IDIFF}"
          -outdir "${TEST_OUT_DIR}/compositor_realtime"
        )
      endforeach()
  endif()
endif()

set(geo_node_tests
  attributes
  curve_primitives
  curves
  curves/interpolate_curves
  geometry
  instance
  repeat_zone
  mesh_primitives
  mesh
  mesh/extrude
  mesh/split_edges
  points
  texture
  utilities
  vector
)

if(WITH_GMP)
  list(APPEND geo_node_tests mesh/boolean)
endif()

if(WITH_OPENVDB)
  list(APPEND geo_node_tests volume)
endif()

if(WITH_OPENSUBDIV)
  list(APPEND geo_node_tests mesh/subdivision_tests)
endif()

foreach(geo_node_test ${geo_node_tests})
  if(EXISTS "${TEST_SRC_DIR}/modeling/geometry_nodes/${geo_node_test}/")
    file(GLOB files "${TEST_SRC_DIR}/modeling/geometry_nodes/${geo_node_test}/*.blend")
    foreach(file ${files})
      get_filename_component(filename ${file} NAME_WE)
      add_blender_test(
        geo_node_${geo_node_test}_test_${filename}
        ${file}
        --python ${TEST_PYTHON_DIR}/geo_node_test.py
      )
      endforeach()
  else()
    message(STATUS "Directory named ${TEST_SRC_DIR}/modeling/geometry_nodes/${geo_node_test}/ Not Found, disabling test.")
  endif()
endforeach()


if(EXISTS "${TEST_SRC_DIR}/modeling/geometry_nodes/simulation/")
  file(GLOB files "${TEST_SRC_DIR}/modeling/geometry_nodes/simulation/*.blend")
  foreach(file ${files})
    get_filename_component(filename ${file} NAME_WE)
    add_blender_test(
      geo_node_simulation_test_${filename}
      ${file}
      --python ${TEST_PYTHON_DIR}/geo_node_sim_test.py
      )
  endforeach()
else()
  message(STATUS "Directory named ${TEST_SRC_DIR}/modeling/geometry_nodes/simulation/ not found, disabling tests")
endif()


if(WITH_OPENGL_DRAW_TESTS)
  if(NOT OPENIMAGEIO_IDIFF)
    message(STATUS "Disabling OpenGL draw tests because OIIO idiff does not exist")
  elseif(NOT EXISTS "${TEST_SRC_DIR}/opengl")
    message(STATUS "Disabling OpenGL draw tests because tests folder does not exist at ${TEST_SRC_DIR}")
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
    alembic_export_tests
    ${CMAKE_CURRENT_LIST_DIR}/alembic_export_tests.py
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

if(WITH_USD)
  add_blender_test(
          usd_export_test
          --python ${CMAKE_CURRENT_LIST_DIR}/bl_usd_export_test.py
          --
          --testdir "${TEST_SRC_DIR}/usd"
  )
  add_blender_test(
    usd_import_test
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_usd_import_test.py
    --
    --testdir "${TEST_SRC_DIR}/usd"
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

if(NOT OPENIMAGEIO_IDIFF)
  message(STATUS "Disabling ImBuf image format tests because OIIO idiff does not exist")
else()
  set(OPTIONAL_FORMATS "")
  if(WITH_IMAGE_CINEON)
    set(OPTIONAL_FORMATS "${OPTIONAL_FORMATS} CINEON")
  endif()
  if(WITH_IMAGE_OPENEXR)
    set(OPTIONAL_FORMATS "${OPTIONAL_FORMATS} OPENEXR")
  endif()
  if(WITH_IMAGE_OPENJPEG)
    set(OPTIONAL_FORMATS "${OPTIONAL_FORMATS} OPENJPEG")
  endif()
  if(WITH_IMAGE_WEBP)
    set(OPTIONAL_FORMATS "${OPTIONAL_FORMATS} WEBP")
  endif()

  add_blender_test(
    bf_imbuf_save
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_imbuf_save.py
    --
    -test_dir "${TEST_SRC_DIR}/imbuf_io"
    -output_dir "${TEST_OUT_DIR}/imbuf_io/save"
    -idiff "${OPENIMAGEIO_IDIFF}"
    -optional_formats "${OPTIONAL_FORMATS}"
  )

  add_blender_test(
    bf_imbuf_load
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_imbuf_load.py
    --
    -test_dir "${TEST_SRC_DIR}/imbuf_io"
    -output_dir "${TEST_OUT_DIR}/imbuf_io/load"
    -idiff "${OPENIMAGEIO_IDIFF}"
    -optional_formats "${OPTIONAL_FORMATS}"
  )
endif()

# ------------------------------------------------------------------------------
# SEQUENCER RENDER TESTS

if(NOT OPENIMAGEIO_IDIFF)
  message(STATUS "Disabling sequencer render tests because OIIO idiff does not exist")
else()
  set(render_tests
    transform
  )

  foreach(render_test ${render_tests})
    add_python_test(
      sequencer_render_${render_test}
      ${CMAKE_CURRENT_LIST_DIR}/sequencer_render_tests.py
      -blender "${TEST_BLENDER_EXE}"
      -testdir "${TEST_SRC_DIR}/sequence_editing/${render_test}"
      -idiff "${OPENIMAGEIO_IDIFF}"
      -outdir "${TEST_OUT_DIR}/sequence_editing"
    )
  endforeach()
endif()


add_subdirectory(collada)

# TODO: disabled for now after collection unification
# add_subdirectory(view_layer)
