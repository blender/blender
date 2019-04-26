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

set(INC
  ..
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../editors/include
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/guardedalloc
  ../../../../intern/opencolorio
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
  bpy_capi_utils.c
  bpy_driver.c
  bpy_gizmo_wrap.c
  bpy_interface.c
  bpy_interface_atexit.c
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
  bpy_rna_driver.c
  bpy_rna_gizmo.c
  bpy_rna_id_collection.c
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
  bpy_rna_driver.h
  bpy_rna_gizmo.h
  bpy_rna_id_collection.h
  bpy_traceback.h
  bpy_utils_previews.h
  bpy_utils_units.h
  ../BPY_extern.h
  ../BPY_extern_clog.h
)

set(LIB
  bf_editor_animation
  bf_editor_interface
  bf_editor_space_api
  bf_python_gpu
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
  add_definitions(-DWITH_AUDASPACE)
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
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_CODEC_SNDFILE)
  add_definitions(-DWITH_SNDFILE)
endif()

if(WITH_COMPOSITOR)
  add_definitions(-DWITH_COMPOSITOR)
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

if(WITH_IMAGE_DDS)
  add_definitions(-DWITH_DDS)
endif()

if(WITH_IMAGE_HDR)
  add_definitions(-DWITH_HDR)
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_OPENIMAGEIO)
  add_definitions(-DWITH_OPENIMAGEIO)
endif()

if(WITH_IMAGE_OPENJPEG)
  add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_TIFF)
  add_definitions(-DWITH_TIFF)
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

if(WITH_SDL)
  list(APPEND INC_SYS
    ${SDL_INCLUDE_DIR}
  )
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

if(WITH_LIBMV)
  add_definitions(-DWITH_LIBMV)
endif()

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_MOD_FLUID)
endif()

if(WITH_MOD_OCEANSIM)
  add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_MOD_REMESH)
  add_definitions(-DWITH_MOD_REMESH)
endif()

if(WITH_MOD_SMOKE)
  add_definitions(-DWITH_SMOKE)
endif()

if(WITH_OPENCOLLADA)
  add_definitions(-DWITH_COLLADA)
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
    ../../alembic
  )
endif()

if(WITH_OPENIMAGEIO)
  add_definitions(-DWITH_OPENIMAGEIO)
  list(APPEND INC
    ../../imbuf/intern/oiio
  )
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
  list(APPEND INC
    ../../../../intern/opensubdiv
  )
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_python "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
