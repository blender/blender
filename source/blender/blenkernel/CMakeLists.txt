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
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../blenfont
	../blenlib
	../blenloader
	../blentranslation
	../depsgraph
	../draw
	../gpu
	../ikplugin
	../imbuf
	../makesdna
	../makesrna
	../bmesh
	../modifiers
	../gpencil_modifiers
	../shader_fx
	../nodes
	../physics
	../render/extern/include
	../../../intern/ghost
	../../../intern/guardedalloc
	../../../intern/glew-mx
	../../../intern/iksolver/extern
	../../../intern/memutil
	../../../intern/mikktspace
	../../../intern/smoke/extern
	../../../intern/atomic
	../../../intern/clog
	../../../intern/libmv
	../../../intern/opensubdiv
	../../../extern/curve_fit_nd
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/CCGSubSurf.c
	intern/CCGSubSurf_legacy.c
	intern/CCGSubSurf_opensubdiv.c
	intern/CCGSubSurf_opensubdiv_converter.c
	intern/CCGSubSurf_util.c
	intern/DerivedMesh.c
	intern/action.c
	intern/addon.c
	intern/anim.c
	intern/anim_sys.c
	intern/appdir.c
	intern/armature.c
	intern/armature_update.c
	intern/autoexec.c
	intern/blender.c
	intern/blender_copybuffer.c
	intern/blender_undo.c
	intern/blender_user_menu.c
	intern/blendfile.c
	intern/bmfont.c
	intern/boids.c
	intern/bpath.c
	intern/brush.c
	intern/bvhutils.c
	intern/cachefile.c
	intern/camera.c
	intern/cdderivedmesh.c
	intern/cloth.c
	intern/collection.c
	intern/collision.c
	intern/colorband.c
	intern/colortools.c
	intern/constraint.c
	intern/context.c
	intern/crazyspace.c
	intern/curve.c
	intern/curve_decimate.c
	intern/customdata.c
	intern/customdata_file.c
	intern/data_transfer.c
	intern/deform.c
	intern/displist.c
	intern/dynamicpaint.c
	intern/editderivedmesh.c
	intern/editlattice.c
	intern/editmesh.c
	intern/editmesh_bvh.c
	intern/editmesh_tangent.c
	intern/effect.c
	intern/fcurve.c
	intern/fluidsim.c
	intern/fmodifier.c
	intern/font.c
	intern/freestyle.c
	intern/gpencil.c
	intern/gpencil_modifier.c
	intern/icons.c
	intern/icons_rasterize.c
	intern/idcode.c
	intern/idprop.c
	intern/idprop_utils.c
	intern/image.c
	intern/image_gen.c
	intern/ipo.c
	intern/key.c
	intern/lamp.c
	intern/lattice.c
	intern/library.c
	intern/library_idmap.c
	intern/library_override.c
	intern/library_query.c
	intern/library_remap.c
	intern/linestyle.c
	intern/mask.c
	intern/mask_evaluate.c
	intern/mask_rasterize.c
	intern/material.c
	intern/mball.c
	intern/mball_tessellate.c
	intern/mesh.c
	intern/mesh_convert.c
	intern/mesh_evaluate.c
	intern/mesh_iterators.c
	intern/mesh_mapping.c
	intern/mesh_merge.c
	intern/mesh_remap.c
	intern/mesh_runtime.c
	intern/mesh_tangent.c
	intern/mesh_validate.c
	intern/modifier.c
	intern/movieclip.c
	intern/multires.c
	intern/nla.c
	intern/node.c
	intern/object.c
	intern/object_facemap.c
	intern/object_deform.c
	intern/object_dupli.c
	intern/object_update.c
	intern/ocean.c
	intern/outliner_treehash.c
	intern/packedFile.c
	intern/paint.c
	intern/particle.c
	intern/particle_child.c
	intern/particle_distribute.c
	intern/particle_system.c
	intern/pbvh.c
	intern/pbvh_bmesh.c
	intern/pointcache.c
	intern/layer.c
	intern/layer_utils.c
	intern/lightprobe.c
	intern/report.c
	intern/rigidbody.c
	intern/scene.c
	intern/screen.c
	intern/seqcache.c
	intern/seqeffects.c
	intern/seqmodifier.c
	intern/sequencer.c
	intern/shader_fx.c
	intern/shrinkwrap.c
	intern/smoke.c
	intern/softbody.c
	intern/sound.c
	intern/speaker.c
	intern/studiolight.c
	intern/subdiv.c
	intern/subdiv_converter.c
	intern/subdiv_converter_mesh.c
	intern/subdiv_displacement.c
	intern/subdiv_displacement_multires.c
	intern/subdiv_eval.c
	intern/subdiv_foreach.c
	intern/subdiv_mesh.c
	intern/subdiv_stats.c
	intern/subsurf_ccg.c
	intern/suggestions.c
	intern/text.c
	intern/texture.c
	intern/tracking.c
	intern/tracking_auto.c
	intern/tracking_detect.c
	intern/tracking_plane_tracker.c
	intern/tracking_region_tracker.c
	intern/tracking_solver.c
	intern/tracking_stabilize.c
	intern/tracking_util.c
	intern/undo_system.c
	intern/unit.c
	intern/workspace.c
	intern/world.c
	intern/writeavi.c

	BKE_DerivedMesh.h
	BKE_action.h
	BKE_addon.h
	BKE_anim.h
	BKE_animsys.h
	BKE_appdir.h
	BKE_armature.h
	BKE_autoexec.h
	BKE_blender.h
	BKE_blender_copybuffer.h
	BKE_blender_undo.h
	BKE_blender_version.h
	BKE_blendfile.h
	BKE_bmfont.h
	BKE_bmfont_types.h
	BKE_boids.h
	BKE_bpath.h
	BKE_brush.h
	BKE_bvhutils.h
	BKE_cachefile.h
	BKE_camera.h
	BKE_ccg.h
	BKE_cdderivedmesh.h
	BKE_cloth.h
	BKE_collection.h
	BKE_collision.h
	BKE_colorband.h
	BKE_colortools.h
	BKE_constraint.h
	BKE_context.h
	BKE_crazyspace.h
	BKE_curve.h
	BKE_customdata.h
	BKE_customdata_file.h
	BKE_data_transfer.h
	BKE_deform.h
	BKE_displist.h
	BKE_dynamicpaint.h
	BKE_editlattice.h
	BKE_editmesh.h
	BKE_editmesh_bvh.h
	BKE_editmesh_tangent.h
	BKE_effect.h
	BKE_fcurve.h
	BKE_fluidsim.h
	BKE_font.h
	BKE_freestyle.h
	BKE_global.h
	BKE_gpencil.h
	BKE_gpencil_modifier.h
	BKE_icons.h
	BKE_idcode.h
	BKE_idprop.h
	BKE_image.h
	BKE_ipo.h
	BKE_key.h
	BKE_lamp.h
	BKE_lattice.h
	BKE_library.h
	BKE_library_idmap.h
	BKE_library_override.h
	BKE_library_query.h
	BKE_library_remap.h
	BKE_linestyle.h
	BKE_main.h
	BKE_mask.h
	BKE_material.h
	BKE_mball.h
	BKE_mball_tessellate.h
	BKE_mesh.h
	BKE_mesh_iterators.h
	BKE_mesh_mapping.h
	BKE_mesh_remap.h
	BKE_mesh_runtime.h
	BKE_mesh_tangent.h
	BKE_modifier.h
	BKE_movieclip.h
	BKE_multires.h
	BKE_nla.h
	BKE_node.h
	BKE_object.h
	BKE_object_facemap.h
	BKE_object_deform.h
	BKE_ocean.h
	BKE_outliner_treehash.h
	BKE_packedFile.h
	BKE_paint.h
	BKE_particle.h
	BKE_pbvh.h
	BKE_pointcache.h
	BKE_layer.h
	BKE_lightprobe.h
	BKE_report.h
	BKE_rigidbody.h
	BKE_scene.h
	BKE_screen.h
	BKE_sequencer.h
	BKE_shader_fx.h
	BKE_shrinkwrap.h
	BKE_smoke.h
	BKE_softbody.h
	BKE_sound.h
	BKE_speaker.h
	BKE_studiolight.h
	BKE_subdiv.h
	BKE_subsurf.h
	BKE_suggestions.h
	BKE_text.h
	BKE_texture.h
	BKE_tracking.h
	BKE_undo_system.h
	BKE_unit.h
	BKE_workspace.h
	BKE_world.h
	BKE_writeavi.h

	nla_private.h
	tracking_private.h
	particle_private.h
	intern/CCGSubSurf.h
	intern/CCGSubSurf_inline.h
	intern/CCGSubSurf_intern.h
	intern/data_transfer_intern.h
	intern/pbvh_intern.h
	intern/subdiv_converter.h
)

if(WITH_BINRELOC)
	list(APPEND INC_SYS
		${BINRELOC_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_BINRELOC)
endif()

add_definitions(${GL_DEFINITIONS})

if(WIN32)
	list(APPEND INC
		../../../intern/utfconv
	)
endif()

if(WITH_AUDASPACE)
	add_definitions(-DWITH_AUDASPACE)

	list(APPEND INC_SYS
		${AUDASPACE_C_INCLUDE_DIRS}
	)
endif()

if(WITH_BULLET)
	list(APPEND INC_SYS
		${BULLET_INCLUDE_DIRS}
	)
	list(APPEND INC
		../../../intern/rigidbody
	)
	add_definitions(-DWITH_BULLET)
endif()

#if(WITH_MOD_CLOTH_ELTOPO)
#	list(APPEND INC
#		../../../extern/eltopo
#		../../../extern/eltopo/eltopo3d
#	)
#	add_definitions(-DWITH_ELTOPO)
#endif()

if(WITH_IMAGE_OPENEXR)
	add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_TIFF)
	add_definitions(-DWITH_TIFF)
endif()

if(WITH_OPENIMAGEIO)
	add_definitions(-DWITH_OPENIMAGEIO)
endif()

if(WITH_IMAGE_OPENJPEG)
	add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_DDS)
	add_definitions(-DWITH_DDS)
endif()

if(WITH_IMAGE_CINEON)
	add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_FRAMESERVER)
	add_definitions(-DWITH_FRAMESERVER)
endif()

if(WITH_IMAGE_HDR)
	add_definitions(-DWITH_HDR)
endif()

if(WITH_CODEC_AVI)
	list(APPEND INC
		../avi
	)
	add_definitions(-DWITH_AVI)
endif()

if(WITH_CODEC_FFMPEG)
	list(APPEND SRC
		intern/writeffmpeg.c
		BKE_writeffmpeg.h
	)
	list(APPEND INC
		../../../intern/ffmpeg
	)
	list(APPEND INC_SYS
		${FFMPEG_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_FFMPEG)

	remove_strict_c_flags_file(
		intern/writeffmpeg.c
	)
endif()

if(WITH_PYTHON)
	list(APPEND INC
		../python
	)
	add_definitions(-DWITH_PYTHON)

	if(WITH_PYTHON_SAFETY)
		add_definitions(-DWITH_PYTHON_SAFETY)
	endif()

	if(WITH_PYTHON_SECURITY)
		add_definitions(-DWITH_PYTHON_SECURITY)
	endif()


	if(PYTHON_EXECUTABLE)
		get_filename_component(_python_exe_name ${PYTHON_EXECUTABLE} NAME)
		add_definitions(-DPYTHON_EXECUTABLE_NAME=${_python_exe_name})
		unset(_python_exe_name)
	endif()
endif()

if(WITH_MOD_FLUID)
	list(APPEND INC
		../../../intern/elbeem/extern
	)
	add_definitions(-DWITH_MOD_FLUID)
endif()

if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

if(WITH_MOD_OCEANSIM)
	add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_JACK)
	add_definitions(-DWITH_JACK)
endif()

if(WITH_LZO)
	if(WITH_SYSTEM_LZO)
		list(APPEND INC_SYS
			${LZO_INCLUDE_DIR}
		)
		add_definitions(-DWITH_SYSTEM_LZO)
	else()
		list(APPEND INC_SYS
			../../../extern/lzo/minilzo
		)
	endif()
	add_definitions(-DWITH_LZO)
endif()

if(WITH_LZMA)
	list(APPEND INC_SYS
		../../../extern/lzma
	)
	add_definitions(-DWITH_LZMA)
endif()

if(WITH_LIBMV)
	add_definitions(-DWITH_LIBMV)
endif()

if(WITH_FFTW3)
	list(APPEND INC_SYS
		${FFTW3_INCLUDE_DIRS}
	)
	add_definitions(-DFFTW3=1)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_FREESTYLE)
	add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_ALEMBIC)
	list(APPEND INC
		../alembic
	)
	add_definitions(-DWITH_ALEMBIC)
endif()

if(WITH_OPENSUBDIV)
	add_definitions(-DWITH_OPENSUBDIV)
	list(APPEND INC_SYS
		${OPENSUBDIV_INCLUDE_DIRS}
	)
endif()

if(WITH_OPENVDB)
	add_definitions(-DWITH_OPENVDB)
	list(APPEND INC
		 ../../../intern/openvdb
	)

	if(WITH_OPENVDB_BLOSC)
		add_definitions(
			-DWITH_OPENVDB_BLOSC
		)
	endif()
endif()

## Warnings as errors, this is too strict!
#if(MSVC)
#	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
#endif()

blender_add_lib(bf_blenkernel "${SRC}" "${INC}" "${INC_SYS}")
