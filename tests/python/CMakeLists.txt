# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Use '--write-blend=/tmp/test.blend' to view output

set(TEST_SRC_DIR ${CMAKE_SOURCE_DIR}/tests/files)
set(TEST_PYTHON_DIR ${CMAKE_SOURCE_DIR}/tests/python)
set(TEST_OUT_DIR ${CMAKE_BINARY_DIR}/tests)

# ugh, any better way to do this on testing only?
file(MAKE_DIRECTORY ${TEST_OUT_DIR})
file(MAKE_DIRECTORY ${TEST_OUT_DIR}/io_tests)
file(MAKE_DIRECTORY ${TEST_OUT_DIR}/blendfile_io)

# Check which tests can be enabled.
if(IS_DIRECTORY ${TEST_SRC_DIR}/render)
  set(TEST_SRC_DIR_EXISTS TRUE)

  if(OPENIMAGEIO_TOOL)
    set(TEST_OPENIMAGEIO_TOOL_EXISTS TRUE)
  else()
    set(TEST_OPENIMAGEIO_TOOL_EXISTS FALSE)
    message(STATUS "Tests: Disabling render tests, missing oiiotool")
  endif()
else()
  set(TEST_SRC_DIR_EXISTS FALSE)
  message(STATUS "Tests: Disabling most tests, no test data in ${TEST_SRC_DIR}")
endif()

if(WITH_SYSTEM_PYTHON_TESTS)
  if(NOT EXISTS "${TEST_SYSTEM_PYTHON_EXE}")
    message(ERROR
      "'System Python' tests requested but no valid system python path, "
      "set TEST_SYSTEM_PYTHON_EXE."
    )
    set(WITH_SYSTEM_PYTHON_TESTS OFF)
  endif()
endif()

# Run Blender command with parameters.
function(add_blender_test_impl testname envvars_list exe)
  add_test(
    NAME ${testname}
    COMMAND ${exe} ${ARGN}
  )
  blender_test_set_envvars("${testname}" "${envvars_list}")
endfunction()

function(add_blender_test testname)
  add_blender_test_impl(
    "${testname}"
    ""
    "${TEST_BLENDER_EXE}"
    ${TEST_BLENDER_EXE_PARAMS}
    ${ARGN}
  )
endfunction()

function(add_blender_test_allow_error testname)
  # Remove `--debug-exit-on-error` since errors are printed on e.g. meshes with
  # invalid data or failed image loading, but sometimes we want to test those.
  set(EXE_PARAMS ${TEST_BLENDER_EXE_PARAMS})
  list(REMOVE_ITEM EXE_PARAMS --debug-exit-on-error)
  add_blender_test_impl(
    "${testname}"
    ""
    "${TEST_BLENDER_EXE}"
    ${EXE_PARAMS}
    ${ARGN}
  )
endfunction()

if(WITH_UI_TESTS)
  set(_blender_headless_env_vars "BLENDER_BIN=${TEST_BLENDER_EXE}")
  if(WITH_UI_TESTS_HEADLESS)

    # Currently only WAYLAND is supported, support for others may be added later.
    # In this case none of the WESTON environment variables will be used.
    if(WITH_GHOST_WAYLAND)
      set(_weston_bin_in_libdir OFF)
      if(DEFINED LIBDIR)
        set(_weston_bin_default "${LIBDIR}/wayland_weston/bin/weston")
      else()
        set(_weston_bin_default "weston")
      endif()
      set(WESTON_BIN "${_weston_bin_default}" CACHE STRING "\
  The location of weston, leave blank for the default location."
      )
      mark_as_advanced(WESTON_BIN)
      if((DEFINED LIBDIR) AND ("${WESTON_BIN}" STREQUAL "${_weston_bin_default}"))
        set(_weston_bin_in_libdir ON)
      endif()

      list(APPEND _blender_headless_env_vars
        "WESTON_BIN=${WESTON_BIN}"
      )

      if(_weston_bin_in_libdir)
        list(APPEND _blender_headless_env_vars
          "WAYLAND_ROOT_DIR=${LIBDIR}/wayland"
          "WESTON_ROOT_DIR=${LIBDIR}/wayland_weston"
        )
      endif()
    endif()
  else()
    list(APPEND _blender_headless_env_vars
      "PASS_THROUGH=1"
    )
  endif()
  function(add_blender_test_ui testname)
    # Remove `--background`a
    set(EXE_PARAMS ${TEST_BLENDER_EXE_PARAMS})
    list(REMOVE_ITEM EXE_PARAMS --background)
    add_blender_test_impl(
      "${testname}"
      "${_blender_headless_env_vars}"
      "${TEST_PYTHON_EXE}"
      "${CMAKE_SOURCE_DIR}/tests/utils/blender_headless.py"
      # NOTE: attempting to maximize the window causes problems with a headless `weston`,
      # while this could be investigated, use windowed mode instead.
      # Use a window size that balances software GPU rendering with enough room to use the UI.
      --factory-startup
      # Used so GHOST/Wayland doesn't attempt to load LIBDECOR
      # which can fail on some systems, causing tests to fail.
      # On other systems this is harmless.
      --no-window-frame
      -p 0 0 800 600
      "${EXE_PARAMS}"
      "${ARGN}"
    )
  endfunction()
endif()

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
  blender_test_set_envvars("${testname}" "")
endfunction()

# Run Python render test.
function(add_render_test testname testscript)
  if(TEST_OPENIMAGEIO_TOOL_EXISTS)
    set(_args ${ARGN} --blender "${TEST_BLENDER_EXE}" --oiiotool "${OPENIMAGEIO_TOOL}")
    if(WITH_TESTS_BATCHED)
      list(APPEND _args --batch)
    endif()
    add_python_test(${testname} ${testscript} ${_args})
  endif()
endfunction()

# Run Python script outside Blender, using system default Python3 interpreter,
# NOT the one specified in `TEST_PYTHON_EXE`.
function(add_system_python_test testname testscript)
  if(NOT WITH_SYSTEM_PYTHON_TESTS)
    return()
  endif()

  add_test(
    NAME ${testname}
    COMMAND ${TEST_SYSTEM_PYTHON_EXE} ${testscript} ${ARGN}
    WORKING_DIRECTORY $<TARGET_FILE_DIR:blender>
  )
  blender_test_set_envvars("${testname}" "")
endfunction()

# ------------------------------------------------------------------------------
# TESTS USING SYSTEM PYTHON
# ------------------------------------------------------------------------------

add_system_python_test(
  script_validate_on_system_python
  ${CMAKE_CURRENT_LIST_DIR}/system_python/load_tool_scripts.py
  --root-dir ${CMAKE_SOURCE_DIR}
)

# ------------------------------------------------------------------------------
# GENERAL PYTHON CORRECTNESS TESTS
# ------------------------------------------------------------------------------

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

add_blender_test(
  script_http_downloader
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_http_downloader.py
)

# test running operators doesn't segfault under various conditions
if(WITH_TESTS_EXPERIMENTAL)
  add_blender_test(
    script_run_operators
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_run_operators.py
  )
endif()

# ------------------------------------------------------------------------------
# PY API TESTS
# ------------------------------------------------------------------------------

add_blender_test(
  script_pyapi_bpy_app_tempdir
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_bpy_app_tempdir.py
)

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
  script_pyapi_prop
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_prop.py
)

add_blender_test(
  script_pyapi_prop_array
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_prop_array.py
)

add_blender_test(
  script_pyapi_text
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_text.py
)

add_blender_test(
  script_pyapi_bmesh
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_bmesh.py
)

add_blender_test(
  script_pyapi_grease_pencil
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_pyapi_grease_pencil.py
)

# ------------------------------------------------------------------------------
# DATA MANAGEMENT TESTS
# ------------------------------------------------------------------------------

add_blender_test(
  id_management
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_id_management.py
)

add_blender_test(
  bl_rna_paths
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_rna_paths.py
)

add_blender_test(
  bl_rna_accessors
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_rna_accessors.py
)

# ------------------------------------------------------------------------------
# BLEND IO & LINKING
# ------------------------------------------------------------------------------

add_blender_test(
  blendfile_io
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_io.py --
  --output-dir ${TEST_OUT_DIR}/blendfile_io/
)

# This test can be extremely long, especially in debug builds.
# Generate BLENDFILE_VERSIONING_SPLIT_RANGE instances of the test,
# each processing their own subset of the whole set of blendfiles.
if(TEST_SRC_DIR_EXISTS)
  set(BLENDFILE_VERSIONING_SPLIT_RANGE 128)
  math(EXPR BLENDFILE_VERSIONING_SPLIT_RANGE_CMAKE "${BLENDFILE_VERSIONING_SPLIT_RANGE} - 1")
  foreach(idx RANGE ${BLENDFILE_VERSIONING_SPLIT_RANGE_CMAKE})
    add_blender_test(
      "blendfile_versioning_${idx}_over_${BLENDFILE_VERSIONING_SPLIT_RANGE}"
      --log "*blendfile*"
      --debug-memory
      --debug
      --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_versioning.py --
      --src-test-dir ${TEST_SRC_DIR}/
      --output-dir ${TEST_OUT_DIR}/blendfile_io/
      --slice-range ${BLENDFILE_VERSIONING_SPLIT_RANGE}
      --slice-index ${idx}
    )
  endforeach()

  add_blender_test(
    blendfile_liblink
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_liblink.py --
    --src-test-dir ${TEST_SRC_DIR}/
    --output-dir ${TEST_OUT_DIR}/blendfile_io/
  )

  add_blender_test(
    blendfile_relationships
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_relationships.py --
    --src-test-dir ${TEST_SRC_DIR}/
    --output-dir ${TEST_OUT_DIR}/blendfile_io/
  )

  add_blender_test(
    blendfile_library_overrides
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_library_overrides.py --
    --output-dir ${TEST_OUT_DIR}/blendfile_io/
    --test-dir "${TEST_SRC_DIR}/libraries_and_linking"
  )

  add_blender_test(
    blendfile_header
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_blendfile_header.py --
    --testdir "${TEST_SRC_DIR}/io_tests/blend_parsing"
  )
endif()

# ------------------------------------------------------------------------------
# MODELING TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
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

  add_blender_test(
    curves_extrude
    ${TEST_SRC_DIR}/modeling/curves_extrude.blend
    --python ${TEST_PYTHON_DIR}/curves_extrude.py
    --
    --run-all-tests
  )

  add_blender_test(
    mesh_join
    --python ${TEST_PYTHON_DIR}/mesh_join.py
  )

  add_blender_test_allow_error(
    mesh_validate
    --python ${TEST_PYTHON_DIR}/mesh_validate.py
  )

  add_blender_test(
    object_edit
    --python ${TEST_PYTHON_DIR}/object_edit.py
  )

  add_blender_test(
    object_conversion
    ${TEST_SRC_DIR}/modeling/object_conversion.blend
    --python ${TEST_PYTHON_DIR}/object_conversion.py
    --
    --run-all-tests
  )

  add_blender_test(
    object_api
    --python ${TEST_PYTHON_DIR}/bl_object.py
  )
endif()

add_blender_test(
  geometry_attributes
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_geometry_attributes.py
)

add_blender_test(
  geonode_file_reporting
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_geonode_file_reporting.py
  --
  --testdir "${TEST_SRC_DIR}/io_tests"
)

# ------------------------------------------------------------------------------
# MODIFIERS TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
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

  add_blender_test(
    multires
    --python ${TEST_PYTHON_DIR}/bl_multires.py
    --
    --testdir "${TEST_SRC_DIR}/sculpting"
  )
endif()

# ------------------------------------------------------------------------------
# OPERATORS TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
  add_blender_test(
    operators
    ${TEST_SRC_DIR}/modeling/operators.blend
    --python ${TEST_PYTHON_DIR}/operators.py
    --
    --run-all-tests
  )
endif()

# ------------------------------------------------------------------------------
# ANIMATION TESTS
# ------------------------------------------------------------------------------

add_blender_test(
  bl_animation_armature
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_animation_armature.py
)

add_blender_test(
  bl_animation_bake
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_animation_bake.py
)

add_blender_test(
  bl_animation_nla_strip
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_animation_nla_strip.py
)

if(TEST_SRC_DIR_EXISTS)
  add_blender_test(
    bl_animation_drivers
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_animation_drivers.py
    --
    --testdir "${TEST_SRC_DIR}/animation"
  )

  add_blender_test(
    bl_animation_fcurves
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_animation_fcurves.py
    --
    --testdir "${TEST_SRC_DIR}/animation"
  )

  add_blender_test(
    bl_animation_action
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_animation_action.py
    --
    --testdir "${TEST_SRC_DIR}/animation"
    --output-dir "${TEST_OUT_DIR}/bl_animation_action"
  )

  add_blender_test(
    bl_animation_keyframing
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_animation_keyframing.py
    --
    --testdir "${TEST_SRC_DIR}/animation"
  )

  add_blender_test(
    bl_pose_assets
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_pose_assets.py
    --
    --testdir "${TEST_SRC_DIR}/animation"
  )

  add_blender_test(
    bl_rigging_symmetrize
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_rigging_symmetrize.py
    --
    --testdir "${TEST_SRC_DIR}/animation"
  )

  add_blender_test(
    vertex_group_painting
    --python ${CMAKE_CURRENT_LIST_DIR}/vertex_group_painting.py
    --
    --testdir "${TEST_SRC_DIR}/animation"
  )
endif()

# ------------------------------------------------------------------------------
# BRUSH TESTS
add_blender_test(
  bl_brush
  --python ${CMAKE_CURRENT_LIST_DIR}/bl_brush_test.py
)

# ------------------------------------------------------------------------------
# NODE GROUP TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
  add_blender_test(
    bl_node_structure_type_inference
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_node_structure_type_inference.py
    --
    --testdir "${TEST_SRC_DIR}/node_group"
  )

  add_blender_test(
    bl_node_socket_usage_inference
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_node_socket_usage_inference.py
    --
    --testdir "${TEST_SRC_DIR}/node_group"
  )

  add_blender_test(
    bl_node_group_compat
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_node_group_compat.py
    --
    --testdir "${TEST_SRC_DIR}/node_group"
  )

  add_blender_test(
    bl_node_group_interface
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_node_group_interface.py
    --
    --testdir "${TEST_SRC_DIR}/node_group"
  )
endif()

# ------------------------------------------------------------------------------
# SVG TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
  set(_svg_render_tests complex path)

  foreach(render_test ${_svg_render_tests})
    add_render_test(
      io_curve_svg_${render_test}
      ${CMAKE_CURRENT_LIST_DIR}/bl_io_curve_svg_test.py
      --testdir "${TEST_SRC_DIR}/io_tests/svg/${render_test}"
      --outdir "${TEST_OUT_DIR}/io_curve_svg"
    )
  endforeach()

  unset(_svg_render_tests)
endif()

# ------------------------------------------------------------------------------
# RENDER TESTS
# ------------------------------------------------------------------------------

if((WITH_CYCLES OR WITH_GPU_RENDER_TESTS) AND TEST_SRC_DIR_EXISTS)
  set(render_tests
    attributes
    camera
    colorspace
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
    node_inlining
    pointcloud
    principled_bsdf
    shader
    shadow_catcher
    sss
    texture
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

  if(WITH_OPENVDB AND WITH_MOD_FLUID)
    # Some tests in the OpenVDB folder make use of fluid simulations to generate smoke
    list(APPEND render_tests openvdb)
  endif()

  if(WITH_OPENIMAGEDENOISE)
    list(APPEND render_tests denoise)
  endif()

  # Disabled until new OpenPGL version with deterministic results.
  # if(WITH_CYCLES_PATH_GUIDING)
  #   list(APPEND render_tests guiding)
  # endif()

  if(WITH_GPU_RENDER_TESTS)
    list(APPEND render_tests grease_pencil)
  endif()

  list(SORT render_tests)

  # Cycles
  if(WITH_CYCLES)
    set(_cycles_blocklist "")
    set(_cycles_all_test_devices CPU CUDA OPTIX HIP HIP-RT METAL METAL-RT ONEAPI ONEAPI-RT)
    set(_cycles_osl_test_devices CPU OPTIX)
    foreach(_cycles_device ${CYCLES_TEST_DEVICES})
      if(NOT ${_cycles_device} IN_LIST _cycles_all_test_devices)
        message(FATAL_ERROR "Unknown Cycles test device ${_cycles_device}."
            "Supported devices are: ${_cycles_all_test_devices}")
      endif()
      string(TOLOWER "${_cycles_device}" _cycles_device_lower)
      set(_cycles_render_tests bake;${render_tests};osl)

      set(_cycles_osl_test_type none)
      if(WITH_CYCLES_OSL)
        # If the render device is CPU, or a GPU WITH_CYCLES_TEST_OSL enabled,
        # then enable the limited OSL tests (Tests OSL camera with OSL shading turned off).
        # This specific configuration is chosen to avoid long test times on the GPU due to
        # OSL JIT compilation, unless the developer explicitly enables OSL tests.
        if(("${_cycles_device_lower}" STREQUAL "cpu") OR (WITH_CYCLES_TEST_OSL AND ${_cycles_device} IN_LIST _cycles_osl_test_devices))
          set(_cycles_osl_test_type limited)
        endif()
      endif()

      foreach(render_test ${_cycles_render_tests})
        set(_cycles_test_name "cycles_${render_test}_${_cycles_device_lower}")
        if(NOT(WITH_CYCLES_TEST_OSL AND WITH_CYCLES_OSL AND ("${render_test}" STREQUAL "osl")))
          # Only run OSL specific tests during this phase if WITH_CYCLES_TEST_OSL isn't enabled
          add_render_test(
            ${_cycles_test_name}
            ${CMAKE_CURRENT_LIST_DIR}/cycles_render_tests.py
            --testdir "${TEST_SRC_DIR}/render/${render_test}"
            --outdir "${TEST_OUT_DIR}/cycles"
            --device ${_cycles_device}
            --osl "${_cycles_osl_test_type}"
          )
          if(NOT ("${_cycles_device_lower}" STREQUAL "cpu"))
            set_tests_properties(${_cycles_test_name} PROPERTIES RUN_SERIAL TRUE)
          endif()
        endif()

        # OSL variations of every test if WITH_CYCLES_TEST_OSL is set.
        if(WITH_CYCLES_TEST_OSL AND NOT "${_cycles_osl_test_type}" STREQUAL "none")
          add_render_test(
            ${_cycles_test_name}_osl
            ${CMAKE_CURRENT_LIST_DIR}/cycles_render_tests.py
            --testdir "${TEST_SRC_DIR}/render/${render_test}"
            --outdir "${TEST_OUT_DIR}/cycles_osl"
            --device ${_cycles_device}
            --osl "all"
          )
          if(NOT ("${_cycles_device_lower}" STREQUAL "cpu"))
            set_tests_properties(${_cycles_test_name}_osl PROPERTIES RUN_SERIAL TRUE)
          endif()
        endif()

        unset(_cycles_test_name)
      endforeach()
      unset(_cycles_osl_test_type)
    endforeach()
    unset(_cycles_osl_test_devices)
    unset(_cycles_all_test_devices)
  endif()

  if(WITH_GPU_RENDER_TESTS)
    list(APPEND gpu_render_tests ${render_tests})
    list(FILTER gpu_render_tests EXCLUDE REGEX light_group|shadow_catcher|denoise|guiding|reports)

    set(_gpu_render_tests_arguments)

    # EEVEE
    if(WITH_OPENGL_BACKEND)
      foreach(render_test ${gpu_render_tests})
        add_render_test(
          eevee_opengl_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/eevee_render_tests.py
          --testdir "${TEST_SRC_DIR}/render/${render_test}"
          --outdir "${TEST_OUT_DIR}/eevee_opengl"
          --gpu-backend opengl
          ${_gpu_render_tests_arguments}
        )
      endforeach()
    endif()

    if(WITH_METAL_BACKEND)
      foreach(render_test ${gpu_render_tests})
        add_render_test(
          eevee_metal_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/eevee_render_tests.py
          --testdir "${TEST_SRC_DIR}/render/${render_test}"
          --outdir "${TEST_OUT_DIR}/eevee_metal"
          --gpu-backend metal
          ${_gpu_render_tests_arguments}
        )
      endforeach()
    endif()

    if(WITH_VULKAN_BACKEND AND WITH_GPU_RENDER_TESTS_VULKAN)
      foreach(render_test ${gpu_render_tests})
        add_render_test(
          eevee_vulkan_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/eevee_render_tests.py
          --testdir "${TEST_SRC_DIR}/render/${render_test}"
          --outdir "${TEST_OUT_DIR}/eevee_vulkan"
          --gpu-backend vulkan
          ${_gpu_render_tests_arguments}
        )
      endforeach()
    endif()

    # Workbench
    if(WITH_OPENGL_BACKEND)
      foreach(render_test ${gpu_render_tests})
        add_render_test(
          workbench_opengl_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/workbench_render_tests.py
          --testdir "${TEST_SRC_DIR}/render/${render_test}"
          --outdir "${TEST_OUT_DIR}/workbench_opengl"
          --gpu-backend opengl
          ${_gpu_render_tests_arguments}
        )
      endforeach()
    endif()

    if(WITH_METAL_BACKEND)
      foreach(render_test ${gpu_render_tests})
        add_render_test(
          workbench_metal_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/workbench_render_tests.py
          --testdir "${TEST_SRC_DIR}/render/${render_test}"
          --outdir "${TEST_OUT_DIR}/workbench_metal"
          --gpu-backend metal
          ${_gpu_render_tests_arguments}
        )
      endforeach()
    endif()

    if(WITH_VULKAN_BACKEND AND WITH_GPU_RENDER_TESTS_VULKAN)
      foreach(render_test ${gpu_render_tests})
        add_render_test(
          workbench_vulkan_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/workbench_render_tests.py
          --testdir "${TEST_SRC_DIR}/render/${render_test}"
          --outdir "${TEST_OUT_DIR}/workbench_vulkan"
          --gpu-backend vulkan
          ${_gpu_render_tests_arguments}
        )
      endforeach()
    endif()

    # Overlay
    if(WITH_GPU_RENDER_TESTS_HEADED)
      if(WITH_OPENGL_BACKEND)
        add_render_test(
          overlay_opengl
          ${CMAKE_CURRENT_LIST_DIR}/overlay_render_tests.py
          --testdir "${TEST_SRC_DIR}/overlay"
          --outdir "${TEST_OUT_DIR}/overlay"
          --gpu-backend opengl
          ${_gpu_render_tests_arguments}
        )
      endif()

      if(WITH_METAL_BACKEND)
        add_render_test(
          overlay_metal
          ${CMAKE_CURRENT_LIST_DIR}/overlay_render_tests.py
          --testdir "${TEST_SRC_DIR}/overlay"
          --outdir "${TEST_OUT_DIR}/overlay"
          --gpu-backend metal
          ${_gpu_render_tests_arguments}
        )
      endif()

      if(WITH_VULKAN_BACKEND AND WITH_GPU_RENDER_TESTS_VULKAN)
        add_render_test(
          overlay_vulkan
          ${CMAKE_CURRENT_LIST_DIR}/overlay_render_tests.py
          --testdir "${TEST_SRC_DIR}/overlay"
          --outdir "${TEST_OUT_DIR}/overlay"
          --gpu-backend vulkan
          ${_gpu_render_tests_arguments}
        )
      endif()
    endif()

    if(WITH_HYDRA)
      # Hydra Storm
      foreach(render_test ${gpu_render_tests})
        add_render_test(
          storm_hydra_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/storm_render_tests.py
          --testdir "${TEST_SRC_DIR}/render/${render_test}"
          --outdir "${TEST_OUT_DIR}/storm_hydra"
          --export_method "HYDRA"
          ${_gpu_render_tests_arguments}
        )
      endforeach()

      foreach(render_test ${gpu_render_tests})
        add_render_test(
          storm_usd_${render_test}
          ${CMAKE_CURRENT_LIST_DIR}/storm_render_tests.py
          --testdir "${TEST_SRC_DIR}/render/${render_test}"
          --outdir "${TEST_OUT_DIR}/storm_usd"
          --export_method "USD"
          ${_gpu_render_tests_arguments}
        )
      endforeach()
    endif()
    unset(_gpu_render_tests_arguments)
  endif()
endif()

# ------------------------------------------------------------------------------
# COMPOSITOR TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
  set(compositor_tests
    input
    output
    color
    filter
    utilities
    vector

    pixel_nodes
    multiple_node_setups
  )

  if(WITH_LIBMV)
    list(APPEND compositor_tests keying mask tracking transform anisotropic_filtering)
  endif()

  foreach(comp_test ${compositor_tests})
    add_render_test(
      compositor_cpu_${comp_test}
      ${CMAKE_CURRENT_LIST_DIR}/compositor_render_tests.py
      --testdir "${TEST_SRC_DIR}/compositor/${comp_test}"
      --outdir "${TEST_OUT_DIR}/compositor_cpu"
    )
  endforeach()

  add_blender_test(
    compositor_cpu_file_output
    --python ${CMAKE_CURRENT_LIST_DIR}/compositor_file_output_tests.py
    --
    --testdir "${TEST_SRC_DIR}/compositor/file_output/"
    --outdir "${TEST_OUT_DIR}/compositor_cpu/file_output"
  )
endif()

if(WITH_GPU_COMPOSITOR_TESTS AND TEST_SRC_DIR_EXISTS)
  set(compositor_tests
    input
    output
    color
    filter
    utilities
    vector

    pixel_nodes
    multiple_node_setups
  )

  if(WITH_LIBMV)
    list(APPEND compositor_tests keying mask tracking transform)
  endif()

  if(WITH_OPENGL_BACKEND)
    foreach(comp_test ${compositor_tests})
      add_render_test(
        compositor_opengl_${comp_test}
        ${CMAKE_CURRENT_LIST_DIR}/compositor_render_tests.py
        --testdir "${TEST_SRC_DIR}/compositor/${comp_test}"
        --outdir "${TEST_OUT_DIR}/compositor_opengl"
        --gpu-backend opengl
      )
    endforeach()
    add_blender_test(
      compositor_opengl_file_output
      --python ${CMAKE_CURRENT_LIST_DIR}/compositor_file_output_tests.py
      --
      --testdir "${TEST_SRC_DIR}/compositor/file_output/"
      --outdir "${TEST_OUT_DIR}/compositor_opengl/file_output"
      --gpu-backend opengl
    )
  endif()
  if(WITH_METAL_BACKEND)
    foreach(comp_test ${compositor_tests})
      add_render_test(
        compositor_metal_${comp_test}
        ${CMAKE_CURRENT_LIST_DIR}/compositor_render_tests.py
        --testdir "${TEST_SRC_DIR}/compositor/${comp_test}"
        --outdir "${TEST_OUT_DIR}/compositor_metal"
        --gpu-backend metal
      )
    endforeach()
    add_blender_test(
      compositor_metal_file_output
      --python ${CMAKE_CURRENT_LIST_DIR}/compositor_file_output_tests.py
      --
      --testdir "${TEST_SRC_DIR}/compositor/file_output/"
      --outdir "${TEST_OUT_DIR}/compositor_metal/file_output"
      --gpu-backend metal
    )
  endif()
  if(WITH_VULKAN_BACKEND)
    foreach(comp_test ${compositor_tests})
      add_render_test(
        compositor_vulkan_${comp_test}
        ${CMAKE_CURRENT_LIST_DIR}/compositor_render_tests.py
        --testdir "${TEST_SRC_DIR}/compositor/${comp_test}"
        --outdir "${TEST_OUT_DIR}/compositor_vulkan"
        --gpu-backend vulkan
      )
    endforeach()
    add_blender_test(
      compositor_vulkan_file_output
      --python ${CMAKE_CURRENT_LIST_DIR}/compositor_file_output_tests.py
      --
      --testdir "${TEST_SRC_DIR}/compositor/file_output/"
      --outdir "${TEST_OUT_DIR}/compositor_vulkan/file_output"
      --gpu-backend vulkan
    )
  endif()
endif()

add_blender_test(
  compositing_node_group
  --python ${CMAKE_CURRENT_LIST_DIR}/compositing_node_group.py
)

# ------------------------------------------------------------------------------
# GEOMETRY NODE TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
  set(geo_node_tests
    attributes
    closure
    curve_primitives
    curves
    curves/interpolate_curves
    foreach_geometry_element_zone
    geometry
    grease_pencil
    import
    instance
    list
    repeat_zone
    mesh_primitives
    mesh
    mesh/extrude
    mesh/split_edges
    mesh/triangulate
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
    file(GLOB files "${TEST_SRC_DIR}/modeling/geometry_nodes/${geo_node_test}/*.blend")
    foreach(file ${files})
      get_filename_component(filename ${file} NAME_WE)
      add_blender_test(
        geo_node_${geo_node_test}_${filename}
        ${file}
        --python ${TEST_PYTHON_DIR}/geo_node_test.py
      )
    endforeach()
  endforeach()

  file(GLOB files "${TEST_SRC_DIR}/modeling/geometry_nodes/simulation/*.blend")
  foreach(file ${files})
    get_filename_component(filename ${file} NAME_WE)
    add_blender_test(
      geo_node_simulation_test_${filename}
      ${file}
      --python ${TEST_PYTHON_DIR}/geo_node_sim_test.py
    )
  endforeach()
endif()

# ------------------------------------------------------------------------------
# SCREENSHOT TESTS
# ------------------------------------------------------------------------------

if(WITH_GPU_RENDER_TESTS AND TEST_SRC_DIR_EXISTS)
  set(render_tests
  )

  if(WITH_TESTS_EXPERIMENTAL)
    # The viewport tests are flakey and difficult to keep up to date for use
    # in a CI environment due to constantly changing UI, but may still be helpful
    # for local development.
    #
    # Additionally, they do not currently succeed in an ASAN build. (2025-01-29)
    list(APPEND render_tests viewport)
  endif()

  foreach(render_test ${render_tests})
    add_render_test(
      screenshot_${render_test}
      ${CMAKE_CURRENT_LIST_DIR}/ui_screenshot_tests.py
      --testdir "${TEST_SRC_DIR}/screenshot/${render_test}"
      --outdir "${TEST_OUT_DIR}/screenshot"
    )
  endforeach()
endif()

# ------------------------------------------------------------------------------
# I/O TESTS
# ------------------------------------------------------------------------------

if(WITH_ALEMBIC AND TEST_SRC_DIR_EXISTS)
  find_package_wrapper(Alembic)
  if(NOT ALEMBIC_FOUND)
    message(FATAL_ERROR "Alembic is enabled but cannot be found")
  endif()
  get_filename_component(real_include_dir ${ALEMBIC_INCLUDE_DIR} REALPATH)
  get_filename_component(ALEMBIC_ROOT_DIR ${real_include_dir} DIRECTORY)

  add_python_test(
    io_alembic_export_tests
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
    --outdir "${TEST_OUT_DIR}/io_alembic"
  )
endif()

if(WITH_USD AND TEST_SRC_DIR_EXISTS)
  add_blender_test(
    io_usd_export
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_usd_export_test.py
    --
    --testdir "${TEST_SRC_DIR}/usd"
  )
  add_blender_test(
    io_usd_import
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_usd_import_test.py
    --
    --testdir "${TEST_SRC_DIR}/usd"
    --outdir "${TEST_OUT_DIR}/io_usd"
  )
endif()

if(TEST_SRC_DIR_EXISTS)
  add_blender_test_allow_error(
    io_fbx_import
    --python ${CMAKE_CURRENT_LIST_DIR}/io_fbx_import_test.py
    --
    --testdir "${TEST_SRC_DIR}/io_tests/fbx"
    --outdir "${TEST_OUT_DIR}/io_fbx"
  )
endif()

if(WITH_IO_WAVEFRONT_OBJ AND TEST_SRC_DIR_EXISTS)
  add_blender_test_allow_error(
    io_obj_import
    --python ${CMAKE_CURRENT_LIST_DIR}/io_obj_import_test.py
    --
    --testdir "${TEST_SRC_DIR}/io_tests/obj"
    --outdir "${TEST_OUT_DIR}/io_obj"
  )
endif()

if(WITH_IO_PLY AND TEST_SRC_DIR_EXISTS)
  add_blender_test_allow_error(
    io_ply_import
    --python ${CMAKE_CURRENT_LIST_DIR}/io_ply_import_test.py
    --
    --testdir "${TEST_SRC_DIR}/io_tests/ply"
    --outdir "${TEST_OUT_DIR}/io_ply"
  )
endif()

if(WITH_IO_STL AND TEST_SRC_DIR_EXISTS)
  add_blender_test_allow_error(
    io_stl_import
    --python ${CMAKE_CURRENT_LIST_DIR}/io_stl_import_test.py
    --
    --testdir "${TEST_SRC_DIR}/io_tests/stl"
    --outdir "${TEST_OUT_DIR}/io_stl"
  )
endif()

if(WITH_CODEC_FFMPEG AND TEST_SRC_DIR_EXISTS)
  add_python_test(
    ffmpeg
    ${CMAKE_CURRENT_LIST_DIR}/ffmpeg_tests.py
    --blender "${TEST_BLENDER_EXE}"
    --testdir "${TEST_SRC_DIR}/ffmpeg"
  )
endif()

if(TEST_SRC_DIR_EXISTS AND TEST_OPENIMAGEIO_TOOL_EXISTS)
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
    imbuf_save
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_imbuf_save.py
    --
    -test_dir "${TEST_SRC_DIR}/imbuf_io"
    -output_dir "${TEST_OUT_DIR}/imbuf_io/save"
    -oiiotool "${OPENIMAGEIO_TOOL}"
    -optional_formats "${OPTIONAL_FORMATS}"
  )

  add_blender_test_allow_error(
    imbuf_load
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_imbuf_load.py
    --
    -test_dir "${TEST_SRC_DIR}/imbuf_io"
    -output_dir "${TEST_OUT_DIR}/imbuf_io/load"
    -oiiotool "${OPENIMAGEIO_TOOL}"
    -optional_formats "${OPTIONAL_FORMATS}"
  )
endif()

# ------------------------------------------------------------------------------
# SEQUENCER RENDER TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
  set(render_tests
    effects
    filter
    transform
    blend_modes_byte
    blend_modes_float
    ffmpeg
  )

  set(video_output_tests
    video_output
  )

  foreach(render_test ${render_tests})
    add_render_test(
      sequencer_render_${render_test}
      ${CMAKE_CURRENT_LIST_DIR}/sequencer_render_tests.py
      --testdir "${TEST_SRC_DIR}/sequence_editing/${render_test}"
      --outdir "${TEST_OUT_DIR}/sequence_editing"
    )
  endforeach()

  foreach(video_output_test ${video_output_tests})
    add_render_test(
      sequencer_render_${video_output_test}
      ${CMAKE_CURRENT_LIST_DIR}/sequencer_video_output_tests.py
      --testdir "${TEST_SRC_DIR}/sequence_editing/${video_output_test}"
      --outdir "${TEST_OUT_DIR}/sequence_editing"
    )
  endforeach()

  add_blender_test(
    sequencer_input_colorspace
    --python ${CMAKE_CURRENT_LIST_DIR}/sequencer_input_colorspace.py
    --
    --testdir "${TEST_SRC_DIR}/sequence_editing"
  )

  add_blender_test(
    sequencer_load_meta_stack
    ${TEST_SRC_DIR}/sequence_editing/vse_load_meta_stack.blend
    --python ${TEST_PYTHON_DIR}/sequencer_load_meta_stack.py
  )
endif()

# ------------------------------------------------------------------------------
# SCULPTING TESTS
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
  add_blender_test(
    bl_sculpt_operators
    --python ${CMAKE_CURRENT_LIST_DIR}/bl_sculpt.py
    --
    --testdir "${TEST_SRC_DIR}/sculpting"
  )

  add_blender_test(
    bl_sculpt_brush_curve_presets
    --python ${CMAKE_CURRENT_LIST_DIR}/sculpt_paint/brush_strength_curves_test.py
    --
    --testdir "${TEST_SRC_DIR}/sculpting"
  )

  add_blender_test(
    bl_sculpt_mask_filter
    --python ${CMAKE_CURRENT_LIST_DIR}/sculpt_paint/mask_filter_test.py
    --
    --testdir "${TEST_SRC_DIR}/sculpting"
  )
endif()

if(WITH_GPU_MESH_PAINT_TESTS AND TEST_SRC_DIR_EXISTS)
  set(render_tests
    brushes
  )

  foreach(render_test ${render_tests})
    add_render_test(
      sculpt_render_${render_test}
      ${CMAKE_CURRENT_LIST_DIR}/sculpt_brush_render_tests.py
      --testdir "${TEST_SRC_DIR}/sculpting/${render_test}"
      --outdir "${TEST_OUT_DIR}/sculpting"
    )
  endforeach()
endif()


# ------------------------------------------------------------------------------
# Headless GUI Tests
# ------------------------------------------------------------------------------

if(WITH_UI_TESTS)
  # This could be generated with:
  # `"${TEST_PYTHON_EXE}" "${CMAKE_CURRENT_LIST_DIR}/ui_simulate/run.py" --list-tests`
  # list explicitly so changes bisecting/updated are sure to re-run CMake.
  set(_ui_tests
    test_sculpt.asset_shelf_brush_selection
    test_tools.sculpt_mode_toolbar
    test_undo.compositor_make_group
    test_undo.text_editor_edit_mode_mix
    test_undo.text_editor_simple
    test_undo.view3d_edit_mode_multi_window
    test_undo.view3d_font_edit_mode_simple
    test_undo.view3d_mesh_edit_separate
    test_undo.view3d_mesh_particle_edit_mode_simple
    test_undo.view3d_multi_mode_multi_window
    test_undo.view3d_multi_mode_select
    test_undo.view3d_sculpt_dyntopo_and_edit
    test_undo.view3d_sculpt_dyntopo_simple
    test_undo.view3d_sculpt_dyntopo_stroke_toggle
    test_undo.view3d_sculpt_with_memfile_step
    test_undo.view3d_sculpt_trim
    test_undo.view3d_simple
    test_undo.view3d_texture_paint_complex
    test_undo.view3d_texture_paint_simple
    test_workspace.sanity_check_general
    test_workspace.sanity_check_2d_animation
    test_workspace.sanity_check_sculpting
    test_workspace.sanity_check_storyboarding
    test_workspace.sanity_check_vfx
    test_workspace.sanity_check_video_editing
  )
  foreach(ui_test ${_ui_tests})
    add_blender_test_ui(
      "ui_${ui_test}"
      --enable-event-simulate
      --python "${CMAKE_CURRENT_LIST_DIR}/ui_simulate/run_blender_setup.py"
      --
      --tests "${ui_test}"
    )
  endforeach()
  unset(_ui_tests)
endif()


# ------------------------------------------------------------------------------
# VIEW LAYER Tests
# ------------------------------------------------------------------------------

if(TEST_SRC_DIR_EXISTS)
  # TODO: disabled for now after collection unification
  # add_subdirectory(view_layer)
endif()

# ------------------------------------------------------------------------------
# Linux Release sainty checks
# ------------------------------------------------------------------------------

if(WITH_LINUX_OFFICIAL_RELEASE_TESTS)
  get_filename_component(release_root_folder ${TEST_BLENDER_EXE} DIRECTORY)
  set(extra_args "")
  if(WITH_COMPILER_ASAN)
    set(extra_args
      ${extra_args}
      --sanitizer-build
    )
  endif()
  add_python_test(
    linux_release_sanity_checks
    ${CMAKE_SOURCE_DIR}/tools/check_blender_release/check_release.py
    -- --directory "${release_root_folder}" ${extra_args}
  )
  unset(extra_args)
  unset(release_root_folder)
endif()
