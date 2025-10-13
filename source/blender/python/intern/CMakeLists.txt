# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ..
  ../../editors/include
  ../../imbuf/intern/oiio
  ../../makesrna
  ../../../../intern/mantaflow/extern
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  bpy.cc
  bpy_app.cc
  bpy_app_alembic.cc
  bpy_app_build_options.cc
  bpy_app_ffmpeg.cc
  bpy_app_handlers.cc
  bpy_app_icons.cc
  bpy_app_ocio.cc
  bpy_app_oiio.cc
  bpy_app_opensubdiv.cc
  bpy_app_openvdb.cc
  bpy_app_sdl.cc
  bpy_app_timers.cc
  bpy_app_translations.cc
  bpy_app_usd.cc
  bpy_capi_utils.cc
  bpy_cli_command.cc
  bpy_driver.cc
  bpy_geometry_set.cc
  bpy_gizmo_wrap.cc
  bpy_inline_shader_nodes.cc
  bpy_interface.cc
  bpy_interface_atexit.cc
  bpy_interface_run.cc
  bpy_intern_string.cc
  bpy_library_load.cc
  bpy_library_write.cc
  bpy_msgbus.cc
  bpy_operator.cc
  bpy_operator_function.cc
  bpy_operator_wrap.cc
  bpy_path.cc
  bpy_props.cc
  bpy_rna.cc
  bpy_rna_anim.cc
  bpy_rna_array.cc
  bpy_rna_callback.cc
  bpy_rna_context.cc
  bpy_rna_data.cc
  bpy_rna_driver.cc
  bpy_rna_gizmo.cc
  bpy_rna_id_collection.cc
  bpy_rna_operator.cc
  bpy_rna_text.cc
  bpy_rna_types_capi.cc
  bpy_rna_ui.cc
  bpy_traceback.cc
  bpy_utils_previews.cc
  bpy_utils_units.cc

  bpy.hh
  bpy_app.hh
  bpy_app_alembic.hh
  bpy_app_build_options.hh
  bpy_app_ffmpeg.hh
  bpy_app_handlers.hh
  bpy_app_icons.hh
  bpy_app_ocio.hh
  bpy_app_oiio.hh
  bpy_app_opensubdiv.hh
  bpy_app_openvdb.hh
  bpy_app_sdl.hh
  bpy_app_timers.hh
  bpy_app_translations.hh
  bpy_app_usd.hh
  bpy_capi_utils.hh
  bpy_cli_command.hh
  bpy_driver.hh
  bpy_geometry_set.hh
  bpy_gizmo_wrap.hh
  bpy_inline_shader_nodes.hh
  bpy_intern_string.hh
  bpy_library.hh
  bpy_msgbus.hh
  bpy_operator.hh
  bpy_operator_wrap.hh
  bpy_path.hh
  bpy_props.hh
  bpy_rna.hh
  bpy_rna_anim.hh
  bpy_rna_callback.hh
  bpy_rna_context.hh
  bpy_rna_data.hh
  bpy_rna_driver.hh
  bpy_rna_gizmo.hh
  bpy_rna_id_collection.hh
  bpy_rna_operator.hh
  bpy_rna_text.hh
  bpy_rna_types_capi.hh
  bpy_rna_ui.hh
  bpy_traceback.hh
  bpy_utils_previews.hh
  bpy_utils_units.hh
  ../BPY_extern.hh
  ../BPY_extern_clog.hh
  ../BPY_extern_python.hh
  ../BPY_extern_run.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_animation
  bf_editor_interface
  bf_editor_space_api
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::animrig
  bf_python_gpu
  PRIVATE bf::imbuf::opencolorio
  PRIVATE bf::nodes

  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
  PRIVATE bf::windowmanager
)

# Only to check if `buildinfo` is available.
if(WITH_BUILDINFO)
  add_definitions(-DBUILD_DATE)
endif()

if(WITH_INSTALL_PORTABLE)
  add_definitions(-DWITH_INSTALL_PORTABLE)
endif()

if(WITH_PYTHON_MODULE)
  add_definitions(-DWITH_PYTHON_MODULE)
endif()


# Find the SSL certificate for the portable Blender installation.
# Without this, the absolute path on the builder is used, causing HTTPS access to fail.
# For example `urllib.request.urlopen("https://projects.blender.org")` fails
# (or any other HTTPS site). see: #102300 for details.
# NOTE: that this isn't necessary on WIN32.
if(WITH_PYTHON AND WITH_PYTHON_INSTALL AND (APPLE OR WITH_INSTALL_PORTABLE) AND (NOT WIN32))
  # - `PYTHON_SSL_CERT_FILE` absolute path to the PEM file.
  find_python_module_file("certifi/cacert.pem" PYTHON_SSL_CERT_FILE _python_ssl_cert_file_relative)
  mark_as_advanced(PYTHON_SSL_CERT_FILE)
  if(PYTHON_SSL_CERT_FILE)
    add_definitions(-DPYTHON_SSL_CERT_FILE="${_python_ssl_cert_file_relative}")
  else()
    message(WARNING
      "Unable to find \"certifi/cacert.pem\" within \"${PYTHON_LIBPATH}\", "
      "this build will not be able to use bundled certificates with the \"ssl\" module!"
    )
  endif()
  unset(_python_ssl_cert_file_relative)
endif()

if(WITH_PYTHON_SAFETY)
  add_definitions(-DWITH_PYTHON_SAFETY)
endif()

if(WITH_AUDASPACE)
  # It's possible to build with AUDASPACE (for file IO) but without the `aud` Python API,
  # when building without NUMPY so define both `WITH_AUDASPACE` & `DWITH_AUDASPACE_PY`.
  add_definitions(-DWITH_AUDASPACE)
  if(WITH_PYTHON_NUMPY)
    add_definitions(-DWITH_AUDASPACE_PY)
  endif()
endif()

if(WITH_BULLET)
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_CODEC_FFMPEG)
  list(APPEND INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFMPEG_LIBRARIES}
  )
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_CODEC_SNDFILE)
  add_definitions(-DWITH_SNDFILE)
endif()

if(WITH_CYCLES)
  list(APPEND INC
    ../../../../intern/cycles/blender
  )
  list(APPEND LIB
    bf_intern_cycles
  )
  add_definitions(-DWITH_CYCLES)
endif()

if(WITH_CYCLES_OSL)
  add_definitions(-DWITH_CYCLES_OSL)
endif()

if(WITH_CYCLES_EMBREE)
  add_definitions(-DWITH_CYCLES_EMBREE)
endif()

if(WITH_FREESTYLE)
  list(APPEND INC
    ../../freestyle/intern/python
  )
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_IMAGE_CINEON)
  add_definitions(-DWITH_IMAGE_CINEON)
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_IMAGE_OPENEXR)
endif()

if(WITH_IMAGE_OPENJPEG)
  add_definitions(-DWITH_IMAGE_OPENJPEG)
endif()

if(WITH_IMAGE_WEBP)
  add_definitions(-DWITH_IMAGE_WEBP)
endif()

if(WITH_INPUT_NDOF)
  add_definitions(-DWITH_INPUT_NDOF)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_OPENAL)
  add_definitions(-DWITH_OPENAL)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_SDL)
  list(APPEND INC_SYS
    ${SDL_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${SDL_LIBRARY}
  )
  add_definitions(-DWITH_SDL)
endif()

if(WITH_JACK)
  add_definitions(-DWITH_JACK)
endif()

if(WITH_COREAUDIO)
  add_definitions(-DWITH_COREAUDIO)
endif()

if(WITH_LIBMV)
  add_definitions(-DWITH_LIBMV)
endif()

if(WITH_PULSEAUDIO)
  add_definitions(-DWITH_PULSEAUDIO)
endif()

if(WITH_WASAPI)
  add_definitions(-DWITH_WASAPI)
endif()

if(WITH_MOD_OCEANSIM)
  add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_MOD_REMESH)
  add_definitions(-DWITH_MOD_REMESH)
endif()

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_IO_WAVEFRONT_OBJ)
  add_definitions(-DWITH_IO_WAVEFRONT_OBJ)
endif()

if(WITH_IO_PLY)
  add_definitions(-DWITH_IO_PLY)
endif()

if(WITH_IO_STL)
  add_definitions(-DWITH_IO_STL)
endif()

if(WITH_IO_FBX)
  add_definitions(-DWITH_IO_FBX)
endif()

if(WITH_IO_GREASE_PENCIL)
  add_definitions(-DWITH_IO_GREASE_PENCIL)
endif()

if(WITH_ALEMBIC)
  add_definitions(-DWITH_ALEMBIC)
  list(APPEND INC
    ../../io/alembic
  )
endif()

if(WITH_USD)
  add_definitions(-DWITH_USD)
  list(APPEND INC
    ../../io/usd
  )
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
  list(APPEND INC
    ../../../../intern/opensubdiv
  )
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

if(WITH_POTRACE)
  add_definitions(-DWITH_POTRACE)
endif()

if(WITH_PUGIXML)
  add_definitions(-DWITH_PUGIXML)
endif()

if(WITH_HARU)
  add_definitions(-DWITH_HARU)
endif()

if(WITH_HYDRA)
  list(APPEND LIB
    bf_render_hydra
  )
  add_definitions(-DWITH_HYDRA)
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_EXPERIMENTAL_FEATURES)
endif()

blender_add_lib(bf_python "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_python bf_rna)
