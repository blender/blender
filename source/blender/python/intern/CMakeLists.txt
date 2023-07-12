# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ..
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../editors/include
  ../../gpu
  ../../imbuf
  ../../imbuf/intern/oiio
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/mantaflow/extern
  ../../../../intern/opencolorio
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  bpy.c
  bpy_app.c
  bpy_app_alembic.c
  bpy_app_build_options.c
  bpy_app_ffmpeg.c
  bpy_app_handlers.c
  bpy_app_icons.c
  bpy_app_ocio.c
  bpy_app_oiio.c
  bpy_app_opensubdiv.c
  bpy_app_openvdb.c
  bpy_app_sdl.c
  bpy_app_timers.c
  bpy_app_translations.c
  bpy_app_usd.c
  bpy_capi_utils.c
  bpy_driver.c
  bpy_gizmo_wrap.c
  bpy_interface.c
  bpy_interface_atexit.c
  bpy_interface_run.c
  bpy_intern_string.c
  bpy_library_load.c
  bpy_library_write.c
  bpy_msgbus.c
  bpy_operator.c
  bpy_operator_wrap.c
  bpy_path.c
  bpy_props.c
  bpy_rna.c
  bpy_rna_anim.c
  bpy_rna_array.c
  bpy_rna_callback.c
  bpy_rna_context.c
  bpy_rna_data.c
  bpy_rna_driver.c
  bpy_rna_gizmo.c
  bpy_rna_id_collection.c
  bpy_rna_operator.c
  bpy_rna_text.c
  bpy_rna_types_capi.c
  bpy_rna_ui.c
  bpy_traceback.c
  bpy_utils_previews.c
  bpy_utils_units.c
  stubs.c

  bpy.h
  bpy_app.h
  bpy_app_alembic.h
  bpy_app_build_options.h
  bpy_app_ffmpeg.h
  bpy_app_handlers.h
  bpy_app_icons.h
  bpy_app_ocio.h
  bpy_app_oiio.h
  bpy_app_opensubdiv.h
  bpy_app_openvdb.h
  bpy_app_sdl.h
  bpy_app_timers.h
  bpy_app_translations.h
  bpy_app_usd.h
  bpy_capi_utils.h
  bpy_driver.h
  bpy_gizmo_wrap.h
  bpy_intern_string.h
  bpy_library.h
  bpy_msgbus.h
  bpy_operator.h
  bpy_operator_wrap.h
  bpy_path.h
  bpy_props.h
  bpy_rna.h
  bpy_rna_anim.h
  bpy_rna_callback.h
  bpy_rna_context.h
  bpy_rna_data.h
  bpy_rna_driver.h
  bpy_rna_gizmo.h
  bpy_rna_id_collection.h
  bpy_rna_operator.h
  bpy_rna_text.h
  bpy_rna_types_capi.h
  bpy_rna_ui.h
  bpy_traceback.h
  bpy_utils_previews.h
  bpy_utils_units.h
  ../BPY_extern.h
  ../BPY_extern_clog.h
  ../BPY_extern_python.h
  ../BPY_extern_run.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  bf_editor_animation
  bf_editor_interface
  bf_editor_space_api
  PRIVATE bf::intern::guardedalloc
  bf_python_gpu

  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
)

# only to check if buildinfo is available
if(WITH_BUILDINFO)
  add_definitions(-DBUILD_DATE)
endif()

if(WITH_PYTHON_MODULE)
  add_definitions(-DWITH_PYTHON_MODULE)
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

if(WITH_CODEC_AVI)
  add_definitions(-DWITH_AVI)
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

if(WITH_COMPOSITOR_CPU)
  add_definitions(-DWITH_COMPOSITOR_CPU)
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
  add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_OPENJPEG)
  add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_WEBP)
  add_definitions(-DWITH_WEBP)
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
  if(WITH_SDL_DYNLOAD)
    list(APPEND LIB
      extern_sdlew
    )
  else()
    list(APPEND LIB
      ${SDL_LIBRARY}
    )
  endif()
  add_definitions(-DWITH_SDL)
endif()

if(WITH_SDL_DYNLOAD)
  list(APPEND INC
    ../../../../extern/sdlew/include
  )
  add_definitions(-DWITH_SDL_DYNLOAD)
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

if(WITH_OPENCOLLADA)
  add_definitions(-DWITH_COLLADA)
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

if(WITH_IO_GPENCIL)
  add_definitions(-DWITH_IO_GPENCIL)
endif()

if(WITH_ALEMBIC)
  add_definitions(-DWITH_ALEMBIC)
endif()

if(WITH_OPENCOLORIO)
  add_definitions(-DWITH_OCIO)
endif()

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB)
  list(APPEND INC
    ../../../../intern/openvdb
  )
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

blender_add_lib(bf_python "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_python bf_rna)
